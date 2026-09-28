# Hardware (as of 2026-09-28)

**TL;DR:** a Famicom Disk System drive can have two write lockouts, and this chip removes only one. The FD3206P controller on the drive mechanism blocks whole-disk writes; the ATtiny fixes that with nothing cut. The power board, the second circuit board in the unit, also blocks writes on revisions FMD-POWER-04 and -05 and on some -02 boards, and that one needs its own modification. Identify the power board before anything else.

This has not yet been built on a drive.

## Two boards, two lockouts

The HVC-022 drive unit holds two boards:

| Board | What it does | Lockout | Fixed by |
|---|---|---|---|
| Drive mechanism, with the FD3206P or FD7201P controller | reads and writes the disk | FD3206P releases the heads on a whole-disk write; FD7201P has none | this ATtiny, on FD3206P drives only |
| Power board, labelled FMD-POWER-01 to -05 | switches battery or adapter power to the motor and carries every signal between the RAM adapter connector and the drive mechanism | on -04, -05 and some -02 boards, a circuit that blocks the write signal | the power board modification below |

A drive writes whole disks only when both lockouts are gone. An FD7201P drive needs no chip but can still need the power board change; an FD3206P drive with an FMD-POWER-01 board needs only the chip. The ATtiny's no-cut rule covers the drive mechanism; it cannot reach the power board's circuit, because that circuit sits upstream on a different board.

## Step 1: identify the power board

1. Remove the six Phillips screws on the bottom of the unit.
2. Flip the unit and lift the top cover off carefully.
3. Remove the two Phillips screws holding the battery compartment and set it aside.
4. Remove the screws holding the power board and lift it out.
5. Turn it component side up and find the text `©198X Nintendo` and the code `FMD-POWER-XX`.

| Label | Protected | What to do |
|---|---|---|
| FMD-POWER-01 | no | nothing |
| FMD-POWER-02 without a green daughterboard | no | nothing |
| FMD-POWER-02 with a green daughterboard | yes, on the daughterboard | remove the daughterboard, step 2a |
| FMD-POWER-03 | not documented; presumed like -02 | look for a daughterboard; if there is none and whole-disk writes still fail with the chip installed, the board is the suspect |
| FMD-POWER-04 | yes | step 2b |
| FMD-POWER-05 | yes | step 2c |

## Step 2: remove the power board lockout

The Famicom World article [FDS Power Board Modifications](https://famicomworld.com/workshop/tech/fds-power-board-modifications/) has a photo of each board with the exact points marked. Follow its photos; the steps below say what each change is, not where on your board it lands.

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

## Step 3: install the chip, FD3206P drives only

The only part added to the drive mechanism is the programmed ATtiny2313A, soldered on top of the FD3206P. Nothing on that board is cut and no wire is added.

1. Program the chip first; see the README.
2. Bend or clip ATtiny pins 1, 2, 3, 7, 8, 9, 11, 12, 16, 17, 18 and 19 so they cannot touch the FD3206P. Their counterparts on the FD3206P are signals nobody has documented, and the firmware never needs them.
3. Lift out the drive mechanism and remove its bottom plate to reach the controller board.
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

If whole-disk writes still fail, check that WRITE DATA and /WRITE GATE reach FD3206P pins 6 and 4 during the write. If they do not, the power board is still blocking the write signal upstream of the chip.

## Compared with the GAL modchips

The GAL16V8 modchip in the open-source FDS-FD3206-Modchip project lifts pin 1 and pin 19 and joins them with a short wire. In registered mode a GAL clocks its flip-flop only from pin 1, on a rising edge, while the drive toggles on the falling edge of WRITE DATA; so the GAL inverts WRITE DATA onto output pin 19 and loops it back into its own clock pin. Neither pin is a reset. The commercial "FD3206 Add-On Chip V4" is sold with its markings removed, and its install photo shows the same pin 1 to pin 19 jumper and eight soldered pins, so it is most likely the same GAL design; that is an inference from the photo, not a confirmed part number.

The ATtiny needs no loop: INT0 on pin 6 triggers on the falling edge directly, and pins 1 and 19 are clipped. Both designs solder the same eight pins, and both leave the power board to step 2.
