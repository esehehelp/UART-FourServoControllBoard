package main

import (
	"bytes"
	"errors"
	"fmt"
	"testing"
	"time"

	"uart-servo-controller/pkg/serial"
)

type fakeEnv struct {
	port        string
	bootAfter   int // BootROM appears after this many polls (-1: never)
	flashFails  int // first N flashes fail
	polls       int
	events      []string
	flashes     []string
	legacyShown bool
}

func (f *fakeEnv) env() Env {
	return Env{
		FindPort: func() (string, error) { return f.port, nil },
		EnterDLM: func(p string, legacy bool) error {
			f.legacyShown = legacy
			f.events = append(f.events, "dlm:"+p)
			return nil
		},
		Touch1200: func(p string) error { f.events = append(f.events, "touch:"+p); return nil },
		BootROMFound: func(string) bool {
			f.polls++
			return f.bootAfter >= 0 && f.polls > f.bootAfter
		},
		RunWchisp: func(w string, args ...string) error {
			f.flashes = append(f.flashes, fmt.Sprint(args))
			if len(f.flashes) <= f.flashFails {
				return errors.New("flash failed")
			}
			return nil
		},
		Sleep:         func(time.Duration) {},
		FileExists:    func(p string) bool { return p == "fw.bin" || p == "fw.elf" },
		LookupWchisp:  func() string { return "wchisp" },
		Logf:          func(string, ...any) {},
		PollInterval:  100 * time.Millisecond,
		RetryInterval: time.Second,
	}
}

func TestUploadDLMFlow(t *testing.T) {
	f := &fakeEnv{port: "/dev/ttyACM0", bootAfter: 3}
	if err := Upload(Config{Mode: "dlm", Firmware: "fw.elf", Wait: 8 * time.Second}, f.env()); err != nil {
		t.Fatal(err)
	}
	if len(f.events) != 1 || f.events[0] != "dlm:/dev/ttyACM0" {
		t.Errorf("events = %v", f.events)
	}
	if f.polls != 4 {
		t.Errorf("BootROM polls = %d, want 4", f.polls)
	}
	// .elf is replaced by the .bin next to it
	if len(f.flashes) != 1 || f.flashes[0] != "[flash fw.bin]" {
		t.Errorf("flashes = %v", f.flashes)
	}
}

func TestUploadRetriesOnce(t *testing.T) {
	f := &fakeEnv{port: "COM5", bootAfter: 0, flashFails: 1}
	if err := Upload(Config{Mode: "touch", Firmware: "fw.bin", Wait: time.Second}, f.env()); err != nil {
		t.Fatal(err)
	}
	if f.events[0] != "touch:COM5" || len(f.flashes) != 2 {
		t.Errorf("events = %v flashes = %v", f.events, f.flashes)
	}

	f = &fakeEnv{port: "COM5", bootAfter: 0, flashFails: 2}
	if err := Upload(Config{Mode: "dlm", Firmware: "fw.bin", Wait: time.Second}, f.env()); err == nil {
		t.Error("expected error after two failed flashes")
	}
}

func TestUploadWithoutBoardFlashesDirectly(t *testing.T) {
	f := &fakeEnv{bootAfter: -1}
	if err := Upload(Config{Mode: "dlm", Firmware: "fw.bin", Wait: time.Second}, f.env()); err != nil {
		t.Fatal(err)
	}
	if len(f.events) != 0 || f.polls != 0 || len(f.flashes) != 1 {
		t.Errorf("events = %v polls = %d flashes = %v", f.events, f.polls, f.flashes)
	}
}

func TestUploadErrors(t *testing.T) {
	f := &fakeEnv{}
	if err := Upload(Config{Mode: "dlm", Firmware: "missing.bin"}, f.env()); err == nil {
		t.Error("missing firmware must fail")
	}
	e := f.env()
	e.LookupWchisp = func() string { return "" }
	if err := Upload(Config{Mode: "dlm", Firmware: "fw.bin"}, e); err == nil {
		t.Error("missing wchisp must fail")
	}
	f = &fakeEnv{port: "COM5"}
	if err := Upload(Config{Mode: "bogus", Firmware: "fw.bin"}, f.env()); err == nil {
		t.Error("unknown mode must fail")
	}
}

func TestDLMPacket(t *testing.T) {
	pkt, err := serial.Unmarshal(dlmPacket(false))
	if err != nil || pkt.Cmd != 0xF0 || pkt.Target != 0xFF || pkt.TTL != 16 || len(pkt.Data) != 0 {
		t.Errorf("v3 packet = %v, %v", pkt, err)
	}
	legacy := dlmPacket(true)
	want := []uint8{0xAA, 0xFF, 0x00, 0xF0, 0x00}
	if !bytes.Equal(legacy[:5], want) || legacy[5] != serial.CRC8(want) {
		t.Errorf("legacy packet = % X", legacy)
	}
}

func TestProbeFoundDevice(t *testing.T) {
	for out, want := range map[string]bool{
		"Found 0 device":                   false,
		"Found 1 device\n#0: CH32X035F7P6": true,
		"error: no device":                 false,
	} {
		if got := probeFoundDevice(out); got != want {
			t.Errorf("probeFoundDevice(%q) = %v, want %v", out, got, want)
		}
	}
}
