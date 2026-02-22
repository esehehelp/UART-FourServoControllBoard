import serial
import sys
import time
import math
import threading

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

def send_pattern(ser, target_id, steps, loop=True):
    """
    steps: list of (duty, duration_ms)
    """
    mode = 1 if loop else 0
    count = len(steps)
    if count > 20:
        print("Warning: Max 20 steps allowed. Truncating.")
        steps = steps[:20]
        count = 20
    
    data = [mode, count]
    for duty, dur in steps:
        data.append(duty & 0xFF)
        data.append((dur >> 8) & 0xFF)
        data.append(dur & 0xFF)
    
    pkt = build_packet(target_id, 0x00, 0x07, data)
    ser.write(pkt)

def serial_monitor(ser, stop_event):
    while not stop_event.is_set():
        if ser.in_waiting:
            try:
                line = ser.readline().decode('ascii', errors='replace').strip()
                if line:
                    print(f"  [Board]: {line}")
            except Exception as e:
                pass
        time.sleep(0.01)

def test_led_patterns(port):
    target_id = 0x01
    try:
        with serial.Serial(port, 115200, timeout=0.1) as ser:
            print(f"--- LED Pattern Test on {port} ---")
            
            stop_monitor = threading.Event()
            monitor_thread = threading.Thread(target=serial_monitor, args=(ser, stop_monitor), daemon=True)
            monitor_thread.start()

            # 1. Simple Blink (500ms ON, 500ms OFF)
            print("\nPattern 1: Simple Blink (500ms ON/OFF)")
            send_pattern(ser, target_id, [
                (255, 500),
                (0, 500)
            ])
            time.sleep(4)

            # 2. Heartbeat (Double Blink)
            print("\nPattern 2: Heartbeat (Double Blink)")
            send_pattern(ser, target_id, [
                (255, 100), (0, 100),
                (255, 100), (0, 700)
            ])
            time.sleep(4)

            # 3. Breathing (Smooth fade)
            print("\nPattern 3: Breathing (20 steps)")
            steps = []
            for i in range(10): # Fade in
                duty = int(math.sin((i / 10) * (math.pi / 2)) * 255)
                steps.append((duty, 100))
            for i in range(10): # Fade out
                duty = int(math.cos((i / 10) * (math.pi / 2)) * 255)
                steps.append((duty, 100))
            send_pattern(ser, target_id, steps)
            time.sleep(6)

            # 4. SOS Pattern
            print("\nPattern 4: SOS Pattern")
            sos = []
            for _ in range(3): sos.append((255, 200)); sos.append((0, 200)) # S
            for _ in range(3): sos.append((255, 600)); sos.append((0, 200)) # O
            for _ in range(3): sos.append((255, 200)); sos.append((0, 200)) # S
            sos.append((0, 1000))
            send_pattern(ser, target_id, sos)
            time.sleep(10)

            print("\nStopping Pattern (Setting static 0)")
            ser.write(build_packet(target_id, 0x00, 0x05, [0]))
            time.sleep(1)

            stop_monitor.set()
            print("Test Completed.")
                
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python test_led_pattern.py <port>")
    else:
        test_led_patterns(sys.argv[1])
