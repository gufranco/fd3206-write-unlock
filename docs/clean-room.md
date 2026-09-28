# Clean-Room Record (as of 2026-09-28)

## Sources used

Only public documentation informed this code:

| Source | Facts taken |
|---|---|
| Famicom World, "Famicom Disk System FD3206 Write Mod" | FD3206P pads for +5 V, GND, /READY, /WRITE GATE, /WRITE PROTECT, WRITE DATA from its labelled board photos; head traces at pins 14 and 15 and where to cut them; head drive behaviour of its 74LS76 and 74LS45 schematic |
| Brad Taylor, "Famicom Disk System technical reference", nesdev.org | 96.4 kHz bit rate, 10 percent tolerance, 1 us pulses, signal names |
| nesdev wiki, "FDS RAM adaptor cable pinout" and "FDS disk format" | Signal names and polarities, gap lengths |
| Microchip document 8246, ATtiny2313A/4313 | Pinout, timer, interrupts, supply range, pin current |
| avrdude 8 part database | Fuse bit meanings and factory values |
| ATTinyCore board definitions | Arduino IDE menu names for the ATtiny2313A |

## Prior art not copied

Stephen-Arsenault/FDS-FD3206-Modchip, CC BY-SA 4.0, puts a programmable logic device on the same controller. No file, source text, photo, documentation text or name from it appears here. The controller pin numbers were confirmed against the Famicom World board photos, not taken from it.

## Independence checks

| Check | Result |
|---|---|
| Structure | Prior art is one CUPL equation file for a GAL, clocked through an extra wire from one of its own pins. This is a C main loop and a 15-instruction assembly interrupt for a microcontroller that needs no wire, plus a Makefile and a simavr harness. No shared structure |
| Naming | No identifier, file name, project name or string from the prior art. Shared words are the drive's own signal names |
| Logic | The function is the same, because it is the drive's function: one head per flip-flop state, both released unless three conditions hold. The expression differs: the GAL is combinational logic and a register, this swaps two precomputed port values on an edge interrupt and drives the pins open-drain instead of both ways |
| Documentation | README, hardware guide and requirements written from scratch |
| Code transplant | No block of 4 or more lines matches; the languages differ |
| Third-party code | None in the firmware. simavr is used by the tests as an installed library |
| License | MIT, chosen because nothing from the CC BY-SA prior art is included |
