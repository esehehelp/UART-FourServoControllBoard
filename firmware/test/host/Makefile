# Host-side (x86) unit tests for firmware logic that does not need the MCU.
# Uses the real firmware headers; hardware calls are stubbed in stubs.c.
CC     ?= gcc
SRC    := ../../src
CFLAGS := -std=gnu11 -Wall -Wno-unused-parameter -I$(SRC) -I../../lib/Drivers/inc -DCH32X035
OUT    := build

.PHONY: test clean
test: $(OUT)/test_protocol $(OUT)/test_config
	$(OUT)/test_protocol
	$(OUT)/test_config

# protocol.c + the real config modules, with the flash page emulated in RAM
$(OUT)/test_protocol: test_protocol.c stubs.c $(SRC)/protocol.c $(SRC)/config.c $(SRC)/config_cmd.c | $(OUT)
	$(CC) $(CFLAGS) -w -include host_flash.h -DCONFIG_FLASH_PTR=host_flash -o $@ $^

$(OUT)/test_config: test_config.c $(SRC)/config.c | $(OUT)
	$(CC) $(CFLAGS) -w -o $@ $^

$(OUT):
	mkdir -p $@

clean:
	rm -rf $(OUT)
