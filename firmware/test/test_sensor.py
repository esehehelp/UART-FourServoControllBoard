import serial
import sys
import time
import math
from protocol_utils import build_packet, read_packet, RESP_SENSOR_DATA, RESP_ERROR, error_text

def calculate_temp(raw_adc):
    if raw_adc == 0 or raw_adc >= 4095: return 0.0
    r_fixed = 5100.0
    r_ntc_25 = 22000.0
    b_constant = 4050.0
    t25 = 298.15
    r_ntc = (raw_adc * r_fixed) / (4095.0 - raw_adc)
    if r_ntc <= 0: return 0.0
    inv_t = (1.0 / t25) + (1.0 / b_constant) * math.log(r_ntc / r_ntc_25)
    return (1.0 / inv_t) - 273.15

def read_sensors(port):
    try:
        with serial.Serial(port, 115200, timeout=1.0) as ser:
            print(f"Connecting to {port}...")
            ser.reset_input_buffer()
            
            # Read Sensors
            pkt = build_packet(0x01, 0x00, 0x02, [0x00])
            ser.write(pkt)
            
            # Response 0x82: Type, V, T, C, FB0-3 (15 bytes of data)
            result = read_packet(ser, RESP_SENSOR_DATA)
            if not result:
                print("No response.")
                return
            if result["cmd"] == RESP_ERROR:
                print(f"Device error: {error_text(result)}")
                return

            if result["cmd"] == RESP_SENSOR_DATA:
                data = result["data"]
                if len(data) >= 7:
                    v_raw = (data[1] << 8) | data[2]
                    t_raw = (data[3] << 8) | data[4]
                    c_raw = (data[5] << 8) | data[6]
                    
                    voltage = (v_raw / 4095.0) * 3.3 * 6.1
                    temp_c = calculate_temp(t_raw)
                    # Current: OPA2 PGA x32, Shunt=0.01 Ohm (firmware/docs/constants.md)
                    current_ma = (c_raw / 4095.0) * 3.3 / (32.0 * 0.01) * 1000.0
                    
                    print("\n" + "="*30)
                    print(f" Board ID:      0x{result['source']:02X}")
                    print(f" Voltage:       {voltage:5.2f} V  (Raw: {v_raw})")
                    print(f" Temperature:   {temp_c:5.1f} ℃  (Raw: {t_raw})")
                    print(f" Current:       {current_ma:5.1f} mA (Raw: {c_raw})")
                    print("="*30 + "\n")
                else:
                    print(f"Invalid data length: {len(data)}")
            else:
                print("Packet error or invalid response.")
                
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python test_sensor.py <port>")
    else:
        read_sensors(sys.argv[1])
