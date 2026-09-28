MCU := attiny2313a
AVRDUDE_PART := t2313a
F_CPU := 8000000UL
LFUSE := 0xE4
HFUSE := 0xD9
EFUSE := 0xFF
PROGRAMMER ?= usbasp
PORT ?= usb
AVRDUDE ?= avrdude
PYTHON ?= python3

BUILD := build
NAME := fdswriteunlock
RELEASE := $(BUILD)/release
DEBUG := $(BUILD)/debug
RELEASE_ELF := $(RELEASE)/$(NAME).elf
RELEASE_HEX := $(RELEASE)/$(NAME).hex
DEBUG_ELF := $(DEBUG)/$(NAME).elf

CONTAINER_TARGETS := all size analyse hosttest simtest test misra figures

.PHONY: $(CONTAINER_TARGETS) fuses flash hooks clean

clean:
	rm -rf $(BUILD)

ifndef FDSWU_TOOLCHAIN

$(CONTAINER_TARGETS):
	$(PYTHON) tools/docker_make.py $@

fuses:
	$(AVRDUDE) -c $(PROGRAMMER) -P $(PORT) -p $(AVRDUDE_PART) -U lfuse:w:$(LFUSE):m -U hfuse:w:$(HFUSE):m -U efuse:w:$(EFUSE):m

flash: all
	$(AVRDUDE) -c $(PROGRAMMER) -P $(PORT) -p $(AVRDUDE_PART) -U flash:w:$(RELEASE_HEX):i

hooks:
	git config core.hooksPath .githooks

else

AVR_CC := avr-gcc
AVR_OBJCOPY := avr-objcopy
AVR_OBJDUMP := avr-objdump
AVR_NM := avr-nm
AVR_SIZE := avr-size
HOST_CC := gcc
AVR_INCLUDE := /usr/lib/avr/include

FIRMWARE_C := $(sort $(wildcard src/*.c))
FIRMWARE_S := $(sort $(wildcard src/*.S))
FIRMWARE_H := $(sort $(wildcard include/*/*.h))
LOGIC_C := src/conditions.c src/heads.c
HOST_TEST_C := tests/host/host_test.c tests/host/host_assert.c
SIM_TEST_C := tests/sim/sim_test.c tests/sim/board.c
C_FILES := $(FIRMWARE_C) $(FIRMWARE_H) $(HOST_TEST_C) $(SIM_TEST_C) tests/sim/board.h tests/type_widths.c

C_STD := -std=c23 -pedantic-errors
WARNINGS := -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wshadow -Wstrict-prototypes \
	-Wmissing-prototypes -Wundef -Wcast-qual -Wswitch-enum -Wswitch-default -Wdouble-promotion \
	-Wnull-dereference -Wvla -Wredundant-decls -Wformat=2
AVR_CFLAGS := -mmcu=$(MCU) -DF_CPU=$(F_CPU) $(C_STD) -Os -flto -ffat-lto-objects -Iinclude $(WARNINGS) \
	-fno-common -ffunction-sections -fdata-sections
AVR_ASFLAGS := -mmcu=$(MCU) -x assembler-with-cpp -Iinclude -Wall -Wextra -Werror
AVR_LDFLAGS := -mmcu=$(MCU) -Os -flto -Wl,--gc-sections
HOST_CFLAGS := $(C_STD) -Iinclude $(WARNINGS)
SIM_CFLAGS := $(C_STD) -O2 $(WARNINGS) $(patsubst -I%,-isystem %,$(shell pkg-config --cflags simavr libelf))
SIM_LIBS := $(shell pkg-config --libs simavr libelf)

RELEASE_OBJECTS := $(patsubst src/%,$(RELEASE)/%.o,$(FIRMWARE_C) $(FIRMWARE_S))
DEBUG_OBJECTS := $(patsubst src/%,$(DEBUG)/%.o,$(FIRMWARE_C) $(FIRMWARE_S))
INSTRUCTIONS := $(RELEASE)/$(NAME).insn
HOST_TEST := $(BUILD)/host/host_test
SIM_TEST := $(BUILD)/sim/sim_test

CPPCHECK_FLAGS := --std=c23 --platform=avr8 --enable=all --check-level=exhaustive --error-exitcode=1 \
	--suppress=checkersReport '--suppress=*:$(AVR_INCLUDE)/*' -Iinclude -I$(AVR_INCLUDE) \
	-D__AVR_ATtiny2313A__ -DF_CPU=$(F_CPU) -DFDSWU_DEBUG

