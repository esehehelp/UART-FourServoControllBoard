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

def send_packet(ser, target, cmd, data, source=0x00):
    pkt = bytearray([0xAA, target, source, cmd, len(data)]) + bytearray(data)
    pkt.append(crc8(pkt))
    ser.write(pkt)

def set_servo(ser, idx, pos, target=0x01):
    # pos: 500 to 2500 (us)
    send_packet(ser, target, 0x01, [idx, (pos >> 8) & 0xFF, pos & 0xFF])

def set_device_id(ser, target, new_id):
    send_packet(ser, target, 0x04, [0x01, new_id])
    time.sleep(0.1)

def set_role(ser, target, role):
    """role: 0=DEVICE, 1=HOST"""
    send_packet(ser, target, 0x04, [0x02, role])
    time.sleep(0.1)

def discover_devices(ser, host_id=0x01):
    ser.reset_input_buffer()
    send_packet(ser, host_id, 0xA0, [])
    time.sleep(0.15)
    data = ser.read(256)
    if len(data) >= 6 and data[0] == 0xAA and data[3] == 0xA1:
        count = data[4]
        ids = list(data[5:5+count])
        print(f"  Found {count} device(s): {[hex(i) for i in ids]}")
    else:
        print(f"  No response (got: {data.hex() if data else 'nothing'})")

def test_uart_ring(ser):
    print("UART Ring Test: sending to device 0x02 (expect echo back)...")
    send_packet(ser, 0x02, 0x02, [0x00])
    data = ser.read(64)
    if len(data) > 0 and data[0] == 0xAA:
        print(f"  PASS: received {len(data)} bytes back via ring: {data.hex()}")
    else:
        print(f"  FAIL: no response (got {data.hex() if data else 'nothing'})")

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
            test_uart_ring(ser)

    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    import sys
    port = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"
    mode = sys.argv[2] if len(sys.argv) > 2 else None

    if mode == "--set-host":
        with serial.Serial(port, 115200, timeout=0.1) as ser:
            print(f"Setting device at {port} to HOST role (device_id=0x01)...")
            set_device_id(ser, 0xFF, 0x01)  # broadcast set ID=1
            set_role(ser, 0x01, 1)           # set role=HOST
            print("Done.")
    elif mode == "--set-device":
        with serial.Serial(port, 115200, timeout=0.1) as ser:
            print(f"Setting device at {port} to DEVICE role (device_id=0x02)...")
            set_device_id(ser, 0xFF, 0x02)  # broadcast set ID=2
            set_role(ser, 0x02, 0)           # set role=DEVICE
            print("Done.")
    elif mode == "--discover":
        with serial.Serial(port, 115200, timeout=0.2) as ser:
            print(f"Running device discovery via host at {port}...")
            discover_devices(ser, host_id=0x01)
    else:
        run_detailed_test(port)
