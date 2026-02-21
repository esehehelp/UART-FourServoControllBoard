import serial
import time

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

def send_packet(ser, dest_id, cmd, data):
    pkt = bytearray([0xAA, dest_id, cmd, len(data)]) + bytearray(data)
    pkt.append(crc8(pkt))
    ser.write(pkt)

def set_servo(ser, idx, pos):
    # pos: 500 to 2500 (us)
    send_packet(ser, 1, 0x01, [idx, (pos >> 8) & 0xFF, pos & 0xFF])

def run_detailed_test(port):
    try:
        with serial.Serial(port, 115200, timeout=0.1) as ser:
            print(f"Detailed Motor Test on {port}...")
            
            # 1. Test each channel one by one
            for ch in range(4):
                time.sleep(0.5)
                print(f"\n--- Testing Channel {ch} ---")
                
                print("Moving to 1000us (Min)...")
                set_servo(ser, ch, 1000)
                time.sleep(0.5)
                
                print("Moving to 2000us (Max)...")
                set_servo(ser, ch, 2000)
                time.sleep(0.5)
                
                print("Moving to 1500us (Center)...")
                set_servo(ser, ch, 1500)
                time.sleep(0.5)

            # 2. Sweep Test (All channels simultaneously)
            print("\n--- All Channels Sweep Test ---")
            for pos in range(1000, 2001, 50):
                for ch in range(4):
                    set_servo(ser, ch, pos)
                time.sleep(0.05)
            
            for pos in range(2000, 999, -50):
                for ch in range(4):
                    set_servo(ser, ch, pos)
                time.sleep(0.05)

            print("\nTest Sequence Completed.")
            
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    import sys
    port = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"
    run_detailed_test(port)
