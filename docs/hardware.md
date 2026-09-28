# Hardware (as of 2026-09-28)

The only part added to the drive is the programmed ATtiny2313A, soldered on top of the FD3206P. Nothing is cut and no wire is added. This has not yet been built on a drive.

## Install

1. Program the chip first; see the README.
2. Bend or clip ATtiny pins 1, 2, 3, 7, 8, 9, 11, 12, 16, 17, 18 and 19 so they cannot touch the FD3206P. Their counterparts on the FD3206P are signals nobody has documented, and the firmware never needs them.
3. Open the drive, lift out the mechanism, and remove the bottom plate to reach the controller board.
4. Place the ATtiny on the FD3206P with pin 1 over pin 1. Both notches face the same way.
5. Solder ATtiny pins 4, 5, 6, 10, 13, 14, 15 and 20 to the FD3206P pins underneath.
6. Refit the bottom plate with tape over the area above the chips.

## Before the first write

Measure the two head pull-up resistors. With a head pulled low the ATtiny sinks 5 V divided by that resistance, which must stay under its 20 mA per-pin rating, meaning a pull-up of 250 Ohm or more. The FD3206P shares the load while it drives the same head.

## Test on the drive

Use a disk you do not care about.

1. Save a game. This is the case the FD3206P also writes; see the README.
2. Rewrite a whole disk with a disk-writing tool, then read it back several times.
3. With a logic analyzer, compare WRITE DATA on pin 6 with pins 14 and 15. Each falling edge moves the low level from one pin to the other.

## Check the power board first

The FD3206P is not the only write lockout in some drives. The Famicom World article "FDS Power Board Modifications" reports that later power boards carry their own copy-protection circuit, which blocks rewriting even on drives with the unprotected FD7201P:

| Power board label | Needs a change |
|---|---|
| FMD-POWER-01 | no |
| FMD-POWER-02 | only units fitted with a protection daughterboard, which is removed |
| FMD-POWER-03 | not documented |
| FMD-POWER-04 | yes: remove JP14 and link points A and B with a wire |
| FMD-POWER-05 | yes: remove two jumper wires, cut two traces and add two wire links |

Read the label on the power board before installing. On FMD-POWER-04 and -05 the ATtiny alone cannot make whole-disk writes work, and the change above is a board modification this project does not make. Commercial FD3206 modchips carry the same instruction.

## Compared with the GAL modchips

The GAL16V8 modchips, including the commercial "V4" sold with its markings removed, lift pin 1 and pin 19 and join them with a short wire. In registered mode a GAL clocks its flip-flop only from pin 1, on a rising edge, while the drive toggles on the falling edge of WRITE DATA; so the GAL inverts WRITE DATA onto output pin 19 and loops it back into its own clock pin. Neither pin is a reset. The ATtiny needs no loop: INT0 on pin 6 is set to trigger on the falling edge directly, and pins 1 and 19 are clipped. Both designs solder the same eight pins.

## FD7201P drives

Drives with the earlier FD7201P controller have no write lockout and need none of this.
