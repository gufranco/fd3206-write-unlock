# Hardware (as of 2026-09-28)

The only part added to the drive is the programmed ATtiny2313A. This wiring has not yet been built on a drive. Every value below comes from the ATtiny2313A datasheet and from the Famicom World board photos of the FD3206P.

## Wiring

FD3206P pin numbers follow the chip's own numbering, pin 1 at the notch.

| ATtiny2313A pin | FD3206P point | Signal |
|---|---|---|
| 20, VCC | pin 20 | +5 V |
| 10, GND | pin 10 | Ground |
| 8, PD4 | pin 6 | WRITE DATA |
| 12, PB0 | pin 4 | /WRITE GATE |
| 13, PB1 | pin 5 | /WRITE PROTECT |
| 15, PB3 | pin 13 | /READY |
| 14, PB2 | head side of the cut beside pin 15 | Head 1 |
| 9, PD5 | head side of the cut beside pin 14 | Head 2 |

Leave every other ATtiny pin unconnected.

## Install

1. Program the chip first; see the README.
2. Open the drive, lift out the mechanism, and remove the bottom plate to reach the controller board.
3. Cut the two traces that run from FD3206P pins 15 and 14 to the write heads. Cut them close to the chip, so the head-side pull-up resistors stay connected to the heads. Check with a multimeter that each pin no longer connects to its head line.
4. Measure each head line's pull-up resistor. With a head pulled low the ATtiny sinks 5 V divided by that resistance, and that must stay under the ATtiny2313A's 20 mA per-pin rating, meaning a pull-up of 250 Ohm or more. If a pull-up is smaller, stop: the chip alone cannot drive that head.
5. Solder the eight wires from the table above.
6. Fix the chip in an empty corner of the case, insulated from the metal plate, and route the wires through the plate's opening.

## Test on the drive

Use a disk you do not care about.

1. Save a game.
2. Rewrite a whole disk with a disk-writing tool, then read it back several times.
3. With a logic analyzer, compare WRITE DATA on ATtiny pin 8 with pins 14 and 9. Each falling edge moves the low level from one pin to the other.

## FD7201P drives

Drives with the earlier FD7201P controller have no write lockout and need none of this.
