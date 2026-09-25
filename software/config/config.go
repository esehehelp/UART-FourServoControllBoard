package config

// Protocol constants
const (
	PKT_HEADER    = 0xAA
	HOST_ID       = 0x00
	BROADCAST_ID  = 0xFF
	DEFAULT_BAUD  = 115200
	TIMEOUT_MS    = 100
	// MAX_PACKET_LEN is the firmware parser buffer size (Parser_t.buf[128]).
	// A packet is 6 bytes of framing + data, so data is limited to 122 bytes.
	MAX_PACKET_LEN = 128
	MAX_DATA_LEN   = MAX_PACKET_LEN - 6
)

// USB_VID_WCH is the USB vendor ID of the board (CH32X035, firmware
// lib/Drivers/inc/usb_desc.h DEF_USB_VID). Used to try the board's port first.
const USB_VID_WCH = "1A86"

// Device IDs
const (
	DEVICE_ID = 0x01
)

// Command codes
const (
	CMD_SERVO_WRITE      = 0x01 // Write servo position [ch, duty_h, duty_l]
	CMD_SENSOR_READ      = 0x02 // Read sensors [type]
	CMD_SYNC_WRITE       = 0x03 // Sync write all servos
	CMD_CONFIG_WRITE     = 0x04 // Config write
	CMD_LED_SET          = 0x05 // Set LED duty [led1_duty, led2_duty] (0-255 each)
	CMD_PD_VOLTAGE       = 0x06 // Set PD voltage [mv_h, mv_l]
	CMD_CAL_SAVE         = 0x07 // Save calibration [ch, slope(4B), intercept(4B), min_h, min_l, max_h, max_l]
	CMD_CAL_GET          = 0x08 // Get calibration [ch]
	CMD_SERVO_FREE       = 0x09 // Free servo (PWM off) [ch_mask]
)

// Sensor types for CMD_SENSOR_READ
const (
	SENSOR_TYPE_ALL = 0x00
)

// Response codes
const (
	RESP_SENSOR_DATA = 0x82
)

// UI/Display constants
const (
	MAX_PLOT_POINTS     = 100
	UPDATE_INTERVAL_MS  = 33  // ~30fps
	GRAPH_WINDOW_SECS   = 5
	// SENSOR_TIMEOUT_MS: sensor data is marked invalid when no response
	// arrives for this long (device disconnected / not answering)
	SENSOR_TIMEOUT_MS   = 2000
)

// Servo constants
// SERVO_MIN_PULSE / SERVO_MAX_PULSE match the firmware default calibration
// range (firmware/src/config.c: min_pulse=500, max_pulse=2500). The firmware
// clamps to the per-channel calibrated range, so values outside it have no
// effect.
const (
	SERVO_MIN_PULSE    = 500
	SERVO_MAX_PULSE    = 2500
	SERVO_DEFAULT      = 1500
	NUM_SERVOS         = 4
)

// LED constants
const (
	LED_MIN_DUTY = 0
	LED_MAX_DUTY = 255
)

// USB-PD constants
const (
	PD_VOLTAGE_5V  = 5000
	PD_VOLTAGE_9V  = 9000
	PD_VOLTAGE_15V = 15000
	PD_VOLTAGE_20V = 20000
)

// Calibration constants
const (
	NUM_CHANNELS       = 4
	CAL_DATA_LEN       = 13 // ch + slope(4) + intercept(4) + min(2) + max(2)
)

// Kalman filter parameters
const (
	KF_Q_VOLTAGE = 0.01
	KF_R_VOLTAGE = 0.1
	KF_Q_CURRENT = 1.0
	KF_R_CURRENT = 10.0
)

// Sensor data parsing
const (
	// Circuit constants come from firmware/docs/constants.txt.

	// Voltage [V]: (d[1]<<8|d[2]) * 0.00491
	// = 3.3 V / 4095 * 6.1 (divider R1=5.1k, R2=1.0k)
	VOLTAGE_SCALE = 0.00491
	// Current [mA]: (d[5]<<8|d[6]) * 2.518
	// = 3.3 V / 4095 / 32 (OPA2 PGA x32) / 0.01 Ω (10 mΩ shunt) * 1000
	CURRENT_SCALE = 2.518
	// Feedback: (d[7+j*2]<<8|d[8+j*2]) * 3.3/4095/0.55
	FB_VOLTAGE_SCALE = 3.3 / 4095.0 / 0.55
	// Temperature from NTC (22k, B=4050) on the low side of a 5.1k pull-up.
	// TEMP_R0 is the NTC resistance at TEMP_T0 (B-parameter equation only);
	// TEMP_SERIES_R is the pull-up used for the divider calculation.
	TEMP_R0       = 22000.0
	TEMP_B        = 4050.0
	TEMP_T0       = 298.15
	TEMP_SERIES_R = 5100.0
)

// Calibration parameters (manual position calibration via PWM off)
const (
	// CAL_WAIT_CENTER_MS: settle time after moving to center (ms)
	CAL_WAIT_CENTER_MS = 600
	// CAL_SAMPLE_COUNT: number of FBV ADC readings to average per position
	CAL_SAMPLE_COUNT = 5
	// CAL_SAMPLE_TIMEOUT_MS: max wait for each new sensor response while
	// sampling; calibration fails if the device stops answering (ms)
	CAL_SAMPLE_TIMEOUT_MS = 500
	// CAL_FB_RAW_MIN / CAL_FB_RAW_MAX: accepted feedback value range in ADC
	// counts (0.1 V - 3.2 V at the 3.3 V ADC). Values at the rails mean the
	// feedback wire is open/shorted or the input is saturated.
	CAL_FB_RAW_MIN = 124
	CAL_FB_RAW_MAX = 3971
	// CAL_PULSE_MIN / CAL_PULSE_MAX: reference pulse widths stored as the min/max
	// position commands (µs). These define the commanded range that the firmware
	// will use; the FBV recorded at each position provides the feedback mapping.
	CAL_PULSE_MIN = 500
	CAL_PULSE_MAX = 2500
)
