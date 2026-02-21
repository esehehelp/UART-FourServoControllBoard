import serial
import time
import sys
import threading
import csv
import math
import numpy as np
import matplotlib.pyplot as plt
from scipy.signal import medfilt

# --- Kalman Filter Class ---
class KalmanFilter:
    def __init__(self, q=1e-5, r=1e-2, p=1.0, initial_value=0):
        self.q = q # Process variance
        self.r = r # Measurement variance
        self.p = p # Estimated error
        self.x = initial_value # Value

    def update(self, measurement):
        # Prediction
        self.p = self.p + self.q
        # Measurement update
        k = self.p / (self.p + self.r)
        self.x = self.x + k * (measurement - self.x)
        self.p = (1 - k) * self.p
        return self.x

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

def calculate_temp(raw_adc):
    if raw_adc == 0 or raw_adc >= 4095: return 0.0
    r_fixed = 5100.0; r_ntc_25 = 22000.0; b_constant = 4050.0; t25 = 298.15
    r_ntc = (raw_adc * r_fixed) / (max(1, 4095.0 - raw_adc))
    if r_ntc <= 0: return 0.0
    inv_t = (1.0 / t25) + (1.0 / b_constant) * math.log(r_ntc / r_ntc_25)
    return (1.0 / inv_t) - 273.15

# --- Globals ---
running = True
ser_lock = threading.Lock()

def servo_sweep_task(ser):
    global running
    try:
        while running:
            for pos in range(1000, 2001, 20):
                if not running: break
                pkt = build_packet(0x01, 0x00, 0x01, [0x02, (pos >> 8) & 0xFF, pos & 0xFF])
                with ser_lock: ser.write(pkt)
                time.sleep(0.2)
            for pos in range(2000, 999, -20):
                if not running: break
                pkt = build_packet(0x01, 0x00, 0x01, [0x02, (pos >> 8) & 0xFF, pos & 0xFF])
                with ser_lock: ser.write(pkt)
                time.sleep(0.2)
    except Exception as e:
        if running: print(f"Servo Task Error: {e}")

def main():
    global running
    if len(sys.argv) < 2:
        print("Usage: python record_sweep.py <port>")
        return

    port = sys.argv[1]
    ts_now = int(time.time())
    filename_csv = f"sensor_log_{ts_now}.csv"
    filename_png = f"sensor_plot_{ts_now}.png"
    
    try:
        ser = serial.Serial(port, 115200, timeout=0.02)
        ser.reset_input_buffer()
        print(f"Connected to {port}. Logging for 3 seconds...")

        t = threading.Thread(target=servo_sweep_task, args=(ser,))
        t.daemon = True
        t.start()

        start_time = time.perf_counter()
        times, volts, temps, currents = [], [], [], []
        
        # Initialize Kalman filter for current
        kf = KalmanFilter(q=0.001, r=0.1) # Tune these parameters as needed
        
        while time.perf_counter() - start_time < 3.0:
            pkt = build_packet(0x01, 0x00, 0x02, [0x00])
            with ser_lock: ser.write(pkt)
            
            resp = ser.read(13)
            if len(resp) == 13 and resp[0] == 0xAA and resp[3] == 0x82:
                if crc8(resp[:-1]) == resp[-1]:
                    ts = time.perf_counter() - start_time
                    data = resp[5:-1]
                    v_raw = (data[1] << 8) | data[2]
                    t_raw = (data[3] << 8) | data[4]
                    c_raw = (data[5] << 8) | data[6]
                    
                    voltage = (v_raw / 4095.0) * 3.3 * 6.1
                    temp_c = calculate_temp(t_raw)
                    current_ma = (c_raw / 4095.0) * 3.3 / (32.0 * 0.01) * 1000.0
                    
                    times.append(ts)
                    volts.append(voltage)
                    temps.append(temp_c)
                    currents.append(current_ma)

        running = False
        t.join(timeout=0.5)
        
        if not times:
            print("No data collected.")
            return

        # --- Apply Filters ---
        print(f"Collected {len(times)} samples. Applying filters...")
        
        # 1. Median Filter (Window size 11)
        curr_med = medfilt(currents, kernel_size=11)
        
        # 2. Kalman Filter (Apply to median-filtered data)
        curr_final = []
        for c in curr_med:
            curr_final.append(kf.update(c))
            
        # --- Save CSV ---
        with open(filename_csv, 'w', newline='') as csvfile:
            writer = csv.writer(csvfile)
            writer.writerow(["Timestamp", "Voltage_V", "Temp_C", "Current_mA", "Current_Filtered_mA"])
            for i in range(len(times)):
                writer.writerow([f"{times[i]:.4f}", f"{volts[i]:.3f}", f"{temps[i]:.1f}", f"{currents[i]:.1f}", f"{curr_final[i]:.1f}"])
            
        # --- Plotting ---
        print("Generating plot...")
        plt.figure(figsize=(12, 8))
        
        plt.subplot(2, 1, 1)
        plt.plot(times, volts, color='red', alpha=0.5, label='Voltage (V)')
        plt.ylabel('Voltage (V)')
        plt.legend(loc='upper right')
        plt.title(f"Sensor Data with Filters (PGA x32)")

        plt.subplot(2, 1, 2)
        plt.plot(times, currents, color='blue', alpha=0.2, label='Current Raw (mA)')
        plt.plot(times, curr_final, color='black', linewidth=1.5, label='Current Filtered (mA)')
        plt.ylabel('Current (mA)')
        plt.xlabel('Time (s)')
        plt.legend(loc='upper right')

        plt.tight_layout()
        plt.savefig(filename_png)
        print(f"Data saved to {filename_csv} and {filename_png}")

    except Exception as e:
        print(f"\nError: {e}")
    finally:
        running = False
        ser.close()

if __name__ == "__main__":
    main()
