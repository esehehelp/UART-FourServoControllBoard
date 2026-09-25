// Command uploader flashes UART-FSCB firmware over USB without a WCH-LinkE
// (#54, Go port of firmware/test/dlm_upload.py, flow modelled on
// esehehelp/ch32x035f7p6-micro-devboard's ch32_1k2touch.py):
//
//  1. find the board's USB-CDC port (VID 0x1A86 / PID 0xFE0C) or use -port
//  2. ask the firmware to enter the bootloader (DLM): send command 0xF0
//     (-mode touch uses a 1200 bps open/close instead, for #45)
//  3. wait until the WCH BootROM (PID 0x55E0) shows up
//  4. run "wchisp flash <firmware>", retrying once
//
// If no board port is found the flash is attempted directly (board already in
// the bootloader, e.g. JP1 shorted).
//
//	uploader [-port PORT] [-wchisp PATH] [-mode dlm|touch] [-legacy] firmware.bin
package main

import (
	"flag"
	"fmt"
	"os"
	"time"
)

func main() {
	port := flag.String("port", "", "board serial port (default: auto-detect by USB VID/PID)")
	wchisp := flag.String("wchisp", "", "path to wchisp (default: PATH, PlatformIO tool-wchisp, ~/.cargo/bin)")
	mode := flag.String("mode", "dlm", "bootloader trigger: dlm (command 0xF0) or touch (1200 bps open/close)")
	legacy := flag.Bool("legacy", false, "send 0xF0 in the pre-V0.8 packet format (firmware without TTL)")
	timeout := flag.Duration("wait", 8*time.Second, "how long to wait for the BootROM")
	flag.Parse()
	if flag.NArg() != 1 {
		fmt.Fprintln(os.Stderr, "usage: uploader [-port PORT] [-wchisp PATH] [-mode dlm|touch] [-legacy] <firmware.bin|.elf>")
		os.Exit(2)
	}

	cfg := Config{
		Port:     *port,
		Wchisp:   *wchisp,
		Mode:     *mode,
		Legacy:   *legacy,
		Firmware: flag.Arg(0),
		Wait:     *timeout,
	}
	if err := Upload(cfg, SystemEnv()); err != nil {
		fmt.Fprintln(os.Stderr, "ERROR:", err)
		os.Exit(1)
	}
}
