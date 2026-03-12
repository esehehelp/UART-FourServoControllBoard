package test

import (
"testing"
"uart-servo-controller/pkg/serial"
)

// TestCRC8 tests CRC calculation
func TestCRC8(t *testing.T) {
tests := []struct {
name string
data []uint8
want uint8
}{
{
name: "empty",
data: []uint8{},
want: 0,
},
}

for _, tt := range tests {
t.Run(tt.name, func(t *testing.T) {
got := serial.CRC8(tt.data)
if got != tt.want {
t.Errorf("CRC8() = 0x%02x, want 0x%02x", got, tt.want)
}
})
}
}

// TestPacketMarshal tests packet serialization
func TestPacketMarshal(t *testing.T) {
pkt := serial.NewPacket(0x01, 0x05, []uint8{0x80})
bytes := pkt.Marshal()

if len(bytes) < 6 {
t.Errorf("Marshal() length = %d, want >= 6", len(bytes))
}

if bytes[0] != 0xAA {
t.Errorf("Header = 0x%02x, want 0xAA", bytes[0])
}

if bytes[1] != 0x01 {
t.Errorf("Target = 0x%02x, want 0x01", bytes[1])
}

if bytes[3] != 0x05 {
t.Errorf("Cmd = 0x%02x, want 0x05", bytes[3])
}
}

// TestPacketUnmarshal tests packet deserialization
func TestPacketUnmarshal(t *testing.T) {
// Create a valid packet
original := serial.NewPacket(0x01, 0x05, []uint8{0x80})
bytes := original.Marshal()

// Unmarshal it
pkt, err := serial.Unmarshal(bytes)
if err != nil {
t.Fatalf("Unmarshal() error = %v", err)
}

if pkt.Target != 0x01 {
t.Errorf("Target = 0x%02x, want 0x01", pkt.Target)
}

if pkt.Cmd != 0x05 {
t.Errorf("Cmd = 0x%02x, want 0x05", pkt.Cmd)
}

if len(pkt.Data) != 1 || pkt.Data[0] != 0x80 {
t.Errorf("Data = %v, want [0x80]", pkt.Data)
}
}

// TestCRCMismatch tests CRC validation
func TestCRCMismatch(t *testing.T) {
original := serial.NewPacket(0x01, 0x05, []uint8{0x80})
bytes := original.Marshal()

// Corrupt the data
bytes[len(bytes)-1] ^= 0xFF // Flip CRC

pkt, err := serial.Unmarshal(bytes)
if err == nil {
t.Errorf("Unmarshal() with corrupted CRC should error, got %v", pkt)
}
}

// TestPacketRoundtrip tests multiple packet marshaling/unmarshaling
func TestPacketRoundtrip(t *testing.T) {
tests := []struct {
name string
cmd  uint8
data []uint8
}{
{
name: "servo_control",
cmd:  0x01,
data: []uint8{0, 0x05, 0xDC}, // CH0, 1500us
},
{
name: "led_control",
cmd:  0x05,
data: []uint8{128},
},
{
name: "pd_voltage",
cmd:  0x06,
data: []uint8{0x13, 0x88}, // 5000mV
},
}

for _, tt := range tests {
t.Run(tt.name, func(t *testing.T) {
pkt1 := serial.NewPacket(0x01, tt.cmd, tt.data)
bytes := pkt1.Marshal()

pkt2, err := serial.Unmarshal(bytes)
if err != nil {
t.Fatalf("Unmarshal() error = %v", err)
}

if pkt2.Cmd != tt.cmd {
t.Errorf("Cmd = 0x%02x, want 0x%02x", pkt2.Cmd, tt.cmd)
}

if len(pkt2.Data) != len(tt.data) {
t.Errorf("Data length = %d, want %d", len(pkt2.Data), len(tt.data))
}

for i, b := range tt.data {
if pkt2.Data[i] != b {
t.Errorf("Data[%d] = 0x%02x, want 0x%02x", i, pkt2.Data[i], b)
}
}
})
}
}
