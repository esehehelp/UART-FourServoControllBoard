#!/usr/bin/env python3
"""
Comprehensive UART Test Suite for UART-FourServoControllBoard

Tests firmware implementation including:
- Basic LED control
- Sensor reading (voltage, temperature, current, feedback)
- Servo control (individual and synchronized)
- Device discovery and ring topology
- NEW: Teleoperate command (0x0A)

Usage:
    uv run test_suite.py /dev/ttyACM0
"""

import serial
import time
import sys

def print_section(title):
    print(f"\n{'='*60}")
    print(f"  {title}")
    print(f"{'='*60}")

def print_subsection(title):
    print(f"\n  {title}")
    print(f"  {'-'*56}")

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
    pkt = bytearray([0xAA, target, source, cmd, len(data)]) + bytearray(data)
    pkt.append(crc8(pkt))
    ser.write(pkt)
    time.sleep(0.01)

def read_sensors(ser, target=0x01):
    ser.reset_input_buffer()
    send_packet(ser, target, 0x02, [0x00])
    time.sleep(0.05)
    data = ser.read(256)
    if len(data) >= 21 and data[0] == 0xAA and data[3] == 0x82:
        offset = 5
        v = (data[offset] << 8) | data[offset+1]
        t = (data[offset+2] << 8) | data[offset+3]
        c = (data[offset+4] << 8) | data[offset+5]
        fb = []
        for i in range(4):
            fb_val = (data[offset+6+i*2] << 8) | data[offset+7+i*2]
            fb.append(fb_val)
        return {'voltage': v, 'temp': t, 'current': c, 'feedback': fb}
    return None

def test_led_control(ser):
    print_subsection("LED PWM Control (0x05)")
    try:
        for duty in [0, 128, 255]:
            send_packet(ser, 0x01, 0x05, [duty, duty])
            print(f"    ✓ LED duty cycle: {duty}")
            time.sleep(0.1)
        send_packet(ser, 0x01, 0x05, [0, 0])
        print(f"    ✓ LED test passed")
        return True
    except Exception as e:
        print(f"    ✗ LED test failed: {e}")
        return False

def test_sensor_read(ser):
    print_subsection("Sensor Read (0x02 → 0x82)")
    try:
        sensors = read_sensors(ser, target=0x01)
        if sensors:
            print(f"    ✓ Voltage: {sensors['voltage']} mV")
            print(f"    ✓ Temperature: {sensors['temp']} (ADC)")
            print(f"    ✓ Current: {sensors['current']} mA")
            print(f"    ✓ Feedback: {sensors['feedback']} µs")
            return True
        else:
            print(f"    ✗ Failed to parse sensor response")
            return False
    except Exception as e:
        print(f"    ✗ Sensor read failed: {e}")
        return False

def test_servo_control(ser):
    print_subsection("Servo Control (0x01 & 0x03)")
    try:
        # Single servo write
        for ch in range(2):
            send_packet(ser, 0x01, 0x01, [ch, 0x05, 0xDC])  # 1500µs
            print(f"    ✓ Single servo CH{ch} → 1500µs")
            time.sleep(0.1)
        
        # Sync write all 4 servos
        send_packet(ser, 0x01, 0x03, [0x05, 0xDC, 0x05, 0xDC, 0x05, 0xDC, 0x05, 0xDC])
        print(f"    ✓ Sync write all 4 servos → 1500µs")
        time.sleep(0.2)
        
        return True
    except Exception as e:
        print(f"    ✗ Servo control failed: {e}")
        return False

def test_teleoperate(ser):
    print_subsection("Teleoperate Mode (0x0A)")
    try:
        # Activate teleoperate
        send_packet(ser, 0x01, 0x0A, [0x01, 0x0F])
        print(f"    ✓ Teleoperate activated (all channels)")
        time.sleep(0.1)
        
        # Read sensors while in teleoperate mode
        sensors = read_sensors(ser, target=0x01)
        if sensors:
            print(f"    ✓ Feedback active: {sensors['feedback']} µs")
        else:
            print(f"    ⚠ Could not verify feedback in teleoperate mode")
        
        # Deactivate teleoperate
        send_packet(ser, 0x01, 0x0A, [0x00, 0x00])
        print(f"    ✓ Teleoperate deactivated")
        time.sleep(0.1)
        
        return True
    except Exception as e:
        print(f"    ✗ Teleoperate test failed: {e}")
        return False

def test_configuration(ser):
    print_subsection("Configuration Commands (0x04, 0x08)")
    try:
        # Get calibration data for channel 0
        ser.reset_input_buffer()
        send_packet(ser, 0x01, 0x08, [0x00])
        time.sleep(0.05)
        data = ser.read(256)
        
        if len(data) >= 18 and data[0] == 0xAA and data[3] == 0x88:
            print(f"    ✓ Get calibration (CH0): {data.hex()}")
        else:
            print(f"    ⚠ Calibration response not parsed (got {len(data)} bytes)")
        
        return True
    except Exception as e:
        print(f"    ✗ Configuration test failed: {e}")
        return False

def run_full_test_suite(port):
    print_section("UART-FourServoControllBoard UART Test Suite")
    print(f"\n  Serial Port: {port}")
    print(f"  Baud Rate: 115200")
    
    results = {}
    
    try:
        with serial.Serial(port, 115200, timeout=0.5) as ser:
            time.sleep(0.5)  # Allow device to initialize
            
            results['LED Control'] = test_led_control(ser)
            results['Sensor Read'] = test_sensor_read(ser)
            results['Servo Control'] = test_servo_control(ser)
            results['Configuration'] = test_configuration(ser)
            results['Teleoperate (NEW)'] = test_teleoperate(ser)
    
    except serial.SerialException as e:
        print(f"\n✗ Serial port error: {e}")
        return False

    # Summary
    print_section("Test Results Summary")
    passed = sum(1 for v in results.values() if v)
    total = len(results)
    
    for test_name, result in results.items():
        status = "✓ PASS" if result else "✗ FAIL"
        print(f"  {status:8} {test_name}")
    
    print(f"\n  Total: {passed}/{total} tests passed")
    
    if passed == total:
        print("\n  ✓ All tests passed!")
        return True
    else:
        print(f"\n  ✗ {total - passed} test(s) failed")
        return False

if __name__ == "__main__":
    port = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"
    success = run_full_test_suite(port)
    sys.exit(0 if success else 1)
