# fdswriteunlock

Firmware that lets a Famicom Disk System drive with the Mitsumi FD3206P controller rewrite whole disks, running on a US$2 ATtiny2313A soldered on top of the controller with no extra wires.

**TL;DR:** program an ATtiny2313A with `make fuses-attiny2313a flash-attiny2313a`, solder its pins 4, 5, 6, 10, 13, 14, 15 and 20 onto the matching FD3206P pins, and clip the rest. It passes a 14-scenario simulation suite on both targets; it has not been tested in a real drive yet.

## Why a drive refuses full-disk writes

Drives built from late 1988 use the FD3206P controller in place of the earlier FD7201P. The FD3206P lets the RAM adapter rewrite a single file but cuts the write heads off when it sees a write of the whole disk surface, and the RAM adapter reports error 26. FD7201P drives do not do this and do not need this firmware.

This firmware does the job of the controller's write stage outside the chip. It watches the same signals the controller receives and drives the two write-head lines itself.

## Behaviour

- Each falling edge of WRITE DATA flips an internal state bit, whether or not a write is in progress.
- While /READY, /WRITE PROTECT and /WRITE GATE are all low, the head matching the state bit is pulled low: head 1 for a clear bit, head 2 for a set bit.
- Otherwise both head lines float on the drive's own pull-up resistors.
- Head lines are only ever pulled low or released. They are never driven high.
- A 16 ms watchdog and a 4.3 V brown-out reset release the heads if the firmware stalls or the supply drops.

The full requirement list with test scenarios is in [`docs/requirements.md`](docs/requirements.md).

## Timing

The drive records at 96.4 kHz, a 10.4 us bit cell, and tolerates about 10 percent speed error. At 8 MHz the ATtiny2313A answers a data edge in 8 to 10 clock cycles: 4 for interrupt entry, 2 for the vector jump, 1 for the port write, up to 1 finishing the current instruction and 1 to 2 for input synchronisation. That is 1.0 to 1.25 us, with at most 375 ns of variation, or 3.6 percent of a bit cell. The ATmega328P build at 16 MHz halves both figures.

## FD3206P pins used

| Pin | Signal | ATtiny2313A pin function |
|---|---|---|
| 4 | /WRITE GATE | PA1 input |
| 5 | /WRITE PROTECT | PA0 input |
| 6 | WRITE DATA | PD2, external interrupt 0 |
| 10 | GND | GND |
| 13 | /READY | PB1 input |
| 14 | HEAD 2 | PB2, open-drain output |
| 15 | HEAD 1 | PB3, open-drain output |
| 20 | +5V | VCC |

The function of the other FD3206P pins is not known. The firmware leaves every other ATtiny pin as an input with its pull-up on, and those legs are clipped.

## Build

macOS:

```sh
brew tap osx-cross/avr
brew install osx-cross/avr/avr-gcc osx-cross/avr/simavr avrdude
```

Debian or Ubuntu:

```sh
sudo apt install gcc-avr avr-libc binutils-avr avrdude libsimavr-dev libelf-dev pkg-config make
```

Then:

```sh
make all
make test
```

`make all` writes `build/attiny2313a.hex` and `build/atmega328p.hex`.

## Program the ATtiny2313A

```sh
make fuses-attiny2313a
make flash-attiny2313a
```

Writing the fuses is required. A new ATtiny2313A runs from its 4 MHz oscillator divided by 8, and the timing above assumes 8 MHz. The fuse target sets low fuse `0xE4`, 8 MHz internal oscillator with no clock output on pin 6, and high fuse `0xD9`, brown-out reset at 4.3 V with in-system programming left on.

The default programmer is a USBasp. For an Arduino running the ArduinoISP sketch, pass `TINY_PROGRAMMER=arduino_as_isp TINY_PORT=/dev/cu.usbmodemXXXX` to both commands.

## Install

1. Open the drive and remove the bottom plate of the mechanism to reach the controller board.
2. Find the 20-pin FD3206P. Place the programmed ATtiny2313A on top of it with pin 1 over pin 1. Both notches face the same way.
3. Solder ATtiny pins 4, 5, 6, 10, 13, 14, 15 and 20 to the FD3206P pins underneath.
4. Clip ATtiny pins 1, 2, 3, 7, 8, 9, 11, 12, 16, 17, 18 and 19 short so they touch nothing.
5. Put insulating tape on the bottom plate under the chips before closing the mechanism.

Before step 3, measure the pull-up resistor on each head line. With the head pulled low, the current is 5 V divided by that resistance, and it must stay under the ATtiny's 20 mA per-pin rating.

## Arduino Nano or Pro Mini

The same source builds for a 5 V ATmega328P board, connected with eight wires.

| Board pin | FD3206P pin | Signal |
|---|---|---|
| D2 | 6 | WRITE DATA |
| D8 | 5 | /WRITE PROTECT |
| D9 | 13 | /READY |
| D10 | 14 | HEAD 2 |
| D11 | 15 | HEAD 1 |
| D12 | 4 | /WRITE GATE |
| 5V | 20 | +5V |
| GND | 10 | GND |

Flash it through the bootloader with `make flash-atmega328p NANO_PORT=/dev/cu.usbserial-XXXX`. Boards with the old bootloader need `NANO_BAUD=57600`.

## Testing on hardware

Use a disk you do not care about.

1. Load a game and save. A save goes through both the FD3206P and the firmware.
2. Rewrite a whole disk with a disk-writing tool and read it back several times.
3. With a logic analyzer, compare WRITE DATA on pin 6 against pins 14 and 15, and confirm the delay is near 1 us.

If saves fail after the install, the FD3206P and the firmware may be pulling opposite heads during single-file writes. Clip FD3206P legs 14 and 15 at the package body, leaving the stubs soldered to the board, and solder the ATtiny pins to the stubs. The firmware then drives the heads alone.

Some drive power boards also block writes. Those boards need their own modification; this firmware does not change them.

## Tests

`make test` runs the firmware in the simavr simulator for both targets and checks every requirement. It fails if any firmware instruction never executes. simavr does not reproduce interrupt entry cycle by cycle, so the timing figures come from the datasheet count and need a logic analyzer to confirm on hardware.

## Provenance

Written independently from public hardware documentation. [`docs/clean-room.md`](docs/clean-room.md) lists the sources and the independence checks.

## License

MIT. See [`LICENSE`](LICENSE).
