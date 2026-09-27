# Hardware (as of 2026-09-27)

This is a prototype wiring that has not yet been built and tested on a drive. Every value below comes from datasheets and from the Famicom World board photos of the FD3206P.

## Parts

| Qty | Part | Role |
|---|---|---|
| 1 | Raspberry Pi Pico, or any RP2040 board exposing GP0 to GP8 and VSYS | Firmware |
| 1 | 74LVC245A, DIP-20 or SOIC-20 | Shifts the four 5 V drive signals to 3.3 V; its inputs accept up to 5.5 V at a 3.3 V supply |
| 2 | 2N3904 NPN transistor | Pulls a write-head line low |
| 2 | 1 kOhm resistor | Transistor base |
| 2 | 10 kOhm resistor | Base pull-down, keeps the heads released while the RP2040 is off or in reset |
| 1 | 1N5817 Schottky diode | Feeds the drive's 5 V into VSYS without back-feeding the drive from USB |
| 1 | 100 nF ceramic capacitor | 74LVC245A supply decoupling |

## Wiring

FD3206P pin numbers follow the chip's own numbering, pin 1 at the notch.

| From | To | Signal |
|---|---|---|
| FD3206P pin 20 | 1N5817 anode; cathode to Pico VSYS, pin 39 | +5 V |
| FD3206P pin 10 | Pico GND, pin 38, and 74LVC245A pin 10 | Ground |
| Pico 3V3 OUT, pin 36 | 74LVC245A pin 20, with 100 nF to ground | Buffer supply |
| 74LVC245A pin 1, DIR | 3.3 V | A to B direction |
| 74LVC245A pin 19, /OE | Ground | Buffer always on |
| FD3206P pin 6 | 74LVC245A A1, pin 2; B1, pin 18, to GP3 | WRITE DATA |
| FD3206P pin 4 | 74LVC245A A2, pin 3; B2, pin 17, to GP4 | /WRITE GATE |
| FD3206P pin 5 | 74LVC245A A3, pin 4; B3, pin 16, to GP5 | /WRITE PROTECT |
| FD3206P pin 13 | 74LVC245A A4, pin 5; B4, pin 15, to GP6 | /READY |
| 74LVC245A A5 to A8, pins 6 to 9 | Ground | Unused inputs |
| GP7 | 1 kOhm to Q1 base; 10 kOhm from Q1 base to ground | Head 1 drive |
| GP8 | 1 kOhm to Q2 base; 10 kOhm from Q2 base to ground | Head 2 drive |
| Q1 and Q2 emitters | Ground | |
| Q1 collector | Head 1 line, on the head side of the cut next to FD3206P pin 15 | WRITE HEAD 1 |
| Q2 collector | Head 2 line, on the head side of the cut next to FD3206P pin 14 | WRITE HEAD 2 |

GP0 and GP1 carry the UART console. GP25 is the Pico's on-board LED, lit while a write is in progress.

## Install

1. Open the drive, lift out the mechanism, and remove the bottom plate to reach the controller board.
2. Cut the two traces that run from FD3206P pins 15 and 14 to the write heads. Cut them close to the chip, so the head-side pull-up resistors stay connected to the heads. Check with a multimeter that each pin no longer connects to its head line.
3. Solder the eight wires from the table above: +5 V, ground, the four signals, and the two head lines on the head side of the cuts.
4. Tape the inside of the bottom plate where the wires pass, route the wires through the plate's opening, and fix the board in an empty corner of the case.

Before the first write, measure each head pull-up resistor. The 2N3904 sinks up to 200 mA, far above what these lines need, but the value tells you the head current for the record.

## Test on the drive

Use a disk you do not care about.

1. Power the drive and open the console. It prints `fdswriteunlock ready, power-on start`.
2. Save a game. The console prints one `write` line per block the RAM adapter writes.
3. Rewrite a whole disk with a disk-writing tool, then read it back several times.
4. With a logic analyzer, compare WRITE DATA against the two collectors. Each falling edge moves the low level from one collector to the other.

## FD7201P drives

Drives with the earlier FD7201P controller have no write lockout and need none of this.
