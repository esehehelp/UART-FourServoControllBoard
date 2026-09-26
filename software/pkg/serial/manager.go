package serial

import (
	"fmt"
	"io"
	"log"
	"sort"
	"strings"
	"sync"
	"sync/atomic"
	"time"

	goserial "go.bug.st/serial"
	"go.bug.st/serial/enumerator"
	"uart-servo-controller/config"
)

// Packet represents a protocol packet
// Wire format: [0xAA | Target | Source | TTL | Cmd | Len | Data... | CRC8]
type Packet struct {
	Header uint8
	Target uint8
	Source uint8
	TTL    uint8 // hop limit, decremented by each ring node (#13)
	Cmd    uint8
	Data   []uint8
	CRC    uint8
}

// CRC8 calculates CRC8 checksum
func CRC8(data []uint8) uint8 {
	crc := uint8(0)
	for _, b := range data {
		crc ^= b
		for i := 0; i < 8; i++ {
			if crc&0x80 != 0 {
				crc = (crc << 1) ^ 0x07
			} else {
				crc <<= 1
			}
			crc &= 0xFF
		}
	}
	return crc
}

// NewPacket creates a new packet
func NewPacket(target, cmd uint8, data []uint8) *Packet {
	pkt := &Packet{
		Header: config.PKT_HEADER,
		Target: target,
		Source: config.HOST_ID,
		TTL:    config.DEFAULT_TTL,
		Cmd:    cmd,
		Data:   make([]uint8, len(data)),
	}
	copy(pkt.Data, data)
	return pkt
}

// Marshal serializes packet to bytes with CRC
func (p *Packet) Marshal() []uint8 {
	buf := make([]uint8, 0, config.PKT_OVERHEAD+len(p.Data))
	buf = append(buf, p.Header, p.Target, p.Source, p.TTL, p.Cmd, uint8(len(p.Data)))
	buf = append(buf, p.Data...)
	crc := CRC8(buf)
	buf = append(buf, crc)
	return buf
}

// Unmarshal deserializes packet from bytes
func Unmarshal(data []uint8) (*Packet, error) {
	if len(data) < config.PKT_OVERHEAD {
		return nil, fmt.Errorf("packet too short: %d bytes", len(data))
	}

	if data[0] != config.PKT_HEADER {
		return nil, fmt.Errorf("invalid header: 0x%02x", data[0])
	}

	dataLen := int(data[5])
	expectedLen := config.PKT_OVERHEAD + dataLen

	if len(data) < expectedLen {
		return nil, fmt.Errorf("incomplete packet: expected %d bytes, got %d", expectedLen, len(data))
	}

	payloadLen := config.PKT_OVERHEAD - 1 + dataLen
	crcCalc := CRC8(data[:payloadLen])
	crcRecv := data[payloadLen]

	if crcCalc != crcRecv {
		return nil, fmt.Errorf("CRC mismatch: expected 0x%02x, got 0x%02x", crcCalc, crcRecv)
	}

	pkt := &Packet{
		Header: data[0],
		Target: data[1],
		Source: data[2],
		TTL:    data[3],
		Cmd:    data[4],
		Data:   make([]uint8, dataLen),
		CRC:    crcRecv,
	}

	copy(pkt.Data, data[6:6+dataLen])

	return pkt, nil
}

// String returns a human-readable representation
func (p *Packet) String() string {
	return fmt.Sprintf("Pkt{Tgt:0x%02x, Src:0x%02x, TTL:%d, Cmd:0x%02x, Len:%d, CRC:0x%02x}", p.Target, p.Source, p.TTL, p.Cmd, len(p.Data), p.CRC)
}

// DeviceInfo describes a board found on a serial port (#29)
type DeviceInfo struct {
	Port      string `json:"port"`
	ID        uint8  `json:"id"`        // device ID the board answered with
	Name      string `json:"name"`      // user label (empty if unset or old firmware)
	FWVersion string `json:"fwVersion"` // e.g. "0.8.1" (empty for old firmware)
}

// Label is the text shown in the UI: "Name [ID:0x01]" or "Device-01 [ID:0x01]"
func (d DeviceInfo) Label() string {
	name := d.Name
	if name == "" {
		name = fmt.Sprintf("Device-%02X", d.ID)
	}
	return fmt.Sprintf("%s [ID:0x%02X]", name, d.ID)
}

// Manager handles serial communication
type Manager struct {
	mu       sync.Mutex // guards port, info, selected
	port     io.ReadWriteCloser
	info     DeviceInfo // board on port
	selected string     // port chosen by the user; "" = auto (first board found)
	probeMu  sync.Mutex // one probe/scan at a time
	rxChan   chan *Packet
	errChan  chan error
	done     chan struct{}
	running  bool
	lastErr  error
	onStatus func(string, string) // (message, color)
	dropped  atomic.Uint64        // packets dropped because rxChan was full
}

