import serial
import sys
import time
import math

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

def build_packet(target_id, source_id, cmd, data):
    pkt = bytearray([0xAA, target_id, source_id, cmd, len(data)]) + bytearray(data)
    pkt.append(crc8(pkt))
    return pkt

def parse_packet(pkt_data):
    if len(pkt_data) < 6:
        return None
    
    header, target, source, cmd, length = pkt_data[:5]
    if header != 0xAA:
        return None
    
    data = pkt_data[5:5+length]
    if len(pkt_data) < 6 + length:
        return None
        
    crc = pkt_data[5+length]
    if crc != crc8(pkt_data[:5+length]):
        return None
        
    return {
        "target": target,
        "source": source,
        "cmd": cmd,
        "data": data
    }

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
            
            # Response: AA, 00, 01, 82, 07, Type, V_H, V_L, T_H, T_L, C_H, C_L, CRC (13 bytes)
            resp = ser.read(13)
            if not resp:
                print("No response.")
                return

            result = parse_packet(resp)
            if result and result["cmd"] == 0x82:
                data = result["data"]
                if len(data) >= 7:
                    v_raw = (data[1] << 8) | data[2]
                    t_raw = (data[3] << 8) | data[4]
                    c_raw = (data[5] << 8) | data[6]
                    
                    voltage = (v_raw / 4095.0) * 3.3 * 6.1
                    temp_c = calculate_temp(t_raw)
                    # Current: Gain=16, Shunt=0.01 Ohm
                    current_ma = (c_raw / 4095.0) * 3.3 / (16.0 * 0.01) * 1000.0
                    
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
