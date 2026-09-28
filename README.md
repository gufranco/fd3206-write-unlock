# fdswriteunlock

ATtiny2313A firmware that sits on top of the Mitsumi FD3206P controller of a Famicom Disk System drive and does the controller's write stage itself, so the drive can rewrite whole disks with nothing cut and nothing else added.

**TL;DR:** program an ATtiny2313A, place it on the FD3206P pin 1 over pin 1, solder its pins 4, 5, 6, 10, 13, 14, 15 and 20 to the pins underneath, and clip the rest. Depending on the drive's power board, that board needs its own modification too; see [Power board](#power-board). The firmware is written in C23 under the NASA Power of 10 rules and builds in a pinned Docker toolchain. It passes host unit tests at 100 percent line and branch coverage and a 13-scenario simulation suite that executes every firmware instruction. It has not yet run in a real drive.

## Why a drive refuses full-disk writes

Drives built from late 1988 use the FD3206P controller in place of the earlier FD7201P. The FD3206P lets the RAM adapter rewrite a single file but releases the write heads when it sees a write of the whole disk surface, and the RAM adapter reports error 26. FD7201P drives do not do this and do not need this firmware.

## Power board

The chip is not always enough. The drive unit's second circuit board, the power board, carries the RAM adapter's signals to the drive mechanism, and revisions FMD-POWER-04 and -05, plus some -02 boards fitted with a green daughterboard, have a write lockout of their own. It blocks rewriting even on FD7201P drives, and no chip on the FD3206P can reach it.

| Power board label | Change needed |
|---|---|
| FMD-POWER-01 | none |
| FMD-POWER-02 | none, unless a green daughterboard is fitted: remove it and move its drive connector to the board |
| FMD-POWER-03 | not documented; presumed like -02 |
| FMD-POWER-04 | remove JP14 and join points A and B with a wire |
| FMD-POWER-05 | remove two jumper wires, cut two traces and add two wire links |

