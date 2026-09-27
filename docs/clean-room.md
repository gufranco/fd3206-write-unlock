# Clean-Room Record (as of 2026-09-27)

## Sources used

Only public hardware documentation informed this code:

| Source | Facts taken |
|---|---|
| Famicom World, "Famicom Disk System FD3206 Write Mod" | FD3206P pads for +5V, GND, /READY, /WRITE GATE, /WRITE PROTECT, WRITE DATA from its labelled board photos; head traces at pins 14 and 15; head drive behaviour of its 74LS76 and 74LS45 schematic |
| Brad Taylor, "Famicom Disk System technical reference", nesdev.org | 96.4 kHz bit rate, 10 percent tolerance, 1 us pulses, signal names |
| nesdev wiki, "FDS RAM adaptor cable pinout" | Signal names and polarities |
| Microchip document 8246, ATtiny2313A/4313 | Pinout, supply range, pin current |
| avrdude 8 part database | Fuse bit meanings and factory values |

## Prior art not copied

Stephen-Arsenault/FDS-FD3206-Modchip, CC BY-SA 4.0, puts a programmable logic device on the same controller. It was studied for the idea of mounting a chip on top of the FD3206P. No file, source text, photo, documentation text or name from it appears here. The controller pin numbers here were confirmed against the Famicom World board photos, not taken from it. Ideas and hardware facts are not covered by copyright, so its ShareAlike term does not reach this code.

## Independence checks

| Check | Result |
|---|---|
| Structure | Prior art is one CUPL equation file for a GAL. This is AVR assembly with an interrupt handler and polling loop, a per-target pin header, a C simulation harness and a Makefile. No shared structure |
| Naming | No identifier, file name, project name or string from the prior art. Shared words are the drive's own signal names |
| Logic | Prior art is combinational logic plus a clocked register, with the clock made from an extra wire between two pins. This uses an edge interrupt, precomputed port values, a separate release path, open-drain outputs, a watchdog and brown-out reset, none of which the prior art has |
| Pin use | Prior art solders nine pins and adds a wire. This solders eight pins and needs no wire, a consequence of the ATtiny2313A pinout |
| Documentation | README and requirements written from scratch; no sentence or table taken from the prior art |
| Code transplant | No block of 4 or more lines matches; the languages differ |
| License | MIT, chosen because nothing from the CC BY-SA prior art is included |