all: $(RELEASE_HEX) $(DEBUG_ELF)

$(RELEASE)/%.c.o: src/%.c $(FIRMWARE_H)
	@mkdir -p $(@D)
	$(AVR_CC) $(AVR_CFLAGS) -c -o $@ $<

$(DEBUG)/%.c.o: src/%.c $(FIRMWARE_H)
	@mkdir -p $(@D)
	$(AVR_CC) $(AVR_CFLAGS) -DFDSWU_DEBUG -c -o $@ $<

$(RELEASE)/%.S.o: src/%.S include/port/registers.h
	@mkdir -p $(@D)
	$(AVR_CC) $(AVR_ASFLAGS) -c -o $@ $<

$(DEBUG)/%.S.o: src/%.S include/port/registers.h
	@mkdir -p $(@D)
	$(AVR_CC) $(AVR_ASFLAGS) -c -o $@ $<

$(RELEASE_ELF): $(RELEASE_OBJECTS)
	$(AVR_CC) $(AVR_LDFLAGS) -o $@ $^

$(DEBUG_ELF): $(DEBUG_OBJECTS)
	$(AVR_CC) $(AVR_LDFLAGS) -o $@ $^

$(RELEASE_HEX): $(RELEASE_ELF)
	$(AVR_OBJCOPY) -O ihex -R .eeprom $< $@

$(INSTRUCTIONS): $(RELEASE_ELF) tools/list_instructions.py
	$(PYTHON) -m tools.list_instructions $(AVR_OBJDUMP) $(AVR_NM) $< $(RELEASE_OBJECTS) > $@

$(HOST_TEST): $(LOGIC_C) $(HOST_TEST_C) $(FIRMWARE_H)
	@mkdir -p $(@D)
	$(HOST_CC) $(HOST_CFLAGS) -O0 -DFDSWU_DEBUG --coverage -o $@ $(LOGIC_C) $(HOST_TEST_C)

$(SIM_TEST): $(SIM_TEST_C) tests/sim/board.h
	@mkdir -p $(@D)
	$(HOST_CC) $(SIM_CFLAGS) -o $@ $(SIM_TEST_C) $(SIM_LIBS)

size: $(RELEASE_ELF)
	$(AVR_SIZE) $<

figures: $(RELEASE_ELF)
	$(PYTHON) -m tools.doc_figures . $(AVR_SIZE) $(AVR_OBJDUMP) $< --update

analyse: $(RELEASE_ELF) $(DEBUG_ELF)
	clang-format --dry-run --Werror $(C_FILES)
	ruff check
	ruff format --check
	$(PYTHON) -m tools.style_gate $(FIRMWARE_C) $(FIRMWARE_S) $(FIRMWARE_H)
	$(PYTHON) -m tools.layer_check .
	$(AVR_CC) -mmcu=$(MCU) $(C_STD) -fsyntax-only tests/type_widths.c
	$(HOST_CC) $(C_STD) -fsyntax-only tests/type_widths.c
	cppcheck $(CPPCHECK_FLAGS) $(FIRMWARE_C)
	$(PYTHON) -m tools.doc_figures . $(AVR_SIZE) $(AVR_OBJDUMP) $(RELEASE_ELF)
	COVERAGE_FILE=$(BUILD)/.coverage $(PYTHON) -m coverage run -m unittest discover -s tests/tools -t .
	COVERAGE_FILE=$(BUILD)/.coverage $(PYTHON) -m coverage report

misra:
	cppcheck $(CPPCHECK_FLAGS) --error-exitcode=0 --addon=misra $(FIRMWARE_C)

hosttest: $(HOST_TEST)
	rm -f $(BUILD)/host/*.gcda
	$(HOST_TEST)
	gcovr --root . --filter 'src/' --exclude-branches-by-pattern '.*FDSWU_ASSERT.*' \
		--fail-under-line 100 --fail-under-branch 100 --print-summary $(BUILD)/host

simtest: $(SIM_TEST) $(RELEASE_ELF) $(INSTRUCTIONS)
	$(SIM_TEST) $(MCU) $(RELEASE_ELF) $(INSTRUCTIONS)

test: hosttest simtest

endif