// rxChanSize is the receive queue depth. Sensor responses arrive every
// UPDATE_INTERVAL_MS, so this covers several seconds of a stalled consumer.
const rxChanSize = 128

// NewManager creates a new serial manager
func NewManager(onStatus func(string, string)) *Manager {
	return &Manager{
		rxChan:   make(chan *Packet, rxChanSize),
		errChan:  make(chan error, 4),
		done:     make(chan struct{}),
		onStatus: onStatus,
	}
}

// Start begins the manager loop
func (m *Manager) Start() {
	go m.workerLoop()
}

// Stop gracefully stops the manager
func (m *Manager) Stop() {
	close(m.done)
	m.mu.Lock()
	if m.port != nil {
		m.port.Close()
	}
	m.mu.Unlock()
}

// Connected returns the board currently connected, if any.
func (m *Manager) Connected() (DeviceInfo, bool) {
	m.mu.Lock()
	defer m.mu.Unlock()
	return m.info, m.port != nil
}

// SetConnectedName updates the cached name after it was changed on the board.
func (m *Manager) SetConnectedName(name string) {
	m.mu.Lock()
	m.info.Name = name
	m.mu.Unlock()
}

// SelectPort makes the manager use only portName ("" = auto-select the first
// board found). A connection to a different port is closed; the worker loop
// then connects to the selected port.
func (m *Manager) SelectPort(portName string) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.selected = portName
	if m.port != nil && portName != "" && m.info.Port != portName {
		m.port.Close() // the worker sees the read error and reconnects
		m.port = nil
	}
}

// Selected returns the port chosen with SelectPort ("" = auto).
func (m *Manager) Selected() string {
	m.mu.Lock()
	defer m.mu.Unlock()
	return m.selected
}

// ScanDevices lists every board that answers on any serial port (#29). The
// connected board is reported from the current connection, other ports are
// probed briefly and closed again.
func (m *Manager) ScanDevices() ([]DeviceInfo, error) {
	ports, err := listPorts()
	if err != nil {
		return nil, err
	}
	sort.SliceStable(ports, func(i, j int) bool {
		return portPriority(ports[i]) > portPriority(ports[j])
	})

	m.probeMu.Lock()
	defer m.probeMu.Unlock()

	cur, connected := m.Connected()
	var found []DeviceInfo
	if connected {
		found = append(found, cur)
	}
	for _, p := range ports {
		if connected && p.Name == cur.Port {
			continue
		}
		if port, info := probePort(p.Name, serialMode()); port != nil {
			port.Close()
			found = append(found, info)
		}
	}
	return found, nil
}

// RxChan returns the receive channel
func (m *Manager) RxChan() <-chan *Packet {
	return m.rxChan
}

// DroppedPackets returns how many received packets were discarded because
// the receive channel was full.
func (m *Manager) DroppedPackets() uint64 {
	return m.dropped.Load()
}

// ErrChan returns the error channel
func (m *Manager) ErrChan() <-chan error {
	return m.errChan
}

// Send transmits a packet
func (m *Manager) Send(pkt *Packet) error {
	if len(pkt.Data) > config.MAX_DATA_LEN {
		return fmt.Errorf("packet data too long: %d bytes (max %d)", len(pkt.Data), config.MAX_DATA_LEN)
	}
	m.mu.Lock()
	defer m.mu.Unlock()
	if m.port == nil {
		return fmt.Errorf("serial port not connected")
	}
	_, err := m.port.Write(pkt.Marshal())
	return err
}

// workerLoop continuously tries to connect and read packets
func (m *Manager) workerLoop() {
	defer func() {
		m.mu.Lock()
		if m.port != nil {
			m.port.Close()
		}
		m.mu.Unlock()
		close(m.rxChan)
	}()

	buffer := make([]uint8, 0, 256)
	scanTicker := time.NewTicker(1 * time.Second)
	defer scanTicker.Stop()

	for {
		select {
		case <-m.done:
			m.updateStatus("Stopped", "gray")
			return
		default:
		}

		// Try to connect if not connected
		m.mu.Lock()
		port := m.port
		m.mu.Unlock()
		if port == nil {
			buffer = buffer[:0]
			select {
			case <-scanTicker.C:
				m.tryConnect()
			case <-m.done:
				m.updateStatus("Stopped", "gray")
				return
			}
			continue
		}

		// Try to read data
		tempBuf := make([]uint8, 64)
		n, err := port.Read(tempBuf)

		if err != nil {
			log.Printf("Read error: %v", err)
			port.Close()
			m.mu.Lock()
			if m.port == port { // not already replaced by SelectPort
				m.port = nil
			}
			m.mu.Unlock()
			m.updateStatus("Connection lost", "red")
			continue
		}

		if n > 0 {
			buffer = append(buffer, tempBuf[:n]...)
			m.processBuffer(&buffer)
		} else {
			time.Sleep(10 * time.Millisecond)
		}
	}
}

