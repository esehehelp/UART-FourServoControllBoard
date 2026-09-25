package config

// Protocol constants
const (
	PKT_HEADER    = 0xAA
	HOST_ID       = 0x00
	BROADCAST_ID  = 0xFF
	DEFAULT_BAUD  = 115200
	TIMEOUT_MS    = 100
	// Packet: [0xAA | Target | Source | TTL | Cmd | Len | Data... | CRC8]
	// MAX_PACKET_LEN is the firmware parser buffer size (Parser_t.buf[128]).
	// A packet is 7 bytes of framing + data, so data is limited to 121 bytes.
	MAX_PACKET_LEN = 128
	PKT_OVERHEAD   = 7
	MAX_DATA_LEN   = MAX_PACKET_LEN - PKT_OVERHEAD
	// DEFAULT_TTL: hop limit set on every packet the host sends; each ring
	// node decrements it and drops the packet at 0 (loop prevention, #13)
	DEFAULT_TTL = 16
)

// Software version (#39). Major.Minor follow the hardware revision (V0.8);
// HW / FW / SW compatibility is recorded in PRs and release notes.
const (
	SOFTWARE_VERSION  = "0.8.0"
	HARDWARE_REVISION = "V0.8"
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
	CMD_LED_SET          = 0x30 // Set LED duty [ch: 0=LED1,1=LED2, duty: 0-255]
	CMD_PD_VOLTAGE       = 0x06 // Set PD voltage [mv_h, mv_l] -> RESP_PD_ACK
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
	RESP_CFG_ACK     = 0x84 // [sub_cmd]
	RESP_PD_ACK      = 0x86 // [mv_h, mv_l]
	RESP_CAL_ACK     = 0x87 // [ch]
	RESP_CAL_DATA    = 0x88
	RESP_ERROR       = 0xEE // [original_cmd, error_code]
)

// Error codes in RESP_ERROR (#52). Keep in sync with
// firmware/src/error_codes.h.
const (
	ErrCodeOK             = 0x00
	ErrCodeUnknownCmd     = 0x01
	ErrCodeBadLength      = 0x02
	ErrCodeBadChannel     = 0x03
	ErrCodeBadValue       = 0x04
	ErrCodeCRC            = 0x10
	ErrCodeTimeout        = 0x11
	ErrCodeBufferOverflow = 0x12
	ErrCodeTTLExpired     = 0x13
	ErrCodeFlashWrite     = 0x20
	ErrCodeADC            = 0x21
	ErrCodePWM            = 0x22
	ErrCodeOvercurrent    = 0x30
	ErrCodeUndervoltage   = 0x31
	ErrCodeOverheat       = 0x32
	ErrCodeStall          = 0x33
	ErrCodeConfigInvalid  = 0x40
	ErrCodeCalInvalid     = 0x41
)

// ErrCodeNames maps error codes to human-readable messages
var ErrCodeNames = map[uint8]string{
	ErrCodeOK:             "OK",
	ErrCodeUnknownCmd:     "unknown command",
	ErrCodeBadLength:      "bad length",
	ErrCodeBadChannel:     "bad channel",
	ErrCodeBadValue:       "value out of range",
	ErrCodeCRC:            "CRC error",
	ErrCodeTimeout:        "timeout",
	ErrCodeBufferOverflow: "buffer overflow",
	ErrCodeTTLExpired:     "TTL expired (target not found in ring)",
	ErrCodeFlashWrite:     "flash write failed",
	ErrCodeADC:            "ADC error",
	ErrCodePWM:            "PWM error",
	ErrCodeOvercurrent:    "overcurrent",
	ErrCodeUndervoltage:   "undervoltage",
	ErrCodeOverheat:       "overheat",
	ErrCodeStall:          "stall",
	ErrCodeConfigInvalid:  "invalid config",
	ErrCodeCalInvalid:     "invalid calibration",
}

// ACK_TIMEOUT_MS: how long commands that expect an ACK (0x04, 0x06, 0x07)
// wait for RESP_*_ACK or RESP_ERROR
const ACK_TIMEOUT_MS = 500

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

// USB-PD constants (#38). The firmware rejects requests outside
// 5000-16800 mV; the GUI is limited further to 12 V as a safety margin for
// the V0.8 board (3.3 V LDO input rating).
const (
	PD_VOLTAGE_5V     = 5000
	PD_VOLTAGE_9V     = 9000
	PD_VOLTAGE_12V    = 12000
	PD_VOLTAGE_MIN    = 5000
	PD_VOLTAGE_MAX_UI = 12000
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
	// Circuit constants come from firmware/docs/constants.md.

	// Voltage [V]: (d[1]<<8|d[2]) * 0.00491
	// = 3.3 V / 4095 * 6.1 (divider R7=5.1k, R8=1.0k)
	VOLTAGE_SCALE = 0.00491
	// Current [mA]: (d[5]<<8|d[6]) * 2.518
	// = 3.3 V / 4095 / 32 (OPA2 PGA x32) / 0.01 Ω (10 mΩ shunt) * 1000
	CURRENT_SCALE = 2.518
	// Servo FB divider. Most RC servos have no FB output, so the divider is
	// added outside the board by the builder (J3 pins go straight to the MCU;
	// no divider on the PCB). Values depend on the build — see
	// firmware/docs/constants.md "サーボ FB 分圧".
	// This build: pot output -> V_FB_R1 -> FB pin -> V_FB_R2 -> GND
	V_FB_R1 = 2700.0 // [Ω] pot side
	V_FB_R2 = 3300.0 // [Ω] GND side
	// Feedback [V at the pot]: (d[7+j*2]<<8|d[8+j*2]) * 3.3/4095 / ratio
	// ratio = V_FB_R2 / (V_FB_R1 + V_FB_R2) = 0.55
	FB_VOLTAGE_SCALE = (3.3 / 4095.0) / (V_FB_R2 / (V_FB_R1 + V_FB_R2))
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
