package serial

import (
	"fmt"
	"io"
	"log"
	"sort"
	"strings"
	"time"

	goserial "go.bug.st/serial"
	"uart-servo-controller/config"
)

// Packet represents a protocol packet
type Packet struct {
	Header uint8
	Target uint8
	Source uint8
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
		Cmd:    cmd,
		Data:   make([]uint8, len(data)),
	}
	copy(pkt.Data, data)
	return pkt
}

// Marshal serializes packet to bytes with CRC
func (p *Packet) Marshal() []uint8 {
	buf := make([]uint8, 0, 6+len(p.Data))
	buf = append(buf, p.Header, p.Target, p.Source, p.Cmd, uint8(len(p.Data)))
	buf = append(buf, p.Data...)
	crc := CRC8(buf)
	buf = append(buf, crc)
	return buf
}

// Unmarshal deserializes packet from bytes
func Unmarshal(data []uint8) (*Packet, error) {
	if len(data) < 6 {
		return nil, fmt.Errorf("packet too short: %d bytes", len(data))
	}

	if data[0] != config.PKT_HEADER {
		return nil, fmt.Errorf("invalid header: 0x%02x", data[0])
	}

	dataLen := int(data[4])
	expectedLen := 6 + dataLen

	if len(data) < expectedLen {
		return nil, fmt.Errorf("incomplete packet: expected %d bytes, got %d", expectedLen, len(data))
	}

	payloadLen := 5 + dataLen
	crcCalc := CRC8(data[:payloadLen])
	crcRecv := data[payloadLen]

	if crcCalc != crcRecv {
		return nil, fmt.Errorf("CRC mismatch: expected 0x%02x, got 0x%02x", crcCalc, crcRecv)
	}

	pkt := &Packet{
		Header: data[0],
		Target: data[1],
		Source: data[2],
		Cmd:    data[3],
		Data:   make([]uint8, dataLen),
		CRC:    crcRecv,
	}

	copy(pkt.Data, data[5:5+dataLen])

	return pkt, nil
}

// String returns a human-readable representation
func (p *Packet) String() string {
	return fmt.Sprintf("Pkt{Tgt:0x%02x, Cmd:0x%02x, Len:%d, CRC:0x%02x}", p.Target, p.Cmd, len(p.Data), p.CRC)
}

// Manager handles serial communication
type Manager struct {
	port     io.ReadWriteCloser
	rxChan   chan *Packet
	errChan  chan error
	done     chan struct{}
	running  bool
	lastErr  error
	onStatus func(string, string) // (message, color)
}

