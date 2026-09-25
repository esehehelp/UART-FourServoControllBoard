module uart-servo-uploader

go 1.22.0

require (
	go.bug.st/serial v1.6.4
	uart-servo-controller v0.0.0
)

require (
	github.com/creack/goselect v0.1.2 // indirect
	golang.org/x/sys v0.30.0 // indirect
)

// Reuse the packet / CRC implementation of the GUI instead of a third copy (#19).
replace uart-servo-controller => ../software
