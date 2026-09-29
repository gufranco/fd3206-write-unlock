# SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com>
# SPDX-License-Identifier: MIT

MCUS := attiny2313a attiny4313
MCU ?= attiny2313a
AVRDUDE_PART_attiny2313a := t2313a
AVRDUDE_PART_attiny4313 := t4313
AVRDUDE_PART ?= $(AVRDUDE_PART_$(MCU))
F_CPU := 8000000UL
FLASH_BYTES := 2048
EDGE_HANDLER_BUDGET_CYCLES := 33
INTERRUPTS_OFF_BUDGET_CYCLES := 12
SIM_CLOCKS_HZ := 7200000 8000000 8800000
LFUSE := 0xE4
HFUSE := 0xD9
EFUSE := 0xFF
PROGRAMMER ?= usbasp
PORT ?= usb
AVRDUDE ?= avrdude
PYTHON ?= python3

BUILD := build
NAME := fd3206-write-unlock
image_path = $(BUILD)/$(1)/release/$(NAME)-$(1)
RELEASE := $(BUILD)/$(MCU)/release
RELEASE_ELF := $(call image_path,$(MCU)).elf
RELEASE_HEX := $(call image_path,$(MCU)).hex

CONTAINER_TARGETS := all size analyse hosttest simtest test mutation reproducible misra figures

.PHONY: $(CONTAINER_TARGETS) images image fuses flash hooks clean

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
AVR_GCC_INCLUDE := $(shell $(AVR_CC) -print-file-name=include)

FIRMWARE_C := $(sort $(wildcard src/*.c))
FIRMWARE_S := $(sort $(wildcard src/*.S))
FIRMWARE_H := $(sort $(wildcard include/*/*.h))
LOGIC_C := src/conditions.c src/heads.c
HOST_TEST_C := tests/host/host_test.c tests/host/host_assert.c
SIM_TEST_C := tests/sim/sim_test.c tests/sim/board.c
C_FILES := $(FIRMWARE_C) $(FIRMWARE_H) $(HOST_TEST_C) $(SIM_TEST_C) tests/sim/board.h tests/type_widths.c

C_STD := -std=c17 -pedantic-errors
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
INSTRUCTIONS := $(call image_path,$(MCU)).insn
HOST_TEST := $(BUILD)/host/host_test
SIM_TEST := $(BUILD)/sim/sim_test

CPPCHECK_FLAGS := --std=c17 --platform=avr8 --enable=all --check-level=exhaustive --error-exitcode=1 \
	--suppress=checkersReport '--suppress=*:$(AVR_INCLUDE)/*' '--suppress=*:$(AVR_GCC_INCLUDE)/*' \
	-Iinclude -I$(AVR_INCLUDE) -I$(AVR_GCC_INCLUDE) \
	-D__AVR_ATtiny2313A__ -DF_CPU=$(F_CPU)
CPPCHECK_CONFIGS := -DFDSWU_DEBUG -UFDSWU_DEBUG

all: images

images:
	$(foreach mcu,$(MCUS),$(MAKE) --no-print-directory MCU=$(mcu) image &&) true

image: $(RELEASE_HEX) $(INSTRUCTIONS)

$(RELEASE)/%.c.o: src/%.c $(FIRMWARE_H)
	@mkdir -p $(@D)
	$(AVR_CC) $(AVR_CFLAGS) -c -o $@ $<

$(RELEASE)/%.S.o: src/%.S include/port/registers.h
	@mkdir -p $(@D)
	$(AVR_CC) $(AVR_ASFLAGS) -c -o $@ $<

$(RELEASE_ELF): $(RELEASE_OBJECTS)
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

analyse: images
	clang-format --dry-run --Werror $(C_FILES)
	ruff check
	ruff format --check
	$(PYTHON) -m tools.style_gate $(FIRMWARE_C) $(FIRMWARE_S) $(FIRMWARE_H)
	$(PYTHON) -m tools.layer_check .
	reuse lint
	$(AVR_CC) -mmcu=$(MCU) $(C_STD) -fsyntax-only tests/type_widths.c
	$(HOST_CC) $(C_STD) -fsyntax-only tests/type_widths.c
	$(HOST_CC) $(SIM_CFLAGS) -fsigned-char -fsyntax-only $(SIM_TEST_C)
	$(HOST_CC) $(SIM_CFLAGS) -funsigned-char -fsyntax-only $(SIM_TEST_C)
	$(HOST_CC) $(HOST_CFLAGS) -DFDSWU_DEBUG -fsigned-char -fsyntax-only $(LOGIC_C) $(HOST_TEST_C)
	$(HOST_CC) $(HOST_CFLAGS) -DFDSWU_DEBUG -funsigned-char -fsyntax-only $(LOGIC_C) $(HOST_TEST_C)
	$(foreach config,$(CPPCHECK_CONFIGS),cppcheck $(CPPCHECK_FLAGS) $(config) --addon=misra $(FIRMWARE_C) &&) true
	$(PYTHON) -m tools.doc_figures . $(AVR_SIZE) $(AVR_OBJDUMP) $(RELEASE_ELF)
	$(foreach mcu,$(MCUS),$(PYTHON) -m tools.check_hex $(call image_path,$(mcu)).hex --max-bytes $(FLASH_BYTES) &&) true
	$(foreach mcu,$(MCUS),$(PYTHON) -m tools.isr_check $(AVR_OBJDUMP) $(call image_path,$(mcu)).elf \
		$(EDGE_HANDLER_BUDGET_CYCLES) $(INTERRUPTS_OFF_BUDGET_CYCLES) &&) true
	COVERAGE_FILE=$(BUILD)/.coverage $(PYTHON) -m coverage run -m unittest discover -s tests/tools -t .
	COVERAGE_FILE=$(BUILD)/.coverage $(PYTHON) -m coverage report

misra:
	$(foreach config,$(CPPCHECK_CONFIGS),cppcheck $(CPPCHECK_FLAGS) $(config) --addon=misra $(FIRMWARE_C) &&) true

hosttest: $(HOST_TEST)
	rm -f $(BUILD)/host/*.gcda
	$(HOST_TEST)
	gcovr --root . --filter 'src/' --exclude-branches-by-pattern '.*FDSWU_ASSERT.*' \
		--fail-under-line 100 --fail-under-branch 100 --print-summary $(BUILD)/host

simtest: $(SIM_TEST) images
	$(foreach mcu,$(MCUS),$(foreach clock,$(SIM_CLOCKS_HZ),\
		$(SIM_TEST) $(mcu) $(call image_path,$(mcu)).elf $(call image_path,$(mcu)).insn $(clock) &&)) true

test: hosttest simtest

mutation:
	$(PYTHON) -m tools.mutation .

reproducible:
	$(PYTHON) -m tools.reproducible .

endif
