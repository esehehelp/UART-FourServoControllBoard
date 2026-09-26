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
	SOFTWARE_VERSION  = "0.8.1"
	HARDWARE_REVISION = "V0.8"
)

// USB_VID_WCH is the USB vendor ID of the board (CH32X035, firmware
// lib/Drivers/inc/usb_desc.h DEF_USB_VID). Used to try the board's port first.
const USB_VID_WCH = "1A86"

// Device IDs
const (
	// DEVICE_ID is the factory default ID. The GUI talks to the ID the
	// connected board answered with (serial.DeviceInfo.ID).
	DEVICE_ID = 0x01
)

// Command codes
const (
	CMD_SERVO_WRITE      = 0x01 // Write servo position [ch, duty_h, duty_l]
	CMD_SENSOR_READ      = 0x02 // Read sensors [type]
	CMD_SYNC_WRITE       = 0x03 // Sync write all servos
	CMD_CONFIG_WRITE_OLD = 0x04 // Config write, legacy ID (alias of CMD_CONFIG_WRITE)
	CMD_CONFIG_WRITE     = 0x20 // Config write [tag, (ch), value...] -> RESP_CFG_ACK (#49)
	CMD_CONFIG_READ      = 0x21 // Config read [tag, (ch)] -> RESP_CFG_DATA (#49)
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
	RESP_CFG_ACK     = 0x84 // [tag, (ch)]
	RESP_CFG_DATA    = 0x85 // [tag, (ch), value...]
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
	ErrCodeOvervoltage    = 0x34
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
	ErrCodeOvervoltage:    "overvoltage",
	ErrCodeConfigInvalid:  "invalid config",
	ErrCodeCalInvalid:     "invalid calibration",
}

// Config tags for CMD_CONFIG_WRITE / CMD_CONFIG_READ (#49). Keep in sync with
// firmware/src/config_cmd.h. Tags 0x10-0x1F carry a channel byte.
const (
	CFG_TAG_DEVICE_ID       = 0x01 // u8
	CFG_TAG_ROLE            = 0x02 // u8
	CFG_TAG_NAME            = 0x03 // 0-15 bytes UTF-8
	CFG_TAG_DEFAULT_PULSE   = 0x10 // [ch] u16 us, 0 = PWM off at power-up
	CFG_TAG_MIN_PULSE       = 0x11 // [ch] u16 us
	CFG_TAG_MAX_PULSE       = 0x12 // [ch] u16 us
	CFG_TAG_CALIBRATED      = 0x13 // [ch] u8, read-only
	CFG_TAG_PROT_MASK       = 0x30 // u8, PROT_* bits
	CFG_TAG_MAX_CURRENT     = 0x31 // u16 mA
	CFG_TAG_MIN_VOLTAGE     = 0x32 // u16 mV
	CFG_TAG_MAX_VOLTAGE     = 0x33 // u16 mV
	CFG_TAG_MAX_TEMP        = 0x34 // i16 degC
	CFG_TAG_STALL_CURRENT   = 0x35 // u16 mA
	CFG_TAG_STALL_TIME      = 0x36 // u16 ms
	CFG_TAG_STALL_FB_DELTA  = 0x37 // u16 us
	CFG_TAG_STALL_POS_ERROR = 0x38 // u16 us
	CFG_TAG_FW_VERSION      = 0xF0 // [major, minor, patch], read-only
	CFG_TAG_CONFIG_VERSION  = 0xF1 // u8, read-only

	DEVICE_NAME_MAX = 15 // bytes
)

// Protection feature bits (CFG_TAG_PROT_MASK, #47/#28)
const (
	PROT_OVERCURRENT  = 0x01
	PROT_UNDERVOLTAGE = 0x02
	PROT_OVERVOLTAGE  = 0x04
	PROT_OVERHEAT     = 0x08
	PROT_STALL        = 0x10
)

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
