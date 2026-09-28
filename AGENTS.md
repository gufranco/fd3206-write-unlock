# AGENTS.md

Contract for anyone, human or agent, working in this repository.

## What this project is

Firmware for an ATtiny2313A that sits on top of the Mitsumi FD3206P controller of a Famicom Disk System drive and does the controller's write stage itself, so the drive can rewrite whole disks. The FD3206P releases the write heads when it sees a whole-disk write; the ATtiny drives them instead, with the same logic as the classic 74LS76 and 74LS45 write mod.

## Hard rules

1. **Piggyback only.** The ATtiny sits on the FD3206P pin 1 over pin 1. Nothing on the drive board is cut and no other part is added: no resistor, diode, transistor, wire or capacitor. A change that needs any of them is out of scope, however much it would improve something.
2. **ATtiny2313A or ATtiny4313 only.** The FD3206P pin map fixes which ATtiny pin meets which signal, and only the x313 puts GND, VCC, INT0 and a port pin on every signal pin at once. The 8-pin parts have five usable I/O and the design needs six; the sixth would be RESET, which would end in-system programming. Do not propose them again without a new pin argument.
3. **The logic is the classic write stage, nothing more.** One flip-flop state changed by each falling edge of WRITE DATA, one head driven per state, both heads released unless /READY, /WRITE PROTECT and /WRITE GATE are all low. No detection, no heuristics, no state machine layered on top. An earlier revision watched the FD3206P and stepped aside when it wrote; it was removed because every guess is a new way to fail.
4. **Protection is allowed only when it is nearly free.** The watchdog, the brown-out fuse and pull-low outputs stay because each costs a few lines or nothing at run time. Anything else needs a measured reason recorded here first.
5. **A head pin pulls low or floats.** It is an output at 0 or an input, never an output at 1. The FD3206P shares pins 14 and 15, and a pin that never drives high cannot short against it.
6. **Never drive a pin whose FD3206P function is unknown.** Only pins 4, 5, 6, 10, 13, 14, 15 and 20 are soldered. Every other pin is clipped, kept an input, and given its internal pull-up so it cannot float.
7. **C is gnu11 through avr-gcc and avr-libc.** Scalars are `<stdint.h>` types and `bool`. No heap, no recursion, no function pointers. The only unbounded loop is `main`, which runs until power-off. `-Wall -Wextra -Werror` on every compile.
8. **No comments in source files.** Names carry the meaning; explanation belongs here, in `README.md` or in the commit message.
9. **The library comes first.** `<avr/wdt.h>`, `<avr/power.h>` and `<util/atomic.h>` do their jobs; a hand-written equivalent is a defect.
10. **Scripts are Python 3.** Build tooling and checks are Python; Make recipes call a Python script rather than growing shell logic. No bash, awk or perl.
11. **The firmware folder is an Arduino sketch.** `fdswriteunlock/` holds an empty `.ino`, the C and assembly sources and the header, so ATTinyCore builds it unchanged. The firmware defines its own `main`, which keeps the Arduino core's `setup`, `loop` and timer code out of the link.
12. **`make test` passes before any commit.** Thirteen scenarios, and every firmware instruction executed.

## Pin map

Fixed by the FD3206P. Each pin number is the same on both chips.

| Pin | FD3206P signal | ATtiny2313A | Direction |
|---|---|---|---|
| 4 | /WRITE GATE | PA1 | input |
| 5 | /WRITE PROTECT | PA0 | input |
| 6 | WRITE DATA | PD2, INT0 | input, falling edge interrupt |
| 10 | GND | GND | |
| 13 | /READY | PB1 | input |
| 14 | Head 2 | PB2 | pull low or float |
| 15 | Head 1 | PB3 | pull low or float |
| 20 | +5 V | VCC | |

Source: the labelled solder-side photos in the Famicom World article "Famicom Disk System FD3206 Write Mod", which name +5V, GND, /READY, /WRITE GATE, /WRITE PROTECT and WRITE DATA on the FD3206P pads and mark the head traces beside pins 14 and 15.

## Timing budget

The binding numbers, at the 8 MHz internal oscillator:

| Rule | Value | Source |
|---|---|---|
| Bit cell | 10.4 us | 96.4 kHz, Brad Taylor, FDS technical reference |
| Shortest spacing between WRITE DATA falling edges | 4.7 us | half a cell at the 10 percent fast limit |
| WRITE DATA low pulse | about 1 us | same reference |
| Interrupt handler, whole | must finish under 4.7 us, 37 cycles; today about 30 | instruction count of `write_data_edge.S` |
| Interrupt taken to head written | 10 cycles | same |
| Gate response | under 10 us, one bit cell | a gate change falls in a gap of at least 480 bits, nesdev "FDS disk format" |
| Head pin sink current | 20 mA per pin | ATtiny2313A datasheet, Microchip document 8246 |

