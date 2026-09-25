import serial
import sys
import time
from protocol_utils import build_packet

def sweep_led(ser, ch, label):
    print(f"{label} sweep:")
    for duty in [0, 10, 50, 128, 255, 128, 50, 10, 0]:
        print(f"  duty={duty}")
        pkt = build_packet(0x01, 0x00, 0x30, [ch, duty])
        ser.write(pkt)
        time.sleep(0.5)

def test_led(port):
    try:
        with serial.Serial(port, 115200, timeout=1.0) as ser:
            print(f"Testing LED PWM on {port}...")
            sweep_led(ser, 0, "LED1 (PB12, Active-High)")
            sweep_led(ser, 1, "LED2 (PC3, Active-Low)")
            print("LED Test Completed.")
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python test_led.py <port>")
    else:
        test_led(sys.argv[1])
