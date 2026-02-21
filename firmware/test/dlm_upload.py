import serial
import sys
import time
import subprocess
import os

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

def dlm_upload(port, bin_path):
    try:
        # Normalize path
        bin_path = os.path.abspath(bin_path)
        
        print(f"Step 1: Sending DLM command to {port}...")
        pkt = build_packet(0x01, 0x00, 0xF0, [])
        with serial.Serial(port, 115200, timeout=1.0) as ser:
            ser.write(pkt)
            print("DLM Command Sent. Board should be entering warning phase (2s).")
        
        print("Step 2: Waiting for device to jump and re-enumerate (5s total)...")
        time.sleep(5.0)
        
        print(f"Step 3: Attempting to flash via wchisp from: {bin_path}")
        if not os.path.exists(bin_path):
            print(f"Error: Firmware file not found at {bin_path}")
            return

        # Using pio pkg exec to ensure tool-wchisp is used
        cmd = [
            "pio", "pkg", "exec", "-p", "tool-wchisp", "--", 
            "wchisp", "flash", bin_path
        ]
        
        print(f"Running: {' '.join(cmd)}")
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        print("\n--- wchisp output ---")
        print(result.stdout)
        if result.stderr:
            print("Errors:")
            print(result.stderr)
        print("----------------------")
        
        if result.returncode == 0:
            print("\nSUCCESS: Firmware updated via DLM!")
        else:
            print("\nFAILED: Flash process failed.")
            
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python dlm_upload.py <port> [bin_path]")
    else:
        # Default to ELF file which is always generated
        script_dir = os.path.dirname(os.path.abspath(__file__))
        default_bin = os.path.join(script_dir, "../.pio/build/genericCH32X035F7P6/firmware.elf")
        
        bin_p = sys.argv[2] if len(sys.argv) > 2 else default_bin
        dlm_upload(sys.argv[1], bin_p)
