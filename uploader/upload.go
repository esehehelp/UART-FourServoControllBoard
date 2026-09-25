package main

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strings"
	"time"
)

// Config holds the command-line options.
type Config struct {
	Port     string
	Wchisp   string
	Mode     string // "dlm" or "touch"
	Legacy   bool
	Firmware string
	Wait     time.Duration
}

// Env is the outside world, replaceable in tests.
type Env struct {
	FindPort      func() (string, error)
	EnterDLM      func(port string, legacy bool) error
	Touch1200     func(port string) error
	BootROMFound  func(wchisp string) bool
	RunWchisp     func(wchisp string, args ...string) error
	Sleep         func(time.Duration)
	FileExists    func(string) bool
	LookupWchisp  func() string
	Logf          func(format string, a ...any)
	PollInterval  time.Duration
	RetryInterval time.Duration
}

// Upload runs the whole trigger -> wait -> flash sequence.
func Upload(cfg Config, env Env) error {
	fw, err := resolveFirmware(cfg.Firmware, env.FileExists)
	if err != nil {
		return err
	}
	wchisp := cfg.Wchisp
	if wchisp == "" {
		wchisp = env.LookupWchisp()
	}
	if wchisp == "" {
		return fmt.Errorf("wchisp not found: pass -wchisp or install it (pio pkg install -t tool-wchisp)")
	}

	port := cfg.Port
	if port == "" {
		port, err = env.FindPort()
		if err != nil {
			env.Logf("port detection failed: %v", err)
		}
	}

	triggered := false
	if port != "" {
		switch cfg.Mode {
		case "dlm":
			env.Logf("Requesting DLM (0xF0) on %s", port)
			err = env.EnterDLM(port, cfg.Legacy)
		case "touch":
			env.Logf("Triggering BootROM via 1200 bps touch on %s", port)
			err = env.Touch1200(port)
		default:
			return fmt.Errorf("unknown -mode %q (dlm or touch)", cfg.Mode)
		}
		if err != nil {
			env.Logf("bootloader trigger failed: %v -- trying wchisp anyway", err)
		} else {
			triggered = true
		}
	} else {
		env.Logf("Board port not found -- trying wchisp directly (board already in bootloader?)")
	}

	if triggered {
		if waitBootROM(env, wchisp, cfg.Wait) {
			env.Logf("BootROM detected")
		} else {
			env.Logf("BootROM not detected before timeout -- trying wchisp anyway")
		}
	}

	env.Logf("Flashing %s", fw)
	if err := env.RunWchisp(wchisp, "flash", fw); err == nil {
		return nil
	}
	env.Logf("Retrying...")
	if triggered {
		waitBootROM(env, wchisp, 2*time.Second)
	} else {
		env.Sleep(env.RetryInterval)
	}
	if err := env.RunWchisp(wchisp, "flash", fw); err != nil {
		return fmt.Errorf("wchisp flash failed: %w", err)
	}
	return nil
}

// resolveFirmware prefers the .bin next to an .elf (wchisp flashes raw bins).
func resolveFirmware(path string, exists func(string) bool) (string, error) {
	if strings.HasSuffix(path, ".elf") {
		if bin := strings.TrimSuffix(path, ".elf") + ".bin"; exists(bin) {
			return bin, nil
		}
	}
	if !exists(path) {
		return "", fmt.Errorf("firmware file not found: %s", path)
	}
	return path, nil
}

func waitBootROM(env Env, wchisp string, timeout time.Duration) bool {
	for waited := time.Duration(0); waited < timeout; waited += env.PollInterval {
		if env.BootROMFound(wchisp) {
			return true
		}
		env.Sleep(env.PollInterval)
	}
	return false
}

// lookupWchisp searches PATH, the PlatformIO package and cargo's bin dir.
func lookupWchisp() string {
	name := "wchisp"
	if runtime.GOOS == "windows" {
		name = "wchisp.exe"
	}
	if p, err := exec.LookPath(name); err == nil {
		return p
	}
	home, _ := os.UserHomeDir()
	for _, p := range []string{
		filepath.Join(home, ".platformio", "packages", "tool-wchisp", name),
		filepath.Join(home, ".cargo", "bin", name),
	} {
		if fileExists(p) {
			return p
		}
	}
	return ""
}

func fileExists(p string) bool {
	st, err := os.Stat(p)
	return err == nil && !st.IsDir()
}
