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

def send_dlm(port, repeat=1):
    try:
        with serial.Serial(port, 115200, timeout=1.0) as ser:
            for i in range(repeat):
                pkt = build_packet(0x01, 0x00, 0xF0, [])
                print(f"Sending DLM Command ({i+1}/{repeat}): {pkt.hex(' ')}")
                ser.write(pkt)
                if repeat > 1 and i < repeat - 1:
                    time.sleep(0.5)
            
            # Read response if any
            time.sleep(0.1)
            resp = ser.read_all()
            if resp:
                print(f"Response: {resp.decode(errors='ignore')}")
                
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python enter_dlm.py <port> [repeat_count]")
    else:
        rep = int(sys.argv[2]) if len(sys.argv) > 2 else 1
        send_dlm(sys.argv[1], rep)
