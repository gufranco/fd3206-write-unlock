AVR_CC ?= avr-gcc
AVR_OBJCOPY ?= avr-objcopy
AVR_OBJDUMP ?= avr-objdump
AVR_NM ?= avr-nm
AVR_SIZE ?= avr-size
AVRDUDE ?= avrdude
HOST_CC ?= cc
PKG_CONFIG ?= pkg-config

TARGETS := attiny2313a atmega328p
BUILD := build
FIRMWARE := src/fdswriteunlock.S
FIRMWARE_SOURCES := $(FIRMWARE) src/pins.h

TINY_PROGRAMMER ?= usbasp
TINY_PORT ?= usb
TINY_LFUSE := 0xE4
TINY_HFUSE := 0xD9
TINY_EFUSE := 0xFF
NANO_PROGRAMMER ?= arduino
NANO_PORT ?= /dev/ttyUSB0
NANO_BAUD ?= 115200

AVR_FLAGS := -x assembler-with-cpp -Wall -Wextra -Werror -Isrc
HOST_FLAGS := -std=c11 -O2 -Wall -Wextra -Werror -Wpedantic $(patsubst -I%,-isystem %,$(shell $(PKG_CONFIG) --cflags simavr libelf))
HOST_LIBS := $(shell $(PKG_CONFIG) --libs simavr libelf)

ELVES := $(TARGETS:%=$(BUILD)/%.elf)
HEXES := $(TARGETS:%=$(BUILD)/%.hex)
LISTINGS := $(TARGETS:%=$(BUILD)/%.insn)

.PHONY: all size test clean fuses-attiny2313a flash-attiny2313a flash-atmega328p

all: $(HEXES)

$(BUILD):
	mkdir -p $@

$(BUILD)/%.elf: $(FIRMWARE_SOURCES) | $(BUILD)
	$(AVR_CC) -mmcu=$* $(AVR_FLAGS) -o $@ $(FIRMWARE)

$(BUILD)/%.hex: $(BUILD)/%.elf
	$(AVR_OBJCOPY) -O ihex -R .eeprom $< $@

$(BUILD)/%.insn: $(BUILD)/%.elf scripts/list-instructions.sh
	scripts/list-instructions.sh $(AVR_OBJDUMP) $(AVR_NM) $< > $@

$(BUILD)/test_write_stage: test/test_write_stage.c test/board.c test/board.h | $(BUILD)
	$(HOST_CC) $(HOST_FLAGS) -o $@ test/test_write_stage.c test/board.c $(HOST_LIBS)

size: $(ELVES)
	$(AVR_SIZE) $(ELVES)

test: $(ELVES) $(LISTINGS) $(BUILD)/test_write_stage
	$(BUILD)/test_write_stage attiny2313a $(BUILD)/attiny2313a.elf $(BUILD)/attiny2313a.insn
	$(BUILD)/test_write_stage atmega328p $(BUILD)/atmega328p.elf $(BUILD)/atmega328p.insn

fuses-attiny2313a:
	$(AVRDUDE) -c $(TINY_PROGRAMMER) -P $(TINY_PORT) -p t2313a -U lfuse:w:$(TINY_LFUSE):m -U hfuse:w:$(TINY_HFUSE):m -U efuse:w:$(TINY_EFUSE):m

flash-attiny2313a: $(BUILD)/attiny2313a.hex
	$(AVRDUDE) -c $(TINY_PROGRAMMER) -P $(TINY_PORT) -p t2313a -U flash:w:$<:i

flash-atmega328p: $(BUILD)/atmega328p.hex
	$(AVRDUDE) -c $(NANO_PROGRAMMER) -P $(NANO_PORT) -b $(NANO_BAUD) -p m328p -U flash:w:$<:i

clean:
	rm -rf $(BUILD)
