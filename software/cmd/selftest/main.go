// Command selftest runs non-destructive checks against a connected board:
//
//	go run ./cmd/selftest -port /dev/ttyACM0
//
// No servo is moved and no setting is written (see pkg/selftest).
package main

import (
	"flag"
	"fmt"
	"os"
	"time"

	goserial "go.bug.st/serial"

	"uart-servo-controller/config"
	"uart-servo-controller/pkg/selftest"
)

func main() {
	port := flag.String("port", "", "serial port of the board (e.g. /dev/ttyACM0, COM5)")
	id := flag.Uint("id", config.DEVICE_ID, "target device ID")
	flag.Parse()
	if *port == "" {
		fmt.Fprintln(os.Stderr, "usage: selftest -port <serial port> [-id <device id>]")
		os.Exit(2)
	}

	p, err := goserial.Open(*port, &goserial.Mode{BaudRate: config.DEFAULT_BAUD})
	if err != nil {
		fmt.Fprintf(os.Stderr, "open %s: %v\n", *port, err)
		os.Exit(1)
	}
	defer p.Close()
	_ = p.SetReadTimeout(20 * time.Millisecond)

	failed := 0
	for _, r := range selftest.NewRunner(p, uint8(*id)).Run() {
		mark := "PASS"
		if !r.Passed {
			mark = "FAIL"
			failed++
		}
		fmt.Printf("[%s] %s", mark, r.Name)
		if r.Detail != "" {
			fmt.Printf(" — %s", r.Detail)
		}
		fmt.Println()
	}
	if failed > 0 {
		fmt.Printf("%d check(s) failed\n", failed)
		os.Exit(1)
	}
	fmt.Println("all checks passed")
}