// processBuffer extracts complete packets from buffer
func (m *Manager) processBuffer(buffer *[]uint8) {
	for len(*buffer) > 0 {
		// Look for header
		headerIdx := -1
		for i := 0; i < len(*buffer); i++ {
			if (*buffer)[i] == config.PKT_HEADER {
				headerIdx = i
				break
			}
		}

		if headerIdx < 0 {
			// No header found, discard buffer
			*buffer = (*buffer)[:0]
			return
		}

		if headerIdx > 0 {
			// Discard bytes before header
			*buffer = (*buffer)[headerIdx:]
		}

		// Need the full framing to read the length field
		if len(*buffer) < config.PKT_OVERHEAD {
			return
		}

		// Extract packet length
		dataLen := int((*buffer)[5])
		expectedLen := config.PKT_OVERHEAD + dataLen

		if len(*buffer) < expectedLen {
			// Incomplete packet, wait for more data
			return
		}

		// Try to unmarshal packet
		pkt, err := Unmarshal((*buffer)[:expectedLen])
		if err != nil {
			log.Printf("Packet error: %v, discarding byte", err)
			*buffer = (*buffer)[1:] // Skip first byte and retry
			continue
		}

		// Valid packet received
		select {
		case m.rxChan <- pkt:
		case <-m.done:
			return
		default:
			n := m.dropped.Add(1)
			log.Printf("RxChan full, dropping packet %s (total dropped: %d)", pkt, n)
		}

		// Remove processed packet from buffer
		*buffer = (*buffer)[expectedLen:]
	}
}

// tryConnect probes each available port with CMD_SENSOR_READ and accepts
// the first one that responds with a valid RESP_SENSOR_DATA packet.
func (m *Manager) tryConnect() {
	ports, err := listPorts()
	if err != nil {
		log.Printf("Error listing ports: %v", err)
		m.updateStatus("Scanning...", "orange")
		return
	}

	if len(ports) == 0 {
		m.updateStatus("Scanning...", "orange")
		return
	}

	// Try the board (WCH VID) first, then other USB ports; ttyS* (kernel-emulated
	// HW UARTs) only as a last resort
	sort.SliceStable(ports, func(i, j int) bool {
		return portPriority(ports[i]) > portPriority(ports[j])
	})

	selected := m.Selected()
	if selected != "" {
		m.updateStatus(fmt.Sprintf("Waiting for %s...", selected), "orange")
	} else {
		m.updateStatus("Scanning...", "orange")
	}

	m.probeMu.Lock()
	defer m.probeMu.Unlock()
	for _, p := range ports {
		if selected != "" && p.Name != selected {
			continue
		}
		if port, info := probePort(p.Name, serialMode()); port != nil {
			m.mu.Lock()
			if m.selected != "" && m.selected != info.Port { // selection changed meanwhile
				m.mu.Unlock()
				port.Close()
				return
			}
			m.port = port
			m.info = info
			m.mu.Unlock()
			log.Printf("Connected to %s (%s)", info.Port, info.Label())
			m.updateStatus(fmt.Sprintf("Connected: %s — %s", info.Port, info.Label()), "green")
			return
		}
	}
}

func serialMode() *goserial.Mode {
	return &goserial.Mode{
		BaudRate: config.DEFAULT_BAUD,
		DataBits: 8,
		Parity:   goserial.NoParity,
		StopBits: goserial.OneStopBit,
	}
}

// listPorts returns the available ports with USB details when the OS
// provides them, falling back to plain port names otherwise.
func listPorts() ([]*enumerator.PortDetails, error) {
	details, err := enumerator.GetDetailedPortsList()
	if err == nil {
		return details, nil
	}
	log.Printf("Detailed port enumeration failed, using port names only: %v", err)

	names, err := goserial.GetPortsList()
	if err != nil {
		return nil, err
	}
	ports := make([]*enumerator.PortDetails, len(names))
	for i, name := range names {
		ports[i] = &enumerator.PortDetails{Name: name}
	}
	return ports, nil
}