// NewManager creates a new serial manager
func NewManager(onStatus func(string, string)) *Manager {
	return &Manager{
		rxChan:   make(chan *Packet, 16),
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
	if m.port != nil {
		m.port.Close()
	}
}

// RxChan returns the receive channel
func (m *Manager) RxChan() <-chan *Packet {
	return m.rxChan
}

// ErrChan returns the error channel
func (m *Manager) ErrChan() <-chan error {
	return m.errChan
}

// Send transmits a packet
func (m *Manager) Send(pkt *Packet) error {
	if m.port == nil {
		return fmt.Errorf("serial port not connected")
	}

	data := pkt.Marshal()
	_, err := m.port.Write(data)
	return err
}

// workerLoop continuously tries to connect and read packets
func (m *Manager) workerLoop() {
	defer func() {
		if m.port != nil {
			m.port.Close()
		}
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
		if m.port == nil {
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
		n, err := m.port.Read(tempBuf)

		if err != nil {
			log.Printf("Read error: %v", err)
			m.port.Close()
			m.port = nil
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

		// Need at least 6 bytes to read length field
		if len(*buffer) < 6 {
			return
		}

		// Extract packet length
		dataLen := int((*buffer)[4])
		expectedLen := 6 + dataLen

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
			log.Printf("RxChan full, dropping packet")
		}

		// Remove processed packet from buffer
		*buffer = (*buffer)[expectedLen:]
	}
}

// tryConnect probes each available port with CMD_SENSOR_READ and accepts
// the first one that responds with a valid RESP_SENSOR_DATA packet.
func (m *Manager) tryConnect() {
	ports, err := goserial.GetPortsList()
	if err != nil {
		log.Printf("Error listing ports: %v", err)
		m.updateStatus("Scanning...", "orange")
		return
	}

	if len(ports) == 0 {
		m.updateStatus("Scanning...", "orange")
		return
	}

	// Prefer USB/ACM ports; skip ttyS* (kernel-emulated HW UARTs) unless nothing else exists
	sort.SliceStable(ports, func(i, j int) bool {
		return portPriority(ports[i]) > portPriority(ports[j])
	})

	mode := &goserial.Mode{
		BaudRate: config.DEFAULT_BAUD,
		DataBits: 8,
		Parity:   goserial.NoParity,
		StopBits: goserial.OneStopBit,
	}

	m.updateStatus("Scanning...", "orange")

	for _, portName := range ports {
		if portName, port := m.probePort(portName, mode); port != nil {
			m.port = port
			log.Printf("Connected to %s", portName)
			m.updateStatus(fmt.Sprintf("Connected: %s", portName), "green")
			return
		}
	}
}

// portPriority returns a sort key: higher = try first.
// ttyUSB/ttyACM (USB-serial) are preferred over ttyS (on-board UART).
func portPriority(name string) int {
	switch {
	case strings.Contains(name, "ttyUSB"):
		return 3
	case strings.Contains(name, "ttyACM"):
		return 2
	case strings.Contains(name, "ttyS"):
		return 0 // last resort
	default:
		return 1
	}
}

// probePort opens portName, sends CMD_SENSOR_READ, and waits up to 300 ms
// for a RESP_SENSOR_DATA reply. Returns the open port on success, nil on failure.
func (m *Manager) probePort(portName string, mode *goserial.Mode) (string, goserial.Port) {
	port, err := goserial.Open(portName, mode)
	if err != nil {
		return portName, nil
	}

	// Send probe packet
	probe := NewPacket(config.DEVICE_ID, config.CMD_SENSOR_READ, []uint8{config.SENSOR_TYPE_ALL})
	if _, err := port.Write(probe.Marshal()); err != nil {
		port.Close()
		return portName, nil
	}

	// Read with 50 ms per-call timeout; total budget 300 ms
	if err := port.SetReadTimeout(50 * time.Millisecond); err != nil {
		port.Close()
		return portName, nil
	}

	buf := make([]uint8, 64)
	rxBuf := make([]uint8, 0, 64)
	deadline := time.Now().Add(300 * time.Millisecond)

	for time.Now().Before(deadline) {
		n, _ := port.Read(buf)
		if n > 0 {
			rxBuf = append(rxBuf, buf[:n]...)
			if pkt := findPacket(rxBuf); pkt != nil && pkt.Cmd == config.RESP_SENSOR_DATA {
				// Disable per-call read timeout for normal operation
				_ = port.SetReadTimeout(0)
				return portName, port
			}
		}
	}

	port.Close()
	return portName, nil
}

// findPacket scans buf for a complete, CRC-valid packet and returns it.
func findPacket(buf []uint8) *Packet {
	for i := 0; i < len(buf); i++ {
		if buf[i] != config.PKT_HEADER {
			continue
		}
		if len(buf)-i < 6 {
			break
		}
		dataLen := int(buf[i+4])
		end := i + 6 + dataLen
		if len(buf) < end {
			break
		}
		pkt, err := Unmarshal(buf[i:end])
		if err == nil {
			return pkt
		}
	}
	return nil
}

// updateStatus calls the status callback
func (m *Manager) updateStatus(msg, color string) {
	if m.onStatus != nil {
		m.onStatus(msg, color)
	}
}