A C interrupt that calls a helper saves every call-clobbered register and runs about 70 cycles, which is why the handler is assembly. Keep it free of `SREG` changes and of anything the C code owns: it reads and writes only `GPIOR0`, `GPIOR1`, `GPIOR2` and `DDRB`.

## Do not implement from a guess

A hardware fact enters the code only with a source beside it in this file or in `docs/requirements.md`: a datasheet section, a labelled photo, a disassembly address. What the FD3206P does on its other twelve pins is not known and is not guessed. Neither is whether its head outputs are open collector, which is the one assumption the piggyback design still rests on.

## Layout

```
fdswriteunlock/      Arduino sketch folder and the firmware sources
  fdswriteunlock.ino empty on purpose
  firmware.c         main loop, gate, watchdog, start-up
  write_data_edge.S  the INT0 handler
  board.h            pin map
test/                simavr harness and the 13 scenarios, compiled for the host
tools/               Python helpers called by the Makefile
docs/                requirements, hardware guide, clean-room record
build/               generated, never committed
```

## Build commands

```
make            build build/fdswriteunlock.hex
make size       firmware size
make test       run the simavr scenarios, with instruction coverage
make fuses      write lfuse 0xE4 and hfuse 0xD9, PROGRAMMER and PORT as needed
make flash      program the chip
make clean      remove build output
```

## Measuring a change

- **A refactor changes nothing, and the hex proves it.** Compare the SHA-256 of `build/fdswriteunlock.hex` before and after. Equal digests end the review; different ones mean the change is not a pure refactor and `make test` decides.
- **A mutant only counts if it builds.** When checking that a test catches a defect, confirm the mutated firmware compiled. A mutant rejected by `-Werror` produces no test output, which reads like silence, not like a catch.
- **Probe the registers before blaming the firmware.** When a scenario fails, print DDRB, PORTB and the pin state cycle by cycle from a small throwaway program linked against `test/board.c`. Twice now the defect was in the harness.
- **State the timing figure's origin.** An instruction count, a datasheet number and a simulation are three different claims. Say which one a number is.

## Failure modes this repository has had

| What happened | Why it was believable | What catches it now |
|---|---|---|
| The interrupt was written in C and called a helper, so it saved fifteen registers and ran about 9 us, longer than the 4.7 us edge spacing | Every scenario at the normal rate passed | the fastest-rate scenario, 1000 edges 4.7 us apart |
| The harness classified heads assuming both pins were outputs at once, left over from a timer design, and reported working firmware as broken | The failure looked like a firmware gating bug | registers probed directly before any firmware change |
| simavr's Timer0 ignored external-clock edges with TOP at 0, skipped compare B, and never wired the force-compare bits for this part | The firmware matched the datasheet and still failed | the design no longer uses the timer; an emulator result is checked against the datasheet before it is trusted |
| A mutant that failed to compile printed nothing and was nearly read as a result | Empty output looks like a quiet pass | the build line is checked before the test line |
| The design grew to an RP2040 board, then to a writer-detection state machine, well past the classic logic | Each step answered a real risk | hard rules 1 to 4 |
| A second published pin map for the GAL version put WRITE DATA on pin 1 and an output on a head pin | It was in the same repository as the correct one | the pin map above is checked against the labelled photos only |
| The GAL version soldered one of its outputs to FD3206P pin 12, whose function nobody knows | It came from a working install photo | hard rule 6 |
| The global gitignore ignores `*.patch`, which hid a file from a commit | `git status` did not list it | `git check-ignore -v` on any generated file before committing |

## Real hardware

Nothing in this repository can drive a drive or a programmer, so a hardware result exists only when someone reports one. Ask before assuming a hardware run happened. What only a drive settles:

| Question | Why simulation cannot |
|---|---|
| Does a single-file save still work with both chips writing the same heads | the FD3206P's own write stage is not modelled |
| Are the FD3206P head outputs open collector | the piggyback design assumes it and no document states it |
| Does a whole-disk write read back | the 2C33's decoding margin is not modelled |
| What the edge-to-head delay really is | simavr enters the interrupt without the input synchroniser delay |

## Emulators

simavr runs headless and opens nothing. Any other emulator used for a check runs headless too, with no window and no sound; a run someone asked to watch opens windowed and minimised, never full screen.

## What is not done

- Nothing has run on a drive.
- The Arduino IDE route follows ATTinyCore's board definitions but has not been test-built with arduino-cli.
- Whether single-file saves suffer from the FD3206P and the ATtiny writing the same heads out of phase is unknown. The classic GAL modchip has the same exposure.