// portPriority returns a sort key: higher = try first.
// A USB port with the board's VID (WCH) is tried first, then any other USB
// port, then ports ranked by name: ttyUSB/ttyACM/COM (USB-serial or Windows
// COM ports) are preferred over ttyS (on-board UART).
func portPriority(p *enumerator.PortDetails) int {
	switch {
	case p.IsUSB && strings.EqualFold(p.VID, config.USB_VID_WCH):
		return 5
	case p.IsUSB:
		return 4
	case strings.Contains(p.Name, "ttyUSB"):
		return 3
	case strings.Contains(p.Name, "ttyACM"),
		strings.HasPrefix(strings.ToUpper(p.Name), "COM"):
		return 2
	case strings.Contains(p.Name, "ttyS"):
		return 0 // last resort
	default:
		return 1
	}
}

// probePort opens portName and asks for sensor data with a broadcast target,
// so a board answers whatever its device ID is (broadcasts are executed by
// the USB-connected board and never forwarded into the ring). On success it
// also reads the board's name and firmware version (#29) and returns the
// open port; otherwise nil.
func probePort(portName string, mode *goserial.Mode) (goserial.Port, DeviceInfo) {
	info := DeviceInfo{Port: portName}
	port, err := goserial.Open(portName, mode)
	if err != nil {
		return nil, info
	}
	// Read with 50 ms per-call timeout while probing
	if err := port.SetReadTimeout(50 * time.Millisecond); err != nil {
		port.Close()
		return nil, info
	}

	probe := NewPacket(config.BROADCAST_ID, config.CMD_SENSOR_READ, []uint8{config.SENSOR_TYPE_ALL})
	resp := request(port, probe, 300*time.Millisecond, func(p *Packet) bool { return p.Cmd == config.RESP_SENSOR_DATA })
	if resp == nil {
		port.Close()
		return nil, info
	}
	info.ID = resp.Source

	// Optional details; firmware before 0.8.1 answers with an error
	isCfg := func(tag uint8) func(*Packet) bool {
		return func(p *Packet) bool {
			return (p.Cmd == config.RESP_CFG_DATA && len(p.Data) >= 1 && p.Data[0] == tag) ||
				(p.Cmd == config.RESP_ERROR && len(p.Data) >= 1 && p.Data[0] == config.CMD_CONFIG_READ)
		}
	}
	if r := request(port, NewPacket(info.ID, config.CMD_CONFIG_READ, []uint8{config.CFG_TAG_NAME}), 200*time.Millisecond, isCfg(config.CFG_TAG_NAME)); r != nil && r.Cmd == config.RESP_CFG_DATA {
		info.Name = string(r.Data[1:])
	}
	if r := request(port, NewPacket(info.ID, config.CMD_CONFIG_READ, []uint8{config.CFG_TAG_FW_VERSION}), 200*time.Millisecond, isCfg(config.CFG_TAG_FW_VERSION)); r != nil && r.Cmd == config.RESP_CFG_DATA && len(r.Data) >= 4 {
		info.FWVersion = fmt.Sprintf("%d.%d.%d", r.Data[1], r.Data[2], r.Data[3])
	}

	// Disable per-call read timeout for normal operation
	_ = port.SetReadTimeout(0)
	return port, info
}

// request writes pkt and reads until a packet matching want arrives or the
// timeout expires.
func request(port io.ReadWriter, pkt *Packet, timeout time.Duration, want func(*Packet) bool) *Packet {
	if _, err := port.Write(pkt.Marshal()); err != nil {
		return nil
	}
	buf := make([]uint8, 64)
	rxBuf := make([]uint8, 0, 128)
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		n, _ := port.Read(buf)
		if n == 0 {
			continue
		}
		rxBuf = append(rxBuf, buf[:n]...)
		for {
			pkt, rest := nextPacket(rxBuf)
			rxBuf = rest
			if pkt == nil {
				break
			}
			if want(pkt) {
				return pkt
			}
		}
	}
	return nil
}

// nextPacket removes the first complete, CRC-valid packet from buf.
// Returns nil and the remaining bytes if no complete packet is available.
func nextPacket(buf []uint8) (*Packet, []uint8) {
	for len(buf) > 0 {
		if buf[0] != config.PKT_HEADER {
			buf = buf[1:]
			continue
		}
		if len(buf) < config.PKT_OVERHEAD {
			return nil, buf
		}
		end := config.PKT_OVERHEAD + int(buf[5])
		if len(buf) < end {
			return nil, buf
		}
		if pkt, err := Unmarshal(buf[:end]); err == nil {
			return pkt, buf[end:]
		}
		buf = buf[1:]
	}
	return nil, buf
}

// updateStatus calls the status callback
func (m *Manager) updateStatus(msg, color string) {
	if m.onStatus != nil {
		m.onStatus(msg, color)
	}
}
