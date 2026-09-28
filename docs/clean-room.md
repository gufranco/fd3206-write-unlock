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
| simavr source at the pinned commit, GPL-3.0 | Emulator behaviour used by the tests; built at test time, not shipped |

## Prior art not copied

Stephen-Arsenault/FDS-FD3206-Modchip, CC BY-SA 4.0, puts a programmable logic device on the same controller. No file, source text, photo, documentation text or name from it appears here. The controller pin numbers were confirmed against the Famicom World board photos, not taken from it.

## Independence checks

| Check | Result |
|---|---|
| Structure | Prior art is one CUPL equation file for a GAL soldered on top of the controller, with an extra wire. This is C firmware for a microcontroller wired off the controller: a hardware timer toggle, a pin-change interrupt, a watchdog, a Makefile and a simulation harness. No shared structure |
| Naming | No identifier, file name, project name or string from the prior art. Shared words are the drive's own signal names |
| Logic | Prior art is combinational logic plus a clocked register clocked through a jumper wire. This clocks a timer from the data line, toggles two compare outputs, and gates them through pin directions from an interrupt |
| Documentation | README, hardware guide and requirements written from scratch |
| Code transplant | No block of 4 or more lines matches; the languages differ |
| Third-party code | `test/simavr-datasheet-fixes.patch` modifies simavr, so that one file is licensed GPL-3.0-or-later like simavr. It changes the emulator used by the tests and is not part of the firmware |
| License | MIT, chosen because nothing from the CC BY-SA prior art is included |
