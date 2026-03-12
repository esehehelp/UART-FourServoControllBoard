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
import struct
import queue

# --- Signal Processing ---
class KalmanFilter:
    def __init__(self, q=0.01, r=0.1):
        self.q = q; self.r = r; self.x = 0; self.p = 1
    def update(self, measurement):
        self.p = self.p + self.q
        k = self.p / (self.p + self.r)
        self.x = self.x + k * (measurement - self.x)
        self.p = (1 - k) * self.p
        return self.x

# --- Serial Communication Management ---
def crc8(data):
    crc = 0
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80: crc = (crc << 1) ^ 0x07
            else: crc <<= 1
            crc &= 0xFF
    return crc

class SerialManager:
    def __init__(self, pkt_queue, status_cb):
        self.ser = None; self.running = True
        self.queue = pkt_queue; self.status_cb = status_cb
        threading.Thread(target=self._worker, daemon=True).start()

    def _worker(self):
        buffer = bytearray()
        while self.running:
            if not self.ser or not self.ser.is_open:
                self.status_cb("Scanning...", "orange")
                self._try_connect(); time.sleep(1); continue
            try:
                if self.ser.in_waiting > 0:
                    buffer.extend(self.ser.read(self.ser.in_waiting))
                    while len(buffer) >= 6:
                        if buffer[0] == 0xAA:
                            t_len = 6 + buffer[4]
                            if len(buffer) >= t_len:
                                pkt = buffer[:t_len]
                                if crc8(pkt[:-1]) == pkt[-1]:
                                    self.queue.put(pkt)
                                    buffer = buffer[t_len:]
                                else: buffer.pop(0)
                            else: break
                        else: buffer.pop(0)
                else: time.sleep(0.01)
            except: self.ser = None

    def _try_connect(self):
        for p in serial.tools.list_ports.comports():
            if any(x in p.device.upper() for x in ["USB", "ACM", "SERIAL"]):
                try:
                    self.ser = serial.Serial(p.device, 115200, timeout=0.01)
                    self.status_cb(f"Connected: {p.device}", "green"); return
                except: continue

    def send(self, target, cmd, data):
        if self.ser and self.ser.is_open:
            pkt = bytearray([0xAA, target, 0x00, cmd, len(data)]) + bytearray(data)
            pkt.append(crc8(pkt))
            try: self.ser.write(pkt)
            except: self.ser = None

