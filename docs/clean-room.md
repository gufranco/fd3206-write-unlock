# Clean-Room Record (as of 2026-09-27)

## Sources used

Only public documentation informed this code:

| Source | Facts taken |
|---|---|
| Famicom World, "Famicom Disk System FD3206 Write Mod" | FD3206P pads for +5 V, GND, /READY, /WRITE GATE, /WRITE PROTECT, WRITE DATA from its labelled board photos; head traces at pins 14 and 15 and where to cut them; head drive behaviour of its 74LS76 and 74LS45 schematic |
| Brad Taylor, "Famicom Disk System technical reference", nesdev.org | 96.4 kHz bit rate, 10 percent tolerance, 1 us pulses, signal names |
| nesdev wiki, "FDS RAM adaptor cable pinout" | Signal names and polarities |
| Raspberry Pi RP2040 datasheet and pico-sdk 2.3.1 | PIO, GPIO override, IO bank interrupts, PWM edge counting, watchdog |
| rp2040js 1.4.0 source, MIT | Emulator behaviour used by the test harness |

## Prior art not copied

Stephen-Arsenault/FDS-FD3206-Modchip, CC BY-SA 4.0, puts a programmable logic device on the same controller. No file, source text, photo, documentation text or name from it appears here. The controller pin numbers were confirmed against the Famicom World board photos, not taken from it.

## Independence checks

| Check | Result |
|---|---|
| Structure | Prior art is one CUPL equation file for a GAL soldered on the controller. This is RP2040 firmware on a separate board: a PIO program, a GPIO interrupt with output-enable override gating, a PWM edge counter, a console, a CMake build and an emulator test harness. No shared structure |
| Naming | No identifier, file name, project name or string from the prior art. Shared words are the drive's own signal names |
| Logic | Prior art is combinational logic plus a clocked register. This uses a PIO state machine with side-set, interrupt-driven gating, open-collector transistors, a watchdog and a write log, none of which the prior art has |
| Documentation | README, hardware guide and requirements written from scratch |
| Code transplant | No block of 4 or more lines matches; the languages differ |
| Third-party code | pico-sdk is fetched at build time under BSD-3-Clause. rp2040js is a test dependency under MIT, with a one-character local patch in `test/patches`. The RP2040 boot ROM is not used or stored |
| License | MIT, chosen because nothing from the CC BY-SA prior art is included |
