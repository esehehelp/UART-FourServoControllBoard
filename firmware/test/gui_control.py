import tkinter as tk
from tkinter import ttk, messagebox
import serial
import serial.tools.list_ports
import threading
import time
import collections
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import numpy as np
import sys

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

# --- Filter Helpers ---
class KalmanFilter:
    def __init__(self, q=0.01, r=0.1, x0=0, p0=1):
        self.q = q  # Process noise covariance
        self.r = r  # Measurement noise covariance
        self.x = x0 # Estimated state
        self.p = p0 # Estimated error covariance

    def update(self, measurement):
        # Prediction
        self.p = self.p + self.q
        # Update
        k = self.p / (self.p + self.r)
        self.x = self.x + k * (measurement - self.x)
        self.p = (1 - k) * self.p
        return self.x

# --- GUI Application ---
class ServoControlApp:
    def __init__(self, root):
        self.root = root
        self.root.title("UART 4-Servo Board Controller")
        
        self.ser = None
        self.running = True
        self.device_id = 0x01
        self.after_id = None
        
        # Data storage for plotting
        self.max_points = 100
        self.times = collections.deque(maxlen=self.max_points)
        
        # Raw buffers (for median filter) - Reduced window for better responsiveness
        self.v_raw_buf = collections.deque(maxlen=3)
        self.i_raw_buf = collections.deque(maxlen=3)
        self.t_raw_buf = collections.deque(maxlen=3)

        # Filtered history for plotting
        self.voltages = collections.deque(maxlen=self.max_points)
        self.currents = collections.deque(maxlen=self.max_points)
        self.temps = collections.deque(maxlen=self.max_points)
        self.voltages_raw = collections.deque(maxlen=self.max_points)
        self.currents_raw = collections.deque(maxlen=self.max_points)

        # Kalman Filters - Ultra-responsive (effectively trusts measurement immediately after median)
        # Set Q very high and R very low to eliminate lag
        self.kf_v = KalmanFilter(q=200.0, r=0.002)
        self.kf_i = KalmanFilter(q=2000.0, r=0.02)
        self.kf_t = KalmanFilter(q=20.0, r=0.002)

        self.start_time = time.time()

        self.setup_ui()
        self.update_ports()
        
        # Periodic Tasks
        self.root.protocol("WM_DELETE_WINDOW", self.on_closing)
        self.update_gui_data()

    def setup_ui(self):
        # Main Layout: Left (Controls), Right (Graphs)
        main_frame = ttk.Frame(self.root, padding="10")
        main_frame.pack(fill=tk.BOTH, expand=True)
        
        left_frame = ttk.Frame(main_frame)
        left_frame.pack(side=tk.LEFT, fill=tk.Y, padx=(0, 10))
        
        right_frame = ttk.Frame(main_frame)
        right_frame.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True)

        # --- Connection Area ---
        conn_frame = ttk.LabelFrame(left_frame, text="Connection", padding="5")
        conn_frame.pack(fill=tk.X, pady=5)
        
        self.port_var = tk.StringVar()
        self.port_combo = ttk.Combobox(conn_frame, textvariable=self.port_var)
        self.port_combo.pack(side=tk.LEFT, padx=5)
        
        self.btn_connect = ttk.Button(conn_frame, text="Connect", command=self.toggle_connect)
        self.btn_connect.pack(side=tk.LEFT, padx=5)
        
        ttk.Button(conn_frame, text="↻", width=3, command=self.update_ports).pack(side=tk.LEFT)

        # --- Servo Control Area ---
        servo_frame = ttk.LabelFrame(left_frame, text="Servo Control (Pulse Width us)", padding="5")
        servo_frame.pack(fill=tk.X, pady=5)
        
        self.servo_sliders = []
        for i in range(4):
            f = ttk.Frame(servo_frame)
            f.pack(fill=tk.X, pady=2)
            ttk.Label(f, text=f"CH{i}:", width=5).pack(side=tk.LEFT)
            s = ttk.Scale(f, from_=500, to=2500, orient=tk.HORIZONTAL, command=lambda v, idx=i: self.send_servo(idx, v))
            s.set(1500)
            s.pack(side=tk.LEFT, fill=tk.X, expand=True)
            self.servo_sliders.append(s)

        # --- LED Control Area ---
        led_frame = ttk.LabelFrame(left_frame, text="LED Brightness", padding="5")
        led_frame.pack(fill=tk.X, pady=5)
        self.led_slider = ttk.Scale(led_frame, from_=0, to=255, orient=tk.HORIZONTAL, command=self.send_led)
        self.led_slider.set(0)
        self.led_slider.pack(fill=tk.X, padx=5)

        # --- Voltage Control Area ---
        volt_frame = ttk.LabelFrame(left_frame, text="USB-PD Voltage (SAFETY LIMIT: 6V)", padding="5")
        volt_frame.pack(fill=tk.X, pady=5)
        
        v_ctrl_frame = ttk.Frame(volt_frame)
        v_ctrl_frame.pack(fill=tk.X)
        
        self.volt_var = tk.DoubleVar(value=5.0)
        # Restricted range to 5.0V - 6.0V
        self.volt_spin = ttk.Spinbox(v_ctrl_frame, from_=5.0, to=6.0, increment=0.1, textvariable=self.volt_var, width=5)
        self.volt_spin.pack(side=tk.LEFT, padx=5)
        ttk.Label(v_ctrl_frame, text="V").pack(side=tk.LEFT)
        
        self.btn_set_v = ttk.Button(v_ctrl_frame, text="Set", command=self.apply_voltage)
        self.btn_set_v.pack(side=tk.RIGHT, padx=5)
        
        self.lbl_v_set = ttk.Label(volt_frame, text="Target: 5.0 V", font=("", 10, "bold"))
        self.lbl_v_set.pack(pady=5)

        # --- Maintenance Area ---
        maint_frame = ttk.LabelFrame(left_frame, text="Maintenance", padding="5")
        maint_frame.pack(fill=tk.X, pady=5)
        ttk.Button(maint_frame, text="ENTER DLM (Flash Mode)", command=self.enter_dlm).pack(fill=tk.X, padx=5)

        # --- Sensor Display Area ---
        stat_frame = ttk.LabelFrame(left_frame, text="Real-time Stats", padding="5")
        stat_frame.pack(fill=tk.X, pady=5)
        self.lbl_v = ttk.Label(stat_frame, text="Voltage: --- V")
        self.lbl_v.pack(anchor=tk.W)
        self.lbl_i = ttk.Label(stat_frame, text="Current: --- mA")
        self.lbl_i.pack(anchor=tk.W)
        self.lbl_t = ttk.Label(stat_frame, text="Temp: --- C")
        self.lbl_t.pack(anchor=tk.W)

        # --- Matplotlib Area ---
        self.fig, (self.ax_v, self.ax_i) = plt.subplots(2, 1, figsize=(5, 6), sharex=True)
        self.fig.tight_layout(pad=3.0)
        
        # Plot lines for raw and filtered data
        self.line_v_raw, = self.ax_v.plot([], [], 'r:', alpha=0.3, label="Raw V")
        self.line_v,     = self.ax_v.plot([], [], 'r-', linewidth=2, label="Filtered V")
        self.line_i_raw, = self.ax_i.plot([], [], 'b:', alpha=0.3, label="Raw mA")
        self.line_i,     = self.ax_i.plot([], [], 'b-', linewidth=2, label="Filtered mA")
        
        self.ax_v.set_ylabel("Volts")
        self.ax_v.grid(True)
        self.ax_v.legend(loc="upper right", fontsize='small')
        
        self.ax_i.set_ylabel("mA")
        self.ax_i.set_xlabel("Time (s)")
        self.ax_i.grid(True)
        self.ax_i.legend(loc="upper right", fontsize='small')

        self.canvas = FigureCanvasTkAgg(self.fig, master=right_frame)
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)

    def update_ports(self):
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.port_combo['values'] = ports
        if ports: self.port_combo.current(0)

    def toggle_connect(self):
        if self.ser and self.ser.is_open:
            self.ser.close()
            self.btn_connect.config(text="Connect")
        else:
            try:
                port = self.port_var.get()
                self.ser = serial.Serial(port, 115200, timeout=0.1)
                self.btn_connect.config(text="Disconnect")
                threading.Thread(target=self.serial_reader, daemon=True).start()
            except Exception as e:
                messagebox.showerror("Error", f"Could not open port: {e}")

    def send_servo(self, ch, val):
        if not self.ser or not self.ser.is_open: return
        pulse = int(float(val))
        # Command 0x01 (Single Servo Write), Data: [CH, PulseH, PulseL]
        pkt = build_packet(self.device_id, 0x00, 0x01, [ch, (pulse >> 8) & 0xFF, pulse & 0xFF])
        self.ser.write(pkt)

    def send_led(self, val):
        if not self.ser or not self.ser.is_open: return
        duty = int(float(val))
        pkt = build_packet(self.device_id, 0x00, 0x05, [duty])
        self.ser.write(pkt)

    def apply_voltage(self):
        if not self.ser or not self.ser.is_open:
            messagebox.showwarning("Warning", "Connect to the board first!")
            return
        
        try:
            v_set = self.volt_var.get()
        except tk.TclError:
            messagebox.showerror("Error", "Invalid voltage value")
            return

        # Safety Limit Check (GUI Side)
        if v_set > 6.0:
            messagebox.showwarning("Safety Limit", "Voltage is restricted to max 6.0V for safety.")
            v_set = 6.0
            self.volt_var.set(6.0)
        elif v_set < 5.0:
            v_set = 5.0
            self.volt_var.set(5.0)

        self.lbl_v_set.config(text=f"Target: {v_set:.1f} V")
        mv = int(v_set * 1000)
        # Command 0x06: [VoltH, VoltL]
        pkt = build_packet(self.device_id, 0x00, 0x06, [(mv >> 8) & 0xFF, mv & 0xFF])
        self.ser.write(pkt)

    def enter_dlm(self):
        if not self.ser or not self.ser.is_open:
            messagebox.showwarning("Warning", "Connect to the board first!")
            return
        if messagebox.askyesno("Confirm", "The board will reset into ISP mode. Continue?"):
            pkt = build_packet(self.device_id, 0x00, 0xF0, [])
            self.ser.write(pkt)
            time.sleep(0.5)
            self.toggle_connect() # Disconnect since board will reset

    def serial_reader(self):
        buffer = bytearray()
        while self.running:
            if not self.ser or not self.ser.is_open:
                time.sleep(0.1)
                continue
            try:
                data = self.ser.read(1)
                if not data: continue
                buffer.append(data[0])
                
                # Look for header 0xAA and sensor response (Cmd=0x82, Len=7) -> Total 13 bytes
                while len(buffer) >= 13:
                    if buffer[0] == 0xAA:
                        # Packet format: AA [Tgt] [Src] [Cmd] [Len] [D0..D6] [CRC]
                        pkt = buffer[:13]
                        if pkt[3] == 0x82 and crc8(pkt[:-1]) == pkt[-1]:
                            self.parse_sensor_data(pkt[5:12])
                            buffer = buffer[13:]
                        else:
                            buffer.pop(0)
                    else:
                        buffer.pop(0)
            except Exception:
                break

    def parse_sensor_data(self, data):
        # res[0]: Type (0x00)
        # res[1:2]: Voltage RAW
        # res[3:4]: Temp RAW
        # res[5:6]: Current RAW
        v_raw = (data[1] << 8) | data[2]
        t_raw = (data[3] << 8) | data[4]
        c_raw = (data[5] << 8) | data[6]
        
        # Scaling
        # Voltage: RAW * (3.3/4095) * (5.1+1)/1
        v_v_raw = v_raw * 0.00491
        # Current: RAW * (3.3/4095) / (0.01 * 32) (Amps) -> RAW * 2.52 (mA)
        i_ma_raw = c_raw * 2.518

        # --- Median Filter (Step 1) ---
        self.v_raw_buf.append(v_v_raw)
        self.i_raw_buf.append(i_ma_raw)
        self.t_raw_buf.append(t_raw)
        
        v_med = np.median(list(self.v_raw_buf))
        i_med = np.median(list(self.i_raw_buf))
        t_med = np.median(list(self.t_raw_buf))

        # --- Kalman Filter (Step 2) ---
        v_filtered = self.kf_v.update(v_med)
        i_filtered = self.kf_i.update(i_med)
        t_filtered_raw = self.kf_t.update(t_med)

        # Convert T_RAW to Celsius (NTC 22k, B=4050, Pullup=5.1k)
        try:
            res = (t_filtered_raw * 5100) / (4095 - t_filtered_raw)
            temp_c = 1 / (np.log(res / 22000) / 4050 + 1 / 298.15) - 273.15
        except:
            temp_c = 0

        self.times.append(time.time() - self.start_time)
        self.voltages_raw.append(v_v_raw)
        self.voltages.append(v_filtered)
        self.currents_raw.append(i_ma_raw)
        self.currents.append(i_filtered)
        self.temps.append(temp_c)

    def update_gui_data(self):
        if not self.running: return

        # Request sensor data (Cmd 0x02, Type 0x00)
        if self.ser and self.ser.is_open:
            pkt = build_packet(self.device_id, 0x00, 0x02, [0x00])
            self.ser.write(pkt)

        if self.voltages:
            self.lbl_v.config(text=f"Voltage: {self.voltages[-1]:.2f} V")
            self.lbl_i.config(text=f"Current: {self.currents[-1]:.0f} mA")
            self.lbl_t.config(text=f"Temp: {self.temps[-1]:.1f} C")
            
            # Update Graphs
            self.line_v_raw.set_data(list(self.times), list(self.voltages_raw))
            self.line_v.set_data(list(self.times), list(self.voltages))
            self.line_i_raw.set_data(list(self.times), list(self.currents_raw))
            self.line_i.set_data(list(self.times), list(self.currents))
            
            self.ax_v.relim()
            self.ax_v.autoscale_view()
            self.ax_i.relim()
            self.ax_i.autoscale_view()
            self.canvas.draw()
            
        self.after_id = self.root.after(30, self.update_gui_data)

    def on_closing(self):
        self.running = False
        if self.after_id:
            self.root.after_cancel(self.after_id)
        if self.ser: 
            try: self.ser.close()
            except: pass
        plt.close('all')
        self.root.destroy()
        sys.exit(0)

if __name__ == "__main__":
    root = tk.Tk()
    app = ServoControlApp(root)
    root.mainloop()
