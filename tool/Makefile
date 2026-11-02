# Builds equinox_diag from the unmodified Aries SDK.
# Needs only gcc, make and the Linux kernel headers.
#   make                  normal build
#   make NO_BLOCK_READ=1  for adapters without SMBus block reads
#   make test             run tests against the simulated chip
SDK     ?= ../sources/meta-openbmc-retimer-v2.16.2/files
BUILD   ?= build
CC      ?= gcc
CFLAGS  ?= -O2 -g
CPPFLAGS += -Icompat -I$(SDK)
DEFS    := $(if $(NO_BLOCK_READ),-DSMBUS_BLOCK_READ_UNSUPPORTED)
BASE    := -std=gnu11 -D_GNU_SOURCE $(DEFS)
SDK_SRC := aries_api aries_i2c aries_link aries_margin aries_misc astera_log platform
OBJS    := $(addprefix $(BUILD)/,$(addsuffix .o,$(SDK_SRC)) equinox_diag.o)

$(BUILD)/equinox_diag: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ -lm -lpthread

$(BUILD)/%.o: $(SDK)/%.c | $(BUILD)
	$(CC) $(BASE) $(CPPFLAGS) $(CFLAGS) -w -c -o $@ $<

$(BUILD)/equinox_diag.o: equinox_diag.c | $(BUILD)
	$(CC) $(BASE) $(CPPFLAGS) $(CFLAGS) -Wall -Wextra -c -o $@ $<

$(BUILD):
	mkdir -p $@

$(BUILD)/fake_aries.so: test/fake_aries.c | $(BUILD)
	$(CC) -std=gnu11 -O1 -g -I$(SDK) -shared -fPIC -o $@ $< -ldl

test: $(BUILD)/equinox_diag $(BUILD)/fake_aries.so
	$(MAKE) BUILD=$(BUILD)/noblock NO_BLOCK_READ=1 $(BUILD)/noblock/equinox_diag
	./test/run_tests.sh $(BUILD)

clean:
	rm -rf $(BUILD)

.PHONY: test clean
