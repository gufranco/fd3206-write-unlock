# fdswriteunlock

ATtiny2313A firmware that takes over the write stage of a Famicom Disk System drive built on the Mitsumi FD3206P controller, so the drive can rewrite whole disks, with the chip as the only added part.

**TL;DR:** program an ATtiny2313A, cut two traces on the drive board, and solder eight wires from the chip to the board. Nothing else is added. The chip's timer switches the write heads in hardware on every data edge. The firmware passes a 13-scenario simulation suite that executes every firmware instruction. It has not yet run in a real drive.

## Why a drive refuses full-disk writes

Drives built from late 1988 use the FD3206P controller in place of the earlier FD7201P. The FD3206P lets the RAM adapter rewrite a single file but disconnects the write heads when it sees a write of the whole disk surface, and the RAM adapter reports error 26. FD7201P drives do not do this and do not need this firmware.

The ATtiny replaces the controller's write stage. Two cut traces separate the FD3206P from the write heads, and the ATtiny drives the heads from the same signals the controller receives.

## How it works

- **Head toggle, in the timer.** WRITE DATA clocks Timer0 through its T0 pin. The timer runs in clear-on-match mode with both compare values at 0, so every falling edge is a match on both compare units, and each unit toggles its output pin. A forced compare at start-up sets the two outputs to opposite levels, so exactly one head line is low at any time. No instruction runs per data edge.
- **Write gating, in an interrupt.** A pin-change interrupt fires when /WRITE GATE, /WRITE PROTECT or /READY changes. The head pins are outputs only while all three are low. Otherwise they are inputs, and the drive's own pull-up resistors release the heads. The main loop repeats the same check continuously as a safety net.
- **Supervision.** A 60 ms watchdog resets the chip if the main loop stops, and a reset leaves every pin as an input. The 4.3 V brown-out fuse holds the chip in reset while the supply is low, at power-up and power-down.

| Measure | Value | Source |
|---|---|---|
| Data edge to head switch | a few CPU cycles, with one cycle, 125 ns, of variation | ATtiny2313A timer external clock synchroniser |
| Gate change to heads released or engaged | 5.75 us | simulation |
| Shortest data edge spacing handled | 4.7 us, the 10 percent fast limit | simulation, 1000 edges |

The drive records at 96.4 kHz, so a bit cell is 10.4 us. One cycle of variation is 1.2 percent of a cell. A gate change always falls inside a gap between blocks, at least 480 bits long, so a few microseconds of gate delay only trims or extends that gap.

## Pins

| ATtiny2313A pin | Function | Connects to |
|---|---|---|
| 8, PD4 T0 | WRITE DATA | FD3206P pin 6 |
| 12, PB0 | /WRITE GATE | FD3206P pin 4 |
| 13, PB1 | /WRITE PROTECT | FD3206P pin 5 |
| 15, PB3 | /READY | FD3206P pin 13 |
| 14, PB2 OC0A | Head 1 | Head 1 line, head side of the cut beside FD3206P pin 15 |
| 9, PD5 OC0B | Head 2 | Head 2 line, head side of the cut beside FD3206P pin 14 |
| 20, VCC | +5 V | FD3206P pin 20 |
| 10, GND | Ground | FD3206P pin 10 |

Every other pin is unused and has its internal pull-up enabled. Pins 1, 17, 18 and 19 are the programming pins, so the chip can be reprogrammed after installation from a clip or header.

The ATtiny4313 has the same pinout and runs the same firmware.

## Program with the Arduino IDE

1. Load the ArduinoISP example onto an Arduino Uno or Nano and wire it to the ATtiny2313A as an ISP programmer.
2. In the Arduino IDE preferences, add `http://drazzy.com/package_drazzy.com_index.json` to Additional Boards Manager URLs, then install ATTinyCore from the Boards Manager.
3. Open the `fdswriteunlock` folder of this repository as a sketch.
4. Under Tools, select the board `ATtiny4313/2313 (No Bootloader)`, chip `ATtiny2313/ATtiny2313A`, clock `8 MHz (internal)`, and `B.O.D. Enabled (4.3v)`. Set Programmer to `Arduino as ISP`.
5. Run Tools, Burn Bootloader once. The part has no bootloader, so this only writes the clock and brown-out fuses.
6. Run Sketch, Upload Using Programmer.

The sketch file is empty on purpose: the firmware defines its own `main`, so the Arduino core's `setup` and `loop` are never linked. This route follows ATTinyCore's documented menus but has not yet been test-built here with arduino-cli.

## Program from the command line

```sh
make
make fuses PROGRAMMER=usbasp
make flash PROGRAMMER=usbasp
```

For an Arduino running ArduinoISP, pass `PROGRAMMER=arduino_as_isp PORT=/dev/cu.usbmodemXXXX`. The fuses are low `0xE4`, 8 MHz internal oscillator with no clock output, and high `0xD9`, brown-out reset at 4.3 V with programming left enabled. A new chip runs its 4 MHz oscillator divided by 8. The firmware sets the clock prescaler to 1 at start-up, so an unfused chip still works at 4 MHz, with 250 ns of edge variation instead of 125 ns and no brown-out protection.

## Install

See [`docs/hardware.md`](docs/hardware.md).

## Tests

```sh
make test
```

The suite runs the real firmware in simavr and fails if any firmware instruction never executes. It needs `avr-gcc`, `avr-libc`, `libelf` and `git`.

simavr at the pinned commit gets three things about this timer wrong compared with the datasheet, so `make test` builds it from source with [`test/simavr-datasheet-fixes.patch`](test/simavr-datasheet-fixes.patch) applied:

- It ignores external clock edges when the timer's TOP is 0. In clear-on-match mode with OCR0A at 0 the chip toggles on every clock.
- After compare A matches in clear-on-match mode, it resets the counter before checking compare B, so compare B never matches.
- It does not wire the force-compare bits of the ATtiny2313A, and would register the handler twice where both bits share one register.

What simulation does not show: simavr updates the timer at the instant the pin changes, without the synchroniser delay, and its outputs start one edge later than on the chip. Neither changes the result, since every edge still moves the low level to the other head. A logic analyzer on the board is the check for real timing.

## Provenance

Written independently from public documentation. [`docs/clean-room.md`](docs/clean-room.md) lists the sources and the independence checks.

## License

MIT. See [`LICENSE`](LICENSE). The one exception is `test/simavr-datasheet-fixes.patch`, a change to simavr, which is GPL-3.0-or-later like simavr itself.
