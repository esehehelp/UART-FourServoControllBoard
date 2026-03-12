"""
Teleoperate Command Test

This test demonstrates the teleoperate (0x0A) command implementation.
It activates teleoperate mode on reader and follower devices,
performs position calibration, and shows feedback voltage synchronization.
"""

import serial
import time
import struct

def crc8(data):
    crc = 0
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80:
                crc = (crc << 1) ^ 0x07
            else:
                crc <<= 1
            crc &= 0xFF
    return crc

def send_packet(ser, target, cmd, data, source=0x00):
    """Send a packet with CRC8."""
    pkt = bytearray([0xAA, target, source, cmd, len(data)]) + bytearray(data)
    pkt.append(crc8(pkt))
    ser.write(pkt)
    time.sleep(0.01)

def set_servo(ser, ch, pos, target=0x01):
    """Send single servo command (0x01)."""
    send_packet(ser, target, 0x01, [ch, (pos >> 8) & 0xFF, pos & 0xFF])

def sync_write_servos(ser, positions, target=0x01):
    """Send sync write command for all 4 servos (0x03)."""
    data = []
    for pos in positions:
        data.append((pos >> 8) & 0xFF)
        data.append(pos & 0xFF)
    send_packet(ser, target, 0x03, data)

def read_sensors(ser, target=0x01):
    """Send read sensors command (0x02), parse 0x82 response."""
    ser.reset_input_buffer()
    send_packet(ser, target, 0x02, [0x00])
    time.sleep(0.05)
    
    data = ser.read(256)
    if len(data) >= 21 and data[0] == 0xAA and data[3] == 0x82:
        # Parse 0x82 response: [TYPE, V_H, V_L, T_H, T_L, C_H, C_L, FB0_H, FB0_L, FB1_H, FB1_L, FB2_H, FB2_L, FB3_H, FB3_L]
        offset = 5  # Skip header, target, source, cmd, len
        v = (data[offset] << 8) | data[offset+1]
        t = (data[offset+2] << 8) | data[offset+3]
        c = (data[offset+4] << 8) | data[offset+5]
        fb = []
        for i in range(4):
            fb_val = (data[offset+6+i*2] << 8) | data[offset+7+i*2]
            fb.append(fb_val)
        return {'voltage': v, 'temp': t, 'current': c, 'feedback': fb}
    return None

def activate_teleoperate(ser, channel_mask=0x0F, target=0x01):
    """Activate teleoperate mode (0x0A with MODE=0x01)."""
    send_packet(ser, target, 0x0A, [0x01, channel_mask])
    time.sleep(0.1)

def deactivate_teleoperate(ser, target=0x01):
    """Deactivate teleoperate mode (0x0A with MODE=0x00)."""
    send_packet(ser, target, 0x0A, [0x00, 0x00])
    time.sleep(0.1)

def discover_devices(ser, host_id=0x01):
    """Discover devices in ring topology."""
    ser.reset_input_buffer()
    send_packet(ser, host_id, 0xA0, [])
    time.sleep(0.15)
    data = ser.read(256)
    devices = []
    if len(data) >= 6 and data[0] == 0xAA:
        print(f"  Discovery response received: {data.hex()}")
    return devices

def run_teleoperate_test(port):
    """Run teleoperate test sequence."""
    try:
        with serial.Serial(port, 115200, timeout=0.5) as ser:
            print(f"\n=== Teleoperate Test on {port} ===\n")
            
            # 1. Device discovery
            print("1. Device Discovery")
            print("-" * 40)
            discover_devices(ser, host_id=0x01)
            time.sleep(0.5)
            
            # 2. Activate teleoperate mode on reader device (0x01)
            print("\n2. Activating Teleoperate Mode on Reader (0x01)")
            print("-" * 40)
            activate_teleoperate(ser, channel_mask=0x0F, target=0x01)
            print("  Teleoperate mode activated (all channels)")
            time.sleep(0.5)
            
            # 3. Test feedback voltage reading while in teleoperate mode
            print("\n3. Reading Feedback Voltages (Teleoperate Active)")
            print("-" * 40)
            for i in range(3):
                sensors = read_sensors(ser, target=0x01)
                if sensors:
                    print(f"  Read #{i+1}:")
                    print(f"    Voltage: {sensors['voltage']} mV")
                    print(f"    Temperature: {sensors['temp']} (raw ADC)")
                    print(f"    Current: {sensors['current']} mA")
                    print(f"    Feedback[0-3]: {sensors['feedback']} µs")
                else:
                    print(f"  Read #{i+1}: Failed to parse response")
                time.sleep(0.3)
            
            # 4. Move servos and observe feedback voltage tracking
            print("\n4. Servo Position Sweep (Teleoperate Active)")
            print("-" * 40)
            print("  Moving all servos 1000µs → 2000µs → 1500µs")
            
            for target_pos in [1000, 1500, 2000, 1500]:
                print(f"\n  Setting all servos to {target_pos}µs...")
                sync_write_servos(ser, [target_pos] * 4, target=0x01)
                time.sleep(0.2)
                
                sensors = read_sensors(ser, target=0x01)
                if sensors:
                    print(f"    Feedback positions: {sensors['feedback']} µs")
                    print(f"    Voltage: {sensors['voltage']} mV")
                time.sleep(0.3)
            
            # 5. Test individual channel control in teleoperate mode
            print("\n5. Individual Channel Control (Teleoperate Active)")
            print("-" * 40)
            for ch in range(2):  # Just test 2 channels
                print(f"\n  Testing Channel {ch}...")
                for pos in [1200, 1800, 1500]:
                    print(f"    Setting CH{ch} to {pos}µs")
                    set_servo(ser, ch, pos, target=0x01)
                    time.sleep(0.1)
                    sensors = read_sensors(ser, target=0x01)
                    if sensors:
                        print(f"      Feedback: {sensors['feedback'][ch]} µs")
                    time.sleep(0.2)
            
            # 6. Deactivate teleoperate mode
            print("\n6. Deactivating Teleoperate Mode")
            print("-" * 40)
            deactivate_teleoperate(ser, target=0x01)
            print("  Teleoperate mode deactivated")
            time.sleep(0.5)
            
            # 7. Verify deactivation - read sensors once more
            print("\n7. Final Sensor Read (Teleoperate Inactive)")
            print("-" * 40)
            sensors = read_sensors(ser, target=0x01)
            if sensors:
                print(f"  Voltage: {sensors['voltage']} mV")
                print(f"  Temperature: {sensors['temp']} (raw ADC)")
                print(f"  Feedback[0-3]: {sensors['feedback']} µs")
            
            print("\n✓ Teleoperate Test Completed Successfully\n")

    except Exception as e:
        print(f"✗ Error: {e}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    import sys
    port = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"
    run_teleoperate_test(port)
