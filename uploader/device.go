package main

import (
	"fmt"
	"os"
	"os/exec"
	"runtime"
	"strings"
	"time"

	goserial "go.bug.st/serial"
	"go.bug.st/serial/enumerator"

	"uart-servo-controller/config"
	"uart-servo-controller/pkg/serial"
)

const (
	boardVID   = "1A86" // firmware/lib/Drivers/inc/usb_desc.h DEF_USB_VID
	boardPID   = "FE0C" // DEF_USB_PID
	bootROMPID = 0x55E0 // WCH BootROM (ISP) USB PID
	cmdDLM     = 0xF0
)

var bootROMVIDs = []uint16{0x4348, 0x1A86}

// SystemEnv returns the Env that talks to real hardware and tools.
func SystemEnv() Env {
	return Env{
		FindPort:      findBoardPort,
		EnterDLM:      enterDLM,
		Touch1200:     touch1200,
		BootROMFound:  bootROMFound,
		RunWchisp:     runWchisp,
		Sleep:         time.Sleep,
		FileExists:    fileExists,
		LookupWchisp:  lookupWchisp,
		Logf:          func(f string, a ...any) { fmt.Printf(f+"\n", a...) },
		PollInterval:  100 * time.Millisecond,
		RetryInterval: time.Second,
	}
}

func findBoardPort() (string, error) {
	ports, err := enumerator.GetDetailedPortsList()
	if err != nil {
		return "", err
	}
	for _, p := range ports {
		if p.IsUSB && strings.EqualFold(p.VID, boardVID) && strings.EqualFold(p.PID, boardPID) {
			return p.Name, nil
		}
	}
	return "", nil
}

// dlmPacket builds the 0xF0 request. It is broadcast so it works whatever
// device ID the board has; broadcasts are executed by the USB-connected
// board and never forwarded into the ring.
func dlmPacket(legacy bool) []uint8 {
	if legacy {
		// pre-V0.8 format without TTL: [AA, target, source, cmd, len, crc]
		pkt := []uint8{config.PKT_HEADER, config.BROADCAST_ID, config.HOST_ID, cmdDLM, 0}
		return append(pkt, serial.CRC8(pkt))
	}
	return serial.NewPacket(config.BROADCAST_ID, cmdDLM, nil).Marshal()
}

func enterDLM(port string, legacy bool) error {
	p, err := goserial.Open(port, &goserial.Mode{BaudRate: config.DEFAULT_BAUD})
	if err != nil {
		return err
	}
	defer p.Close()
	if _, err := p.Write(dlmPacket(legacy)); err != nil {
		return err
	}
	return p.Drain()
}

func touch1200(port string) error {
	p, err := goserial.Open(port, &goserial.Mode{BaudRate: 1200})
	if err != nil {
		return err
	}
	return p.Close()
}

func bootROMFound(wchisp string) bool {
	if runtime.GOOS == "linux" {
		for _, vid := range bootROMVIDs {
			id := fmt.Sprintf("%04x:%04x", vid, bootROMPID)
			if exec.Command("lsusb", "-d", id).Run() == nil {
				return true
			}
		}
	}
	return wchispProbe(wchisp)
}

func wchispProbe(wchisp string) bool {
	cmd := exec.Command(wchisp, "probe")
	out := make(chan []byte, 1)
	go func() {
		b, _ := cmd.CombinedOutput()
		out <- b
	}()
	select {
	case b := <-out:
		return probeFoundDevice(string(b))
	case <-time.After(2 * time.Second):
		if cmd.Process != nil {
			_ = cmd.Process.Kill()
		}
		return false
	}
}

// probeFoundDevice interprets "wchisp probe" output.
func probeFoundDevice(out string) bool {
	if strings.Contains(out, "Found 0 device") {
		return false
	}
	return (strings.Contains(out, "Found ") && strings.Contains(out, "device")) || strings.Contains(out, "CH32")
}

func runWchisp(wchisp string, args ...string) error {
	cmd := exec.Command(wchisp, args...)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	return cmd.Run()
}