# --- Main Application ---
class ServoControlApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Servo Control")
        
        self.pkt_queue = queue.Queue()
        self.sm = SerialManager(self.pkt_queue, self.on_status)
        
        self.max_pts = 100
        self.times = collections.deque(maxlen=self.max_pts)
        self.v_data = collections.deque(maxlen=self.max_pts)
        self.i_data = collections.deque(maxlen=self.max_pts)
        self.t_data = collections.deque(maxlen=self.max_pts)
        self.fb_v = [collections.deque(maxlen=self.max_pts) for _ in range(4)]
        self.v_history = collections.deque(maxlen=50) 
        
        self.kf_v = KalmanFilter(0.01, 0.1); self.kf_i = KalmanFilter(1.0, 10.0)
        self.start_time = time.time(); self.cal_active = False
        
        self.setup_ui()
        self.update_loop()

    def setup_ui(self):
        self.root.columnconfigure(1, weight=1); self.root.rowconfigure(0, weight=1)
        side = ttk.Frame(self.root, padding=10); side.grid(row=0, column=0, sticky="nsw")
        
        self.lbl_status = tk.Label(side, text="Starting...", fg="gray"); self.lbl_status.pack(fill=tk.X)
        self.lbl_mon = ttk.Label(side, text="V: -- I: -- T: --", font=("monospace", 10, "bold")); self.lbl_mon.pack(pady=5)

        # Control
        ctl_frame = ttk.LabelFrame(side, text="Servo Positions (us)", padding=5); ctl_frame.pack(fill=tk.X)
        self.sliders = []
        for i in range(4):
            f = ttk.Frame(ctl_frame); f.pack(fill=tk.X)
            ttk.Label(f, text=f"CH{i}").pack(side=tk.LEFT)
            s = ttk.Scale(f, from_=0, to=3000, orient=tk.HORIZONTAL, command=lambda v, idx=i: self.sm.send(0x01, 0x01, [idx, int(float(v))>>8, int(float(v))&0xFF]))
            s.set(1500); s.pack(side=tk.RIGHT, fill=tk.X, expand=True); self.sliders.append(s)

        # Calibration
        cal_frame = ttk.LabelFrame(side, text="Calibration", padding=5); cal_frame.pack(fill=tk.X, pady=10)
        self.cal_ch = tk.IntVar(value=0)
        for i in range(4): ttk.Radiobutton(cal_frame, text=f"CH{i}", variable=self.cal_ch, value=i).pack(side=tk.LEFT)
        ttk.Button(cal_frame, text="Start Calibration", command=self.start_cal).pack(fill=tk.X, pady=5)
        self.txt_log = tk.Text(side, height=10, width=30, font=("monospace", 8)); self.txt_log.pack(fill=tk.BOTH)

        # Graphs
        graph_frame = ttk.Frame(self.root); graph_frame.grid(row=0, column=1, sticky="nsew")
        self.fig, self.axes = plt.subplots(4, 1, figsize=(5, 8), sharex=True); self.fig.tight_layout(pad=2.0)
        y_lims = [(0, 15), (0, 4000), (0, 80), (0, 6.5)]
        y_labs = ["Voltage (V)", "Current (mA)", "Temp (degC)", "FB Voltage (V)"]
        for ax, lim, lab in zip(self.axes, y_lims, y_labs):
            ax.set_ylim(lim); ax.set_ylabel(lab); ax.grid(True, alpha=0.3)
        self.ln_v, = self.axes[0].plot([], [], 'r-'); self.ln_i, = self.axes[1].plot([], [], 'b-'); self.ln_t, = self.axes[2].plot([], [], 'g-')
        self.ln_fbs = [self.axes[3].plot([], [], label=f"CH{i}")[0] for i in range(4)]
        self.canvas = FigureCanvasTkAgg(self.fig, master=graph_frame); self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)

    def on_status(self, text, color):
        self.root.after(0, lambda: self.lbl_status.config(text=text, fg=color))

    def log(self, msg):
        self.root.after(0, lambda: (self.txt_log.insert(tk.END, msg + "\n"), self.txt_log.see(tk.END)))

    def start_cal(self):
        if self.cal_active: return
        self.cal_active = True; threading.Thread(target=self._cal_worker, daemon=True).start()

    def _cal_worker(self):
        ch = self.cal_ch.get(); self.log(f"CH{ch} Cal Start")
        def get_state(p):
            self.sm.send(0x01, 0x01, [ch, p>>8, p&0xFF]); time.sleep(0.8)
            self.v_history.clear(); time.sleep(0.4)
            ripple = (max(self.v_history) - min(self.v_history)) if self.v_history else 0.0
            avg_fb = np.mean(list(self.fb_v[ch])[-5:]) if self.fb_v[ch] else 0.0
            return ripple, avg_fb

        def find_limit(start, end):
            curr = start; _, last_fb = get_state(start); dir = 1 if end > start else -1
            # 1. Coarse search (100us)
            for p in range(start, end + dir, dir * 100):
                ripple, fb = get_state(p)
                if ripple > 0.1 or abs(fb - last_fb) < 0.01: break
                curr = p; last_fb = fb
            # 2. Fine search (1us) 
            target = curr + dir * 100
            for p in range(curr, target + dir, dir):
                ripple, fb = get_state(p)
                if ripple > 0.1: break
                curr = p
            return curr

        l_min = find_limit(1500, 0); l_max = find_limit(1500, 3000)
        self.log(f"Limits: {l_min} - {l_max}")
        # Coefficient calc
        _, v1 = get_state(l_min + 150); _, v2 = get_state(l_max - 150)
        a1, a2 = v1 / (3.3/4095/0.55), v2 / (3.3/4095/0.55)
        slope = ((l_max-150) - (l_min+150)) / (a2 - a1); inter = (l_min+150) - slope * a1
        # Save to flash
        data = bytearray([ch]) + bytearray(struct.pack("<f", float(slope))) + bytearray(struct.pack("<f", float(inter))) \
               + bytearray([l_min>>8, l_min&0xFF, l_max>>8, l_max&0xFF])
        self.sm.send(0x01, 0x07, data)
        self.log("Saved to flash"); self.cal_active = False

    def update_loop(self):
        while not self.pkt_queue.empty():
            p = self.pkt_queue.get(); d = p[5:-1]
            if p[3] == 0x82:
                v = ((d[1]<<8)|d[2])*0.00491; i = ((d[5]<<8)|d[6])*2.518; t_raw = (d[3]<<8)|d[4]
                self.v_history.append(v); self.times.append(time.time()-self.start_time)
                self.v_data.append(self.kf_v.update(v)); self.i_data.append(self.kf_i.update(i))
                try:
                    res = (t_raw*5100)/(4095-t_raw)
                    self.t_data.append(1/(np.log(res/22000)/4050+1/298.15)-273.15)
                except: self.t_data.append(0)
                for j in range(4): self.fb_v[j].append(((d[7+j*2]<<8)|d[8+j*2])*3.3/4095/0.55)
                self.lbl_mon.config(text=f"V: {self.v_data[-1]:.2f}V I: {self.i_data[-1]:.0f}mA T: {self.t_data[-1]:.1f}C")

        if self.times:
            t = list(self.times)
            for ax in self.axes: ax.set_xlim(max(0, t[-1]-5), max(5, t[-1]))
            self.ln_v.set_data(t, list(self.v_data)); self.ln_i.set_data(t, list(self.i_data)); self.ln_t.set_data(t, list(self.t_data))
            for k in range(4): self.ln_fbs[k].set_data(t, list(self.fb_v[k]))
            self.canvas.draw_idle()
        
        self.sm.send(0x01, 0x02, [0x00])
        self.root.after(33, self.update_loop)

if __name__ == "__main__":
    root = tk.Tk(); app = ServoControlApp(root)
    root.protocol("WM_DELETE_WINDOW", lambda: (setattr(app.sm, 'running', False), root.destroy(), sys.exit(0)))
    root.mainloop()
