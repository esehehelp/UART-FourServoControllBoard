package device

import (
	"encoding/binary"
	"fmt"
	"unicode/utf8"

	"uart-servo-controller/config"
	"uart-servo-controller/pkg/serial"
)

// Board settings over CMD_CONFIG_WRITE / CMD_CONFIG_READ (#49).

// ProtectionSettings mirrors the firmware's ProtectionConfig_t (#47/#28).
type ProtectionSettings struct {
	FeatureMask    uint8  `json:"featureMask"` // config.PROT_* bits
	MaxCurrentMA   uint16 `json:"maxCurrentMA"`
	MinVoltageMV   uint16 `json:"minVoltageMV"`
	MaxVoltageMV   uint16 `json:"maxVoltageMV"`
	MaxTempC       int16  `json:"maxTempC"`
	StallCurrentMA uint16 `json:"stallCurrentMA"`
	StallTimeMS    uint16 `json:"stallTimeMS"`
	StallFBDelta   uint16 `json:"stallFBDelta"`
	StallPosError  uint16 `json:"stallPosError"`
}

// protU16 lists the unsigned 16-bit protection fields in tag order
// (MaxTempC is signed and handled separately).
func (p *ProtectionSettings) protU16() []struct {
	tag uint8
	val *uint16
} {
	return []struct {
		tag uint8
		val *uint16
	}{
		{config.CFG_TAG_MAX_CURRENT, &p.MaxCurrentMA},
		{config.CFG_TAG_MIN_VOLTAGE, &p.MinVoltageMV},
		{config.CFG_TAG_MAX_VOLTAGE, &p.MaxVoltageMV},
		{config.CFG_TAG_STALL_CURRENT, &p.StallCurrentMA},
		{config.CFG_TAG_STALL_TIME, &p.StallTimeMS},
		{config.CFG_TAG_STALL_FB_DELTA, &p.StallFBDelta},
		{config.CFG_TAG_STALL_POS_ERROR, &p.StallPosError},
	}
}

// ReadConfig reads one tag. key is [tag] or [tag, ch]; the returned value
// excludes the echoed key.
func (c *Controller) ReadConfig(key ...uint8) ([]uint8, error) {
	if len(key) == 0 {
		return nil, fmt.Errorf("missing tag")
	}
	pkt := serial.NewPacket(c.target(), config.CMD_CONFIG_READ, key)
	resp, err := c.sendAndWait(pkt, func(r *serial.Packet) bool {
		if r.Cmd != config.RESP_CFG_DATA || len(r.Data) < len(key) {
			return false
		}
		for i, k := range key {
			if r.Data[i] != k {
				return false
			}
		}
		return true
	})
	if err != nil {
		return nil, err
	}
	return resp.Data[len(key):], nil
}

// WriteConfig writes one tag ([tag, (ch), value...]) and waits for the ACK;
// the board saves it to flash before answering.
func (c *Controller) WriteConfig(data ...uint8) error {
	if len(data) < 2 {
		return fmt.Errorf("missing value")
	}
	pkt := serial.NewPacket(c.target(), config.CMD_CONFIG_WRITE, data)
	_, err := c.sendAndWait(pkt, func(r *serial.Packet) bool {
		return r.Cmd == config.RESP_CFG_ACK && len(r.Data) >= 1 && r.Data[0] == data[0]
	})
	return err
}

// GetName returns the board's user label (#29).
func (c *Controller) GetName() (string, error) {
	v, err := c.ReadConfig(config.CFG_TAG_NAME)
	return string(v), err
}

// SetName stores a user label on the board (at most DEVICE_NAME_MAX bytes).
func (c *Controller) SetName(name string) error {
	if len(name) > config.DEVICE_NAME_MAX {
		return fmt.Errorf("name longer than %d bytes", config.DEVICE_NAME_MAX)
	}
	if !utf8.ValidString(name) {
		return fmt.Errorf("name is not valid UTF-8")
	}
	for _, b := range []byte(name) {
		if b == 0 {
			return fmt.Errorf("name contains NUL")
		}
	}
	return c.WriteConfig(append([]uint8{config.CFG_TAG_NAME}, name...)...)
}

