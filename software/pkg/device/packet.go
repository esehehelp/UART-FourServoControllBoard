package device

import (
	"fmt"
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
	// Build packet: [header, target, source, cmd, len, ...data]
	buf := make([]uint8, 0, 6+len(p.Data))
	buf = append(buf, p.Header, p.Target, p.Source, p.Cmd, uint8(len(p.Data)))
	buf = append(buf, p.Data...)

	// Calculate and append CRC
	crc := CRC8(buf)
	buf = append(buf, crc)

	return buf
}

// Unmarshal deserializes packet from bytes
func Unmarshal(data []uint8) (*Packet, error) {
	if len(data) < 6 {
		return nil, fmt.Errorf("packet too short: %d bytes", len(data))
	}

	// Verify header
	if data[0] != config.PKT_HEADER {
		return nil, fmt.Errorf("invalid header: 0x%02x", data[0])
	}

	// Extract length field
	dataLen := int(data[4])
	expectedLen := 6 + dataLen // header + target + source + cmd + len + data + crc

	if len(data) < expectedLen {
		return nil, fmt.Errorf("incomplete packet: expected %d bytes, got %d", expectedLen, len(data))
	}

	// Verify CRC
	payloadLen := 5 + dataLen // target + source + cmd + len + data
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
