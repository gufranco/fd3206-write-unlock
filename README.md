# fdswriteunlock

English | [日本語](README.ja.md) | [简体中文](README.zh-CN.md) | [繁體中文（香港）](README.zh-HK.md)

ATtiny2313A firmware that sits on top of the Mitsumi FD3206P controller of a Famicom Disk System drive and does the controller's write stage itself, so the drive can rewrite whole disks with nothing cut on the drive mechanism and nothing else added.

**TL;DR:** a drive can have two write lockouts. The FD3206P controller is fixed by this chip: program an ATtiny2313A, place it on the FD3206P pin 1 over pin 1, solder its pins 4, 5, 6, 10, 13, 14, 15 and 20 to the pins underneath, and clip the rest. The power board is fixed separately, and only on the revisions that have a lockout: FMD-POWER-04, -05, some -02 boards and the Sharp Twin Famicom AN-500. The firmware is C17 with zero MISRA C:2012 deviations, follows the NASA Power of 10 rules, keeps every register access in one small assembly module, and is built in a pinned Docker toolchain, with 100 percent host line and branch coverage and a 13-scenario simulation suite that executes every firmware instruction. It has not yet run in a real drive.

## Why a drive refuses full-disk writes

Drives built from late 1988 use the FD3206P controller in place of the earlier FD7201P. The FD3206P lets the RAM adapter rewrite a single file but releases the write heads when it sees a write of the whole disk surface, and the RAM adapter reports error 26. FD7201P drives do not do this and do not need this chip.

## Two boards, two lockouts

The drive unit, the HVC-022 or the drive inside a Sharp Twin Famicom, holds two boards:

| Board | What it does | Lockout | Fixed by |
|---|---|---|---|
| Drive mechanism, with the FD3206P or FD7201P controller | reads and writes the disk | FD3206P releases the heads on a whole-disk write; FD7201P has none | this chip, on FD3206P drives only |
| Power board | switches battery or adapter power to the motor and carries every signal between the RAM adapter connector and the drive mechanism | on FMD-POWER-04, -05, some -02 boards and the Twin Famicom AN-500, a circuit that blocks the write signal | the power board change in step 2 |

A drive writes whole disks only when both lockouts are gone. An FD7201P drive needs no chip but can still need the power board change. An FD3206P drive with an FMD-POWER-01 board needs only the chip. The no-cut rule of this project covers the drive mechanism; the power board's circuit sits upstream on a different board, where no chip on the FD3206P can reach it.

## Step 1: identify the power board

1. Remove the six Phillips screws on the bottom of the unit.
2. Flip the unit and lift the top cover off carefully.
3. Remove the two Phillips screws holding the battery compartment and set it aside.
4. Remove the screws holding the power board and lift it out.
5. Turn it component side up and find the text `©198X Nintendo` and the code `FMD-POWER-XX`.

On a Sharp Twin Famicom skip the table and go to step 2d.

| Label | Protected | What to do |
|---|---|---|
| FMD-POWER-01 | no | nothing |
| FMD-POWER-02 without a green daughterboard | no | nothing |
| FMD-POWER-02 with a green daughterboard | yes, on the daughterboard | step 2a |
| FMD-POWER-03 | not documented; presumed like -02 | look for a daughterboard; if there is none and whole-disk writes still fail with the chip installed, suspect the board |
| FMD-POWER-04 | yes | step 2b |
| FMD-POWER-05 | yes | step 2c |

## Step 2: remove the power board lockout