// GetFirmwareVersion returns "major.minor.patch".
func (c *Controller) GetFirmwareVersion() (string, error) {
	v, err := c.ReadConfig(config.CFG_TAG_FW_VERSION)
	if err != nil {
		return "", err
	}
	if len(v) < 3 {
		return "", fmt.Errorf("short firmware version")
	}
	return fmt.Sprintf("%d.%d.%d", v[0], v[1], v[2]), nil
}

// GetProtection reads all protection settings.
func (c *Controller) GetProtection() (ProtectionSettings, error) {
	var p ProtectionSettings
	v, err := c.ReadConfig(config.CFG_TAG_PROT_MASK)
	if err != nil {
		return p, err
	}
	if len(v) < 1 {
		return p, fmt.Errorf("short value for tag 0x%02X", config.CFG_TAG_PROT_MASK)
	}
	p.FeatureMask = v[0]
	for _, f := range p.protU16() {
		v, err := c.ReadConfig(f.tag)
		if err != nil {
			return p, err
		}
		if len(v) < 2 {
			return p, fmt.Errorf("short value for tag 0x%02X", f.tag)
		}
		*f.val = binary.BigEndian.Uint16(v)
	}
	v, err = c.ReadConfig(config.CFG_TAG_MAX_TEMP)
	if err != nil {
		return p, err
	}
	if len(v) < 2 {
		return p, fmt.Errorf("short value for tag 0x%02X", config.CFG_TAG_MAX_TEMP)
	}
	p.MaxTempC = int16(binary.BigEndian.Uint16(v))
	return p, nil
}

// SetProtection writes the settings that differ from the board's current
// ones. Each write is validated and saved by the board; on the first
// rejection the remaining fields are left unchanged.
func (c *Controller) SetProtection(want ProtectionSettings) error {
	cur, err := c.GetProtection()
	if err != nil {
		return err
	}
	if want.MinVoltageMV >= want.MaxVoltageMV {
		return fmt.Errorf("min voltage must be below max voltage")
	}
	// Order the voltage writes so the pair stays valid on the board after
	// each single write (the firmware rejects min >= max).
	wantFields, curFields := want.protU16(), cur.protU16()
	order := []int{0, 1, 2, 3, 4, 5, 6}
	if want.MinVoltageMV >= cur.MaxVoltageMV {
		order = []int{0, 2, 1, 3, 4, 5, 6} // raise max first
	}
	for _, i := range order {
		if *wantFields[i].val == *curFields[i].val {
			continue
		}
		v := make([]uint8, 3)
		v[0] = wantFields[i].tag
		binary.BigEndian.PutUint16(v[1:], *wantFields[i].val)
		if err := c.WriteConfig(v...); err != nil {
			return fmt.Errorf("tag 0x%02X: %w", v[0], err)
		}
	}
	if want.MaxTempC != cur.MaxTempC {
		t := uint16(want.MaxTempC)
		if err := c.WriteConfig(config.CFG_TAG_MAX_TEMP, uint8(t>>8), uint8(t)); err != nil {
			return fmt.Errorf("max temperature: %w", err)
		}
	}
	if want.FeatureMask != cur.FeatureMask {
		if err := c.WriteConfig(config.CFG_TAG_PROT_MASK, want.FeatureMask); err != nil {
			return fmt.Errorf("feature mask: %w", err)
		}
	}
	return nil
}

// SetDefaultPulse sets a channel's power-up pulse (0 = PWM off, #27/#48).
func (c *Controller) SetDefaultPulse(ch uint8, us uint16) error {
	return c.WriteConfig(config.CFG_TAG_DEFAULT_PULSE, ch, uint8(us>>8), uint8(us))
}
