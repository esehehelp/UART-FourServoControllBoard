import serial
import sys
import time

def crc8(data):
    crc = 0
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80: crc = (crc << 1) ^ 0x07
            else: crc <<= 1
            crc &= 0xFF
    return crc

def build_packet(target_id, source_id, cmd, data):
    pkt = bytearray([0xAA, target_id, source_id, cmd, len(data)]) + bytearray(data)
    pkt.append(crc8(pkt))
    return pkt

def test_led(port):
    try:
        with serial.Serial(port, 115200, timeout=1.0) as ser:
            print(f"Testing LED PWM on {port}...")
            
            # Brightness Sweep
            for duty in [0, 10, 50, 128, 255, 128, 50, 10, 0]:
                print(f"Setting LED Duty: {duty}")
                pkt = build_packet(0x01, 0x00, 0x05, [duty])
                ser.write(pkt)
                time.sleep(0.5)
                
            print("LED Test Completed.")
                
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python test_led.py <port>")
    else:
        test_led(sys.argv[1])
