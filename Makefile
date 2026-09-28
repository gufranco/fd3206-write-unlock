AVR_CC ?= avr-gcc
AVR_OBJCOPY ?= avr-objcopy
AVR_OBJDUMP ?= avr-objdump
AVR_NM ?= avr-nm
AVR_SIZE ?= avr-size
AVRDUDE ?= avrdude
HOST_CC ?= cc
PKG_CONFIG ?= pkg-config

MCU ?= attiny2313a
AVRDUDE_PART ?= t2313a
PROGRAMMER ?= usbasp
PORT ?= usb
LFUSE := 0xE4
HFUSE := 0xD9
EFUSE := 0xFF
F_CPU := 8000000UL

SKETCH := fdswriteunlock
BUILD := build
SOURCES := $(wildcard $(SKETCH)/*.c)
HEADERS := $(wildcard $(SKETCH)/*.h)
OBJECTS := $(SOURCES:$(SKETCH)/%.c=$(BUILD)/%.o)

AVR_FLAGS := -mmcu=$(MCU) -DF_CPU=$(F_CPU) -std=gnu11 -Os -Wall -Wextra -Werror -ffunction-sections -fdata-sections
AVR_LINK_FLAGS := -mmcu=$(MCU) -Wl,--gc-sections
SIMAVR_REPOSITORY := https://github.com/buserror/simavr.git
SIMAVR_COMMIT := d6aed536755bd0ff72b9d2bf02e4a476af64ce82
SIMAVR_PATCH := test/simavr-datasheet-fixes.patch
SIMAVR_DIR := $(BUILD)/simavr-src
SIMAVR_STAMP := $(SIMAVR_DIR)/.built

HOST_FLAGS := -std=c11 -O2 -Wall -Wextra -Werror -Wpedantic -isystem $(SIMAVR_DIR)/simavr/sim -isystem $(SIMAVR_DIR)/simavr/cores $(patsubst -I%,-isystem %,$(shell $(PKG_CONFIG) --cflags libelf))
HOST_LIBS := $(shell $(PKG_CONFIG) --libs libelf)

.PHONY: all size test clean fuses flash

all: $(BUILD)/$(SKETCH).hex

$(BUILD):
	mkdir -p $@

$(BUILD)/%.o: $(SKETCH)/%.c $(HEADERS) | $(BUILD)
	$(AVR_CC) $(AVR_FLAGS) -c -o $@ $<

$(BUILD)/$(SKETCH).elf: $(OBJECTS)
	$(AVR_CC) $(AVR_LINK_FLAGS) -o $@ $(OBJECTS)

$(BUILD)/$(SKETCH).hex: $(BUILD)/$(SKETCH).elf
	$(AVR_OBJCOPY) -O ihex -R .eeprom $< $@

$(BUILD)/$(SKETCH).insn: $(BUILD)/$(SKETCH).elf scripts/list-instructions.sh
	scripts/list-instructions.sh $(AVR_OBJDUMP) $(AVR_NM) $< $(OBJECTS) > $@

$(SIMAVR_STAMP): $(SIMAVR_PATCH) | $(BUILD)
	rm -rf $(SIMAVR_DIR)
	git init -q $(SIMAVR_DIR)
	git -C $(SIMAVR_DIR) fetch -q --depth 1 $(SIMAVR_REPOSITORY) $(SIMAVR_COMMIT)
	git -C $(SIMAVR_DIR) checkout -q FETCH_HEAD
	git -C $(SIMAVR_DIR) apply "$(CURDIR)/$(SIMAVR_PATCH)"
	$(MAKE) -C $(SIMAVR_DIR)/simavr RELEASE=1 obj config
	$(MAKE) -C $(SIMAVR_DIR)/simavr RELEASE=1 libsimavr
	touch $@

$(BUILD)/test_write_stage: test/test_write_stage.c test/board.c test/board.h $(SIMAVR_STAMP)
	$(HOST_CC) $(HOST_FLAGS) -o $@ test/test_write_stage.c test/board.c \
		$$(ls $(SIMAVR_DIR)/simavr/obj-*/*.o | grep -v '/run_avr\.o$$') $(HOST_LIBS)

size: $(BUILD)/$(SKETCH).elf
	$(AVR_SIZE) $<

test: $(BUILD)/$(SKETCH).elf $(BUILD)/$(SKETCH).insn $(BUILD)/test_write_stage
	$(BUILD)/test_write_stage $(MCU) $(BUILD)/$(SKETCH).elf $(BUILD)/$(SKETCH).insn

fuses:
	$(AVRDUDE) -c $(PROGRAMMER) -P $(PORT) -p $(AVRDUDE_PART) -U lfuse:w:$(LFUSE):m -U hfuse:w:$(HFUSE):m -U efuse:w:$(EFUSE):m

flash: $(BUILD)/$(SKETCH).hex
	$(AVRDUDE) -c $(PROGRAMMER) -P $(PORT) -p $(AVRDUDE_PART) -U flash:w:$<:i

clean:
	rm -rf $(BUILD)
