# uploader: USB firmware upload for UART-FSCB (0xF0 -> BootROM -> wchisp)
.PHONY: all linux windows test clean
all: linux

linux:
	go build -o bin/uploader .

windows:
	GOOS=windows GOARCH=amd64 go build -o bin/uploader.exe .

test:
	go test ./...

clean:
	rm -rf bin
