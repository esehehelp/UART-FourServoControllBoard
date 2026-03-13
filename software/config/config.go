package config

// Protocol constants
const (
	PKT_HEADER    = 0xAA
	HOST_ID       = 0x00
	BROADCAST_ID  = 0xFF
	DEFAULT_BAUD  = 115200
	TIMEOUT_MS    = 100
)

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
	CMD_LED_SET          = 0x30 // Set LED duty [ch: 0=LED1,1=LED2, duty: 0-255]
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
)

// Servo constants
const (
	SERVO_MIN_PULSE    = 0
	SERVO_MAX_PULSE    = 3000
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
	// Voltage: (d[1]<<8|d[2]) * 0.00491
	VOLTAGE_SCALE = 0.00491
	// Current: (d[5]<<8|d[6]) * 2.518
	CURRENT_SCALE = 2.518
	// Feedback: (d[7+j*2]<<8|d[8+j*2]) * 3.3/4095/0.55
	FB_VOLTAGE_SCALE = 3.3 / 4095.0 / 0.55
	// Temperature from NTC
	TEMP_R0       = 22000.0
	TEMP_B        = 4050.0
	TEMP_T0       = 298.15
)

// Calibration parameters (manual position calibration via PWM off)
const (
	// CAL_WAIT_CENTER_MS: settle time after moving to center (ms)
	CAL_WAIT_CENTER_MS = 600
	// CAL_SAMPLE_COUNT: number of FBV ADC readings to average per position
	CAL_SAMPLE_COUNT = 5
	// CAL_SAMPLE_INTERVAL_MS: interval between consecutive FBV samples (ms)
	CAL_SAMPLE_INTERVAL_MS = 20
	// CAL_PULSE_MIN / CAL_PULSE_MAX: reference pulse widths stored as the min/max
	// position commands (µs). These define the commanded range that the firmware
	// will use; the FBV recorded at each position provides the feedback mapping.
	CAL_PULSE_MIN = 500
	CAL_PULSE_MAX = 2500
)
