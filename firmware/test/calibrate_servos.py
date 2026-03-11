import serial
import time
import sys
import struct
import numpy as np

# --- Protocol Helpers ---
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

def get_sensor_data(ser, target_id=0x01):
    pkt = build_packet(target_id, 0x00, 0x02, [0x00])
    ser.write(pkt)
    resp = ser.read(21)
    if len(resp) == 21 and resp[0] == 0xAA and resp[3] == 0x82:
        if crc8(resp[:-1]) == resp[-1]:
            data = resp[5:20]
            v_mv = (data[1] << 8) | data[2]
            i_ma = ((data[5] << 8) | data[6]) * 2.518
            fb = []
            for i in range(4):
                val = (data[7 + i*2] << 8) | data[8 + i*2]
                fb.append(val)
            return v_mv, i_ma, fb
    return None, None, None

def set_calibration(ser, ch, slope, intercept, min_p, max_p, target_id=0x01):
    data = bytearray([ch]) 
    data += bytearray(struct.pack("<f", slope)) 
    data += bytearray(struct.pack("<f", intercept))
    data += bytearray([(min_p >> 8) & 0xFF, min_p & 0xFF])
    data += bytearray([(max_p >> 8) & 0xFF, max_p & 0xFF])
    pkt = build_packet(target_id, 0x00, 0x07, data)
    ser.write(pkt)
    time.sleep(0.1)

def high_precision_sample(ser, ch, target_id, count=100):
    """Samples ADC many times and uses robust filtering."""
    samples = []
    for _ in range(count):
        _, _, fb = get_sensor_data(ser, target_id)
        if fb: samples.append(fb[ch])
        time.sleep(0.005)
    if not samples: return None
    # Filter outliers using Interquartile Range
    q1, q3 = np.percentile(samples, [25, 75])
    iqr = q3 - q1
    filtered = [x for x in samples if (q1 - 1.5*iqr <= x <= q3 + 1.5*iqr)]
    return sum(filtered) / len(filtered)

def detect_servo(ser, channel, target_id=0x01):
    print(f"Checking CH{channel}...", end="", flush=True)
    base_samples = []
    for _ in range(10):
        _, cur_i, _ = get_sensor_data(ser, target_id)
        if cur_i is not None: base_samples.append(cur_i)
        time.sleep(0.02)
    if not base_samples: return False
    base_i = sum(base_samples) / len(base_samples)
    
    max_i = base_i
    for pos in [1000, 2000, 1500]:
        ser.write(build_packet(target_id, 0x00, 0x01, [channel, (pos >> 8) & 0xFF, pos & 0xFF]))
        for _ in range(20):
            _, cur_i, _ = get_sensor_data(ser, target_id)
            if cur_i and cur_i > max_i: max_i = cur_i
            time.sleep(0.01)
    diff = max_i - base_i
    if diff > 40:
        print(f" Detected! (Spike: {diff:.1f}mA)")
        return True
    print(f" Not found. (Max spike: {diff:.1f}mA)")
    return False

def calibrate_range_and_params(ser, channel, target_id=0x01):
    # Reset calibration to RAW for measurement
    set_calibration(ser, channel, 1.0, 0.0, 500, 2500, target_id)
    
    print(f"\n--- [HIGH PRECISION] Calibrating Servo CH{channel} ---")
    
    # 0. Warm-up
    print("  Warming up servo...")
    for _ in range(2):
        for p in [1000, 2000]:
            ser.write(build_packet(target_id, 0x00, 0x01, [channel, (p >> 8) & 0xFF, p & 0xFF]))
            time.sleep(0.6)

    # 1. Detect Limits (Stall detection)
    print("  Detecting physical limits (Stall detection)...")
    def find_limit(start, end, step):
        last_stable_pos = start
        for pos in range(start, end + step, step):
            ser.write(build_packet(target_id, 0x00, 0x01, [channel, (pos >> 8) & 0xFF, pos & 0xFF]))
            time.sleep(0.2) # Wait longer for stall to register
            # Sample current
            samples = []
            for _ in range(10):
                _, i, _ = get_sensor_data(ser, target_id)
                if i is not None: samples.append(i)
            avg_i = sum(samples) / len(samples) if samples else 0
            if avg_i > 450: # Threshold for stall
                print(f"    Stall detected at {pos}us ({avg_i:.0f}mA)")
                return last_stable_pos - (step * 4) # Larger buffer for high precision
            last_stable_pos = pos
        return end

    print("    Scanning Min...")
    safe_min = find_limit(1500, 500, -10)
    print("    Scanning Max...")
    safe_max = find_limit(1500, 2500, 10)
    print(f"  Safe Range: {safe_min}us - {safe_max}us")

    # 2. Dual-direction sampling (Hysteresis compensation)
    test_points = np.linspace(safe_min + 50, safe_max - 50, 11).astype(int)
    results_up = {}
    results_down = {}

    print("  Measuring Up-sweep...")
    for p in test_points:
        ser.write(build_packet(target_id, 0x00, 0x01, [channel, (p >> 8) & 0xFF, p & 0xFF]))
        print(f"    {p}us...", end="", flush=True)
        time.sleep(1.5)
        val = high_precision_sample(ser, channel, target_id)
        results_up[p] = val
        print(f" ADC: {val:.2f}")

    print("  Measuring Down-sweep...")
    for p in reversed(test_points):
        ser.write(build_packet(target_id, 0x00, 0x01, [channel, (p >> 8) & 0xFF, p & 0xFF]))
        print(f"    {p}us...", end="", flush=True)
        time.sleep(1.5)
        val = high_precision_sample(ser, channel, target_id)
        results_down[p] = val
        print(f" ADC: {val:.2f}")

    # Average to eliminate hysteresis
    final_pulses = []
    final_adcs = []
    for p in test_points:
        if results_up[p] and results_down[p]:
            final_pulses.append(p)
            final_adcs.append((results_up[p] + results_down[p]) / 2.0)

    if len(final_pulses) >= 2:
        slope, intercept = np.polyfit(final_adcs, final_pulses, 1)
        return float(slope), float(intercept), int(safe_min), int(safe_max)
    return None

def main():
    if len(sys.argv) < 2:
        print("Usage: python calibrate_servos.py <port>")
        return
    port = sys.argv[1]
    target_id = 0x01
    try:
        ser = serial.Serial(port, 115200, timeout=0.1)
        print(f"Connected to {port}. Searching for connected servos...")
        active_channels = [ch for ch in range(4) if detect_servo(ser, ch, target_id)]
        
        if not active_channels:
            print("\nNo servos detected.")
            return

        print(f"\nStarting high-precision calibration for: {active_channels}")
        for ch in active_channels:
            res = calibrate_range_and_params(ser, ch, target_id)
            if res:
                slope, intercept, min_p, max_p = res
                set_calibration(ser, ch, slope, intercept, min_p, max_p, target_id)
                print(f"  CH{ch}: Saved Slope={slope:.6f}, Intercept={intercept:.2f}, Range={min_p}-{max_p}")
        print("\nAll tasks completed successfully!")
    except Exception as e:
        print(f"Error: {e}")
    finally:
        if 'ser' in locals(): ser.close()

if __name__ == "__main__":
    main()
