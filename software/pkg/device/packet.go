package device

import "uart-servo-controller/pkg/serial"

// The packet format and CRC8 are implemented once in pkg/serial.
// These aliases keep the device package API without duplicating the logic.

// Packet represents a protocol packet
type Packet = serial.Packet

// CRC8 calculates CRC8 checksum (poly 0x07)
func CRC8(data []uint8) uint8 { return serial.CRC8(data) }

// NewPacket creates a new packet
func NewPacket(target, cmd uint8, data []uint8) *Packet {
	return serial.NewPacket(target, cmd, data)
}

// Unmarshal deserializes packet from bytes
func Unmarshal(data []uint8) (*Packet, error) { return serial.Unmarshal(data) }