The Famicom World article [FDS Power Board Modifications](https://famicomworld.com/workshop/tech/fds-power-board-modifications/) has a photo of each board with the exact points marked. The steps below say what each change is; the photos show where it lands on your board.

### 2a. FMD-POWER-02 with daughterboard

1. Desolder the green daughterboard and remove it.
2. Clear the solder from the holes it leaves.
3. Desolder the drive connector from the daughterboard.
4. Fit that connector in the power board's own holes and solder it.

The board then matches the unprotected -02.

### 2b. FMD-POWER-04

1. Remove the part labelled JP14, by desoldering it or cutting it out. This disables the protection circuit.
2. Join the points the article marks A and B with a short wire, or with a solder bridge if nothing else is close. This routes the write signal around the disabled circuit to the drive mechanism.

No trace is cut on this board.

### 2c. FMD-POWER-05

1. Desolder the two jumper wires the article marks. One sits under the two black rectangular parts near the RAM adapter connection; bend those parts outward a little to reach it.
2. Cut the two traces the article marks in red. Check with a meter that no connection remains across either cut.
3. Solder the two wire links the article marks in blue. They route the write signals back to the drive mechanism.

### 2d. Sharp Twin Famicom

The Twin Famicom has the same Mitsumi drive mechanism, with an FD7201P or an FD3206P, but its own Sharp power board, so the FMD-POWER table does not apply. Its lockout is removed with two wires instead of a board change.

1. Open the Twin Famicom and find the power board that the drive mechanism's cable runs through.
2. Find the two grey wires: one goes into the power board and the other comes out of it.
3. Disconnect or cut both from the power board.
4. Join the two grey wires to each other, solder the joint and insulate it with heat-shrink or tape. The write signal now runs straight past the board's protection circuit.

Wire colours are not guaranteed to match on every unit; trace both wires to the power board before cutting anything. Reports on the nesdev forum `(as of 2026-09)`:

| Model | Report |
|---|---|
| Twin Famicom, model not stated | Chris Covell reports the two-wire change worked, with a photo, 2014 |
| AN-500B, FD7201P drive | two owners, in 2014 and 2018, were told the two-wire change is the only one needed; neither posted a result |
| any Twin with an FD3206P drive | the FD3206P lockout remains after the two-wire change, so it also needs the chip |
| AN-505 | a post from 2026-09-03 relays a Discord discussion saying this model has no protection; not confirmed by a photo or a test |

## Step 3: program the chip

Every GitHub release attaches `fdswriteunlock.hex`, the exact image the pipeline built and tested, with its SHA-256. Check it with `sha256sum -c fdswriteunlock.hex.sha256`, write the fuses with `make fuses` and flash it with `avrdude -c usbasp -p t2313a -U flash:w:fdswriteunlock.hex:i`, or build the same image yourself as below.

Every build runs in Docker, so the host needs only Docker and Python 3. Programming runs on the host with `avrdude`, installed with `brew install avrdude` on macOS or `apt install avrdude` on Debian.

```sh
make
make fuses PROGRAMMER=usbasp
make flash PROGRAMMER=usbasp
```

To use an Arduino Uno or Nano as the programmer, load the ArduinoISP example onto it from the Arduino IDE, wire it to the ATtiny2313A, and pass `PROGRAMMER=arduino_as_isp PORT=/dev/cu.usbmodemXXXX`. The Arduino IDE only programs the Arduino; the ATtiny firmware is never built there, so every chip receives the image that passed the MISRA check, the tests and the simulation. Each release attaches that image, built by the release pipeline.

The fuses are low `0xE4`, 8 MHz internal oscillator with no clock output, and high `0xD9`, brown-out reset at 4.3 V with programming left enabled. A new chip runs its 4 MHz oscillator divided by 8; the firmware sets the prescaler to 1 at start-up, so an unfused chip still works at 4 MHz, with twice the edge variation and no brown-out protection.

## Step 4: install the chip, FD3206P drives only

The only part added to the drive mechanism is the programmed ATtiny2313A, soldered on top of the FD3206P. Nothing on that board is cut and no wire is added. The ATtiny4313 has the same pinout and runs the same firmware.

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
| 1, 2, 3, 7, 8, 9, 11, 12, 16, 17, 18, 19 | not documented | clip so they touch nothing |

1. Bend or clip the pins marked "clip" so they cannot touch the FD3206P. Their counterparts are signals nobody has documented, and the firmware never needs them.
2. Lift out the drive mechanism and remove its bottom plate to reach the controller board.
3. Place the ATtiny on the FD3206P with pin 1 over pin 1. Both notches face the same way.
4. Solder the eight pins marked "solder" to the FD3206P pins underneath.
5. Refit the bottom plate with tape over the area above the chips.

Before the first write, measure the two head pull-up resistors. With a head pulled low the ATtiny sinks 5 V divided by that resistance, which must stay under its 20 mA per-pin rating, meaning a pull-up of 250 Ohm or more. The FD3206P shares the load while it drives the same head.

## Step 5: test on the drive

Use a disk you do not care about.

1. Save a game. This is the case the FD3206P also writes; see [One thing it does not handle](#one-thing-it-does-not-handle).
2. Rewrite a whole disk with a disk-writing tool, then read it back several times.
3. With a logic analyzer, compare WRITE DATA on pin 6 with pins 14 and 15. Each falling edge moves the low level from one pin to the other.

If whole-disk writes still fail, check that WRITE DATA and /WRITE GATE reach FD3206P pins 6 and 4 during the write. If they do not, the power board is still blocking the write signal upstream of the chip.

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

## Behaviour

Each requirement is verified by a simulation scenario that runs the release firmware image. The gating and head selection are also checked for all 65,536 input port combinations.

| Requirement | Scenario |
|---|---|
| Both head pins MUST be inputs unless /READY, /WRITE PROTECT and /WRITE GATE are all low | one condition high, 100 falling edges: both heads released after every edge |
| While writing, exactly one head pin MUST be low, and it MUST change on every falling edge of WRITE DATA | 1000 edges 4.7 us apart: one head low after every edge, the other one each time |
| A rising edge of WRITE DATA MUST NOT change the heads | 1 us low pulse: the heads change once, at the fall |
| The heads MUST be released within 10 us of any condition going high | each condition in turn, no further edges: both released within 10 us |
| Exactly one head MUST be driven within 10 us of all conditions becoming low | /WRITE GATE falls with the others low: one head low within 10 us |
| The heads MUST end released after a write gate glitch | 250 ns gate pulse: both released afterwards |
| After start-up no pin SHALL be an output, and no pin soldered to the FD3206P SHALL have a pull-up | all inputs high: every direction register zero, no pull-up on a soldered pin |
| The firmware MUST run undivided from its clock and keep the watchdog armed | after start-up: prescaler 1, watchdog enabled |
| The watchdog MUST NOT fire during normal operation | 600 ms of edges, ten times the timeout: no reset, one head low after every edge |

## One thing it does not handle

On single-file saves the FD3206P still writes, on the same two pins, driven from its own flip-flop. If the two flip-flops disagree, both heads are pulled low at once. The classic GAL modchip that sits on this chip has the same property and drives its pins high as well; this firmware at least cannot short against the controller. Whether saves suffer is settled only on a drive. The fix that removes it for certain is the two trace cuts of the classic wired mod, which this project deliberately does not require.

## Compared with the GAL modchips

The GAL16V8 modchip in the open-source FDS-FD3206-Modchip project lifts pin 1 and pin 19 and joins them with a short wire. In registered mode a GAL clocks its flip-flop only from pin 1, on a rising edge, while the drive toggles on the falling edge of WRITE DATA; so the GAL inverts WRITE DATA onto output pin 19 and loops it back into its own clock pin. Neither pin is a reset. The commercial "FD3206 Add-On Chip V4" is sold with its markings removed, and its install photo shows the same pin 1 to pin 19 jumper and eight soldered pins, so it is most likely the same GAL design; that is an inference from the photo, not a confirmed part number.

The ATtiny needs no loop: INT0 on pin 6 triggers on the falling edge directly, and pins 1 and 19 are clipped. Both designs solder the same eight pins, and neither removes a power board lockout.

## Figures

<!-- figures:begin -->
| Figure | Value |
|---|---|
| Flash used | 226 bytes |
| Edge handler | 15 instructions |
| Firmware source | 221 non-blank lines |
<!-- figures:end -->

Measured from the release build.

## Provenance

Written independently from public documentation. Only these sources informed the code and this README:

| Source | Facts taken |
|---|---|
| Famicom World, "Famicom Disk System FD3206 Write Mod" | FD3206P pads for +5 V, GND, /READY, /WRITE GATE, /WRITE PROTECT, WRITE DATA from its labelled board photos; head traces at pins 14 and 15; head drive behaviour of its 74LS76 and 74LS45 schematic |
| Famicom World, "FDS Power Board Modifications" | Which power board revisions carry a write lockout and how each is removed |
| nesdev forum threads 11342, 17037, 19856 and "Disable copy protection on the Twin Famicom AN-505BK" | The Twin Famicom power board change and per-model reports |
| Brad Taylor, "Famicom Disk System technical reference", nesdev.org | 96.4 kHz bit rate, 10 percent tolerance, 1 us pulses, signal names |
| nesdev wiki, "FDS RAM adaptor cable pinout" and "FDS disk format" | Signal names and polarities, gap lengths |
| Microchip document 8246, ATtiny2313A/4313 | Pinout, interrupts, supply range, pin current |
| avrdude 8 part database | Fuse bit meanings and factory values |
| FDSStick and ToToTEK product pages for the FD3206 V4 add-on chip | Install photo showing eight soldered pins and a pin 1 to pin 19 jumper, for the comparison above only |

Stephen-Arsenault/FDS-FD3206-Modchip, CC BY-SA 4.0, puts a programmable logic device on the same controller. No file, source text, photo, documentation text or name from it appears here, and the controller pin numbers were confirmed against the Famicom World board photos, not taken from it. The function is the same because it is the drive's function; the expression differs: the GAL is combinational logic and a register clocked through a wire loop, this is a C main loop and an assembly interrupt that swaps two precomputed port values and drives the pins open-drain. No block of four or more lines matches, and the firmware contains no third-party code.

## License

MIT. See [`LICENSE`](LICENSE).