Read the `FMD-POWER-XX` label before installing the chip. The steps are in [`docs/hardware.md`](docs/hardware.md); the photos marking each point are in the Famicom World article [FDS Power Board Modifications](https://famicomworld.com/workshop/tech/fds-power-board-modifications/). The no-cut rule of this project covers the drive mechanism; the power board change is a prerequisite outside it.

## What the firmware does

The same logic as the classic write mod: a flip-flop that changes state on every falling edge of WRITE DATA, and a gate that lets one head be driven only while /READY, /WRITE PROTECT and /WRITE GATE are all low.

- **Edge interrupt.** WRITE DATA lands on pin 6, which is the ATtiny's INT0. A 15-instruction assembly handler writes the next head state to the port first, then swaps two precomputed values ready for the edge after. It touches no flags and no C state.
- **Gate.** The main loop reads the three conditions. When they change it recomputes the two values the handler swaps, with interrupts off for the few instructions that takes. Otherwise it only services the watchdog.
- **Pull low, never drive high.** A head pin is either an output at 0 or an input. The drive's own pull-up resistors hold a released head high, which is what the original circuit's open-collector decoder did. The FD3206P shares these two pins, and a pin that only ever pulls low cannot short against it.
- **Watchdog.** 60 ms. A reset leaves every pin an input, which releases both heads.

Timing at 8 MHz:

| Measure | Value | Source |
|---|---|---|
| Interrupt taken to head written | 10 cycles, 1.25 us | instruction count of the handler |
| Whole handler | about 30 cycles, 3.75 us | instruction count, under the 4.7 us shortest edge spacing |
| Variation between edges | up to 2 cycles, 250 ns, from the instruction in progress | AVR interrupt response |
| Gate change to heads released or engaged | under 10 us | simulation limit |

The drive records at 96.4 kHz, a bit cell of 10.4 us. 250 ns of variation is 2.4 percent of a cell. A gate change falls inside a gap between blocks of at least 480 bits, so microseconds of gate delay only trim or extend that gap.

## One thing it does not handle

On single-file saves the FD3206P still writes, on the same two pins, driven from its own flip-flop. If the two flip-flops disagree, both heads are pulled low at once. The classic GAL modchip that sits on this chip has the same property and drives its pins high as well; this firmware at least cannot short against the controller. Whether saves suffer is settled only on a drive. The fix that removes it for certain is the two trace cuts of the classic wired mod, which this project deliberately does not require.

## Pins

| ATtiny2313A pin | FD3206P signal | Action |
|---|---|---|
| 4, PA1 | /WRITE GATE | solder |
| 5, PA0 | /WRITE PROTECT | solder |
| 6, PD2 INT0 | WRITE DATA | solder |
| 10 | GND | solder |
| 13, PB1 | /READY | solder |
| 14, PB2 | Head 2 | solder |
| 15, PB3 | Head 1 | solder |
| 20 | +5 V | solder |
| 1, 2, 3, 7, 8, 9, 11, 12, 16, 17, 18, 19 | unknown | clip so they touch nothing |

The ATtiny4313 has the same pinout and runs the same firmware. Install details are in [`docs/hardware.md`](docs/hardware.md). Check the power board first; see [Power board](#power-board).

## Figures

<!-- figures:begin -->
| Figure | Value |
|---|---|
| Flash used | 212 bytes |
| Edge handler | 15 instructions |
| Firmware source | 154 non-blank lines |
<!-- figures:end -->

`make analyse` fails when this table no longer matches the build; `make figures` rewrites it.

## Build

The only requirement on the host is Docker and Python 3. Every build, check and test runs inside a toolchain image pinned by digest: avr-gcc 14.2, avr-libc 2.2.1, simavr 1.6, cppcheck 2.22, clang-format 19. The image tag is a hash of [`Dockerfile`](Dockerfile) and [`docker/requirements-tools.txt`](docker/requirements-tools.txt), so a changed toolchain builds a new image.

```sh
make            # release hex and debug ELF
make analyse    # format, lint, style gate, layer check, type widths, cppcheck, figures, tool tests
make test       # host unit tests with coverage, then the simavr suite
make misra      # advisory MISRA C report
make hooks      # run analyse and test before every commit
```

## Program

Programming runs on the host with `avrdude`, installed with `brew install avrdude` on macOS or `apt install avrdude` on Debian.

```sh
make fuses PROGRAMMER=usbasp
make flash PROGRAMMER=usbasp
```

To use an Arduino Uno or Nano as the programmer, load the ArduinoISP example onto it from the Arduino IDE, wire it to the ATtiny2313A, and pass `PROGRAMMER=arduino_as_isp PORT=/dev/cu.usbmodemXXXX`. The Arduino IDE only programs the Arduino; the ATtiny firmware is never built there, because the AVR compiler the IDE ships predates C23.

The fuses are low `0xE4`, 8 MHz internal oscillator with no clock output, and high `0xD9`, brown-out reset at 4.3 V with programming left enabled. A new chip runs its 4 MHz oscillator divided by 8; the firmware sets the prescaler to 1 at start-up, so an unfused chip still works at 4 MHz, with twice the edge variation and no brown-out protection.

## Tests

Three tiers, all run by `make test` and `make analyse`:

| Tier | What it runs | Gate |
|---|---|---|
| Host | [`tests/host/host_test.c`](tests/host/host_test.c) against the pure logic in `src/heads.c` and `src/conditions.c`, with assertions on | 100 percent line and branch coverage |
| Simulation | [`tests/sim/sim_test.c`](tests/sim/sim_test.c) runs the release ELF in simavr | 13 scenarios, every firmware instruction executed |
| Tools | [`tests/tools/`](tests/tools) against the Python gates | 100 percent line and branch coverage |

## Provenance

Written independently from public documentation. [`docs/clean-room.md`](docs/clean-room.md) lists the sources and the independence checks.

## License

MIT. See [`LICENSE`](LICENSE).
