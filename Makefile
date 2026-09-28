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
SOURCES := $(wildcard $(SKETCH)/*.c) $(wildcard $(SKETCH)/*.S)
HEADERS := $(wildcard $(SKETCH)/*.h)
OBJECTS := $(patsubst $(SKETCH)/%.S,$(BUILD)/%.o,$(SOURCES:$(SKETCH)/%.c=$(BUILD)/%.o))

AVR_FLAGS := -mmcu=$(MCU) -DF_CPU=$(F_CPU) -std=gnu11 -Os -Wall -Wextra -Werror -ffunction-sections -fdata-sections
AVR_LINK_FLAGS := -mmcu=$(MCU) -Wl,--gc-sections
HOST_FLAGS := -std=c11 -O2 -Wall -Wextra -Werror -Wpedantic $(patsubst -I%,-isystem %,$(shell $(PKG_CONFIG) --cflags simavr libelf))
HOST_LIBS := $(shell $(PKG_CONFIG) --libs simavr libelf)

.PHONY: all size test clean fuses flash

all: $(BUILD)/$(SKETCH).hex

$(BUILD):
	mkdir -p $@

$(BUILD)/%.o: $(SKETCH)/%.c $(HEADERS) | $(BUILD)
	$(AVR_CC) $(AVR_FLAGS) -c -o $@ $<

$(BUILD)/%.o: $(SKETCH)/%.S | $(BUILD)
	$(AVR_CC) -mmcu=$(MCU) -x assembler-with-cpp -Wall -Wextra -Werror -c -o $@ $<

$(BUILD)/$(SKETCH).elf: $(OBJECTS)
	$(AVR_CC) $(AVR_LINK_FLAGS) -o $@ $(OBJECTS)

$(BUILD)/$(SKETCH).hex: $(BUILD)/$(SKETCH).elf
	$(AVR_OBJCOPY) -O ihex -R .eeprom $< $@

$(BUILD)/$(SKETCH).insn: $(BUILD)/$(SKETCH).elf tools/list_instructions.py
	python3 tools/list_instructions.py $(AVR_OBJDUMP) $(AVR_NM) $< $(OBJECTS) > $@

$(BUILD)/test_write_stage: test/test_write_stage.c test/board.c test/board.h | $(BUILD)
	$(HOST_CC) $(HOST_FLAGS) -o $@ test/test_write_stage.c test/board.c $(HOST_LIBS)

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
