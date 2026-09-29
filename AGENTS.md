# AGENTS.md

Contract for anyone, human or agent, working in this repository.

## What this project is

Firmware for an ATtiny2313A that sits on top of the Mitsumi FD3206P controller of a Famicom Disk System drive and does the controller's write stage itself, so the drive can rewrite whole disks. The FD3206P releases the write heads when it sees a whole-disk write; the ATtiny drives them instead, with the same logic as the classic 74LS76 and 74LS45 write mod.

## Hard rules

1. **Piggyback only.** The ATtiny sits on the FD3206P pin 1 over pin 1. Nothing on the drive board is cut and no other part is added: no resistor, diode, transistor, wire or capacitor. A change that needs any of them is out of scope, however much it would improve something. The rule covers the drive mechanism board. The power board's own write lockout, on FMD-POWER-04, -05, some -02 boards and the Twin Famicom AN-500, is a separate prerequisite that the docs describe and the firmware cannot remove.
2. **ATtiny2313A or ATtiny4313 only.** The FD3206P pin map fixes which ATtiny pin meets which signal, and only the x313 puts GND, VCC, INT0 and a port pin on every signal pin at once. The 8-pin parts have five usable I/O and the design needs six; the sixth would be RESET, which would end in-system programming. Do not propose them again without a new pin argument.
3. **The logic is the classic write stage, nothing more.** One flip-flop state changed by each falling edge of WRITE DATA, one head driven per state, both heads released unless /READY, /WRITABLE MEDIA and /WRITE GATE are all low. No detection, no heuristics, no state machine layered on top. An earlier revision watched the FD3206P and stepped aside when it wrote; it was removed because every guess is a new way to fail.
4. **Protection is allowed only when it is nearly free.** The watchdog, the brown-out fuse, pull-low outputs and debug-only assertions stay because each costs a few lines or nothing in the release image. Anything else needs a measured reason recorded here first.
5. **A head pin pulls low or floats.** It is an output at 0 or an input, never an output at 1. The FD3206P shares pins 14 and 15, and a pin that never drives high cannot short against it.
6. **Never drive a pin whose FD3206P function is unknown.** Only pins 4, 5, 6, 10, 13, 14, 15 and 20 are soldered. Every other pin is clipped, kept an input, and given its internal pull-up so it cannot float. The four soldered inputs keep their internal pull-ups too: the ATtiny guarantees a high only from 3.0 V where the TTL parts it replaces accept 2.0 V, and the power board already pulls /WRITE GATE up with 10 kOhm. A head pin's port latch stays 0, which `pins.h` asserts at compile time, so enabling a head can only pull it low.
7. **C17 and MISRA C:2012 with zero deviations, built only in the pinned Docker toolchain.** C17 is the newest standard MISRA C:2012 and its amendments cover; C23 features, and the C11 features rule 1.4 calls emergent such as `_Noreturn`, are out. `make analyse` runs the cppcheck MISRA addon over the debug and the release configuration and fails on any finding; there is no deviation list. Every compile, check and test runs in the image described by [`Dockerfile`](Dockerfile). The host needs Docker and Python 3; `make` delegates to the container. Programming with `avrdude` is the one step that runs on the host. Released firmware is built by the release pipeline, never by an IDE.
8. **NASA Power of 10, adapted below.** Every exception is listed in the table in this file; there are no others.
9. **No comments in source files, except the two SPDX header lines.** Names carry the meaning; explanation belongs here, in the READMEs, in the local `docs/` or in the commit message. [`tools/style_gate.py`](tools/style_gate.py) enforces it.
10. **Registers are touched only in assembly.** Every register access lives in [`src/port.S`](src/port.S) and the INT0 handler, behind the C prototypes in [`include/port/port.h`](include/port/port.h). No C file includes an `avr/` or `util/` header, because avr-libc reaches registers through integer-to-pointer casts that MISRA rule 11.4 forbids and through inline assembly the checker cannot see. The watchdog and clock-prescaler timed sequences in `port.S` follow the ATtiny2313A datasheet, Microchip document 8246, and are checked in simulation.
11. **Scripts are Python 3.** Build tooling and checks are Python with 100 percent line and branch coverage; Make recipes call a Python module rather than growing shell logic. No bash, awk or perl.
12. **READMEs are commercial and technical, never development.** The four READMEs are the single source for what the product is, how to install it and how it works, and they never link into `docs/`. Development material, including build internals, test tiers and ADRs, lives in `docs/` or `specs/`, which are gitignored and never pushed. A README change lands in all four languages in the same commit; `make analyse` checks that their figures blocks agree with the build.
13. **`make analyse test` passes before any commit.** `make hooks` installs a pre-commit hook that runs both.

## Power of 10

| Rule | How it holds here | Exception |
|---|---|---|
| 1. Simple control flow, no goto, setjmp or recursion | `tools/style_gate.py` rejects them | none |
| 2. Every loop has a fixed bound | loops in firmware: two | `main` runs until power-off; `fdswu_assert_fail` halts forever with heads released and the watchdog off |
| 3. No dynamic memory after start-up | no heap at all; the style gate rejects `malloc`, `calloc`, `realloc`, `free`, `alloca` | none |
| 4. Functions fit on a page | the style gate fails any function over 60 lines | none |
| 5. Two assertions per function | every C function with logic carries two `FDSWU_ASSERT`; compiled in with `FDSWU_DEBUG` for the debug ELF and the host tests, out of the release image | `fdswu_conditions_allow` is a single expression, `main` is the loop itself, and the assembly port functions are single register moves |
| 6. Smallest scope for data | file-scope state is limited to the three GPIOR registers the edge handler shares | none |
| 7. Check every return value and parameter | MISRA rule 17.7, enforced by the MISRA gate, requires every returned value to be used | none |
| 8. Limited preprocessor | `#define` only for include guards, the assertion macro, typed object-like constants such as `((uint8_t)0x08U)` checked by `_Static_assert`, and names the assembly shares | none |
| 9. Restricted pointers | no pointers in C firmware code, not even through register macros; the style gate rejects function pointers | none |
| 10. All warnings on, static analysers clean | `-Wall -Wextra -Wpedantic -pedantic-errors -Werror` plus conversion, shadow and prototype warnings; cppcheck `--enable=all --check-level=exhaustive` | none; MISRA C:2012 is a gate with zero findings |

## Types

Firmware code uses only `<stdint.h>` fixed-width types and `bool`. The style gate rejects `char`, `short`, `int`, `long`, `unsigned`, `signed`, `float` and `double`, except `int main(void)`. The reason is that their widths differ between the chip and the machines the tests run on, and [`tests/type_widths.c`](tests/type_widths.c) proves it on every `make analyse`:

| Type | avr-gcc 14 | aarch64 gcc | x86_64 gcc |
|---|---|---|---|
| `char` | 1, signed | 1, unsigned | 1, signed |
| `short` | 2 | 2 | 2 |
| `int` | 2 | 4 | 4 |
| `long` | 4 | 8 | 8 |
| `long long` | 8 | 8 | 8 |
| pointer | 2 | 8 | 8 |
| `float` | 4 | 4 | 4 |
| `double` | 4 | 8 | 8 |

Naming follows BARR-C: `fdswu_` prefix on every external symbol, `_t` suffix on every type, `FDSWU_` on every constant and macro.

## Layers

| Layer | Files | May include |
|---|---|---|
| Logic | `src/heads.c`, `src/conditions.c`, `include/fdswu/*.h` | `<stdint.h>`, `<stddef.h>`, `<stdbool.h>`, `fdswu/` |
| Platform C | `src/main.c` | the above plus `port/` |
| Hardware | `src/port.S`, `src/write_data_edge.S`, `include/port/*.h` | the above plus `avr/`, `util/` |

The logic layer compiles and runs on the host, which is how it reaches 100 percent line and branch coverage. [`tools/layer_check.py`](tools/layer_check.py) fails the build when a logic file reaches the platform, or when any C file includes a hardware header.

## Pin map

Fixed by the FD3206P. Each pin number is the same on both chips.

| Pin | FD3206P signal | ATtiny2313A | Direction |
|---|---|---|---|
| 4 | /WRITE GATE | PA1 | input |
| 5 | /WRITABLE MEDIA, low means writable | PA0 | input |
| 6 | WRITE DATA | PD2, INT0 | input, falling edge interrupt |
| 10 | GND | GND | |
| 13 | /READY | PB1 | input |
| 14 | Head 2 | PB2 | pull low or float |
| 15 | Head 1 | PB3 | pull low or float |
| 20 | +5 V | VCC | |

Source: the labelled solder-side photos in the Famicom World article "Famicom Disk System FD3206 Write Mod", which name +5V, GND, /READY, /WRITE GATE, /WRITE PROTECT and WRITE DATA on the FD3206P pads and mark the head traces beside pins 14 and 15. The reverse-engineered FMD-POWER-05 schematic on the nesdev forum names pin 5's signal /writable media and shows only +5 V and the +5 V motor rail reaching the drive board.

## Timing budget

The binding numbers, at the 8 MHz internal oscillator:

| Rule | Value | Source |
|---|---|---|
| Bit cell | 10.4 us | 96.4 kHz, Brad Taylor, FDS technical reference |
| Shortest spacing between WRITE DATA falling edges | 4.7 us | half a cell at the 10 percent fast limit |
| WRITE DATA low pulse | about 1 us | same reference |
| Interrupt handler, whole | must finish under 4.7 us, 37 cycles at 8 MHz and 33 at 7.2 MHz; today 32 at worst: 4 response, 2 vector, 22 or 23 in the handler, up to 3 for the main loop's `ret` in progress | instruction count and the AVR interrupt response in Microchip document 8246 |
| Interrupt taken to head written | 10 cycles, plus up to 3 for the instruction in progress | same |
| Interrupts held off by a condition change | at most 12 cycles: `fdswu_port_commit_heads` picks between two precomputed plans with interrupts off | instruction count of `port.S` |
| Clock | internal RC, specified at +/-10 percent at 3 V; every scenario runs at 7.2, 8.0 and 8.8 MHz | Microchip document 8246, section 6.2.3 |
| Gate response | under 10 us without data edges, 7.4 us worst at 8 MHz and 8.2 us at 7.2 MHz across 64 main-loop phases; under 100 us while data edges arrive at the fastest rate, 19 us and 23 us today | a gate change falls in a gap of at least 480 bits, nesdev "FDS disk format" |
| Head pin sink current | 20 mA per pin | ATtiny2313A datasheet, Microchip document 8246 |

A C interrupt that calls a helper saves every call-clobbered register and runs about 70 cycles, which is why the handler is assembly. Keep it free of `SREG` changes and of anything the C code owns: it reads and writes only `GPIOR0`, `GPIOR1`, `GPIOR2` and `DDRB`.

## Do not implement from a guess

A hardware fact enters the code only with a source beside it in this file or in the README: a datasheet section, a labelled photo, a disassembly address. What the FD3206P does on its other twelve pins is not known and is not guessed. Neither is whether its head outputs are open collector, which is the one assumption the piggyback design still rests on.

## Layout

```
include/fdswu/       logic-layer headers: pins, heads plan, conditions, assertion
include/port/        C prototypes of the port module, register names for the assembly
src/                 firmware: logic modules, main loop, assembly port module, INT0 handler
tests/host/          host unit tests of the logic layer
tests/sim/           simavr harness and the 16 scenarios, run on each chip's image at 7.2, 8.0 and 8.8 MHz
tests/tools/         unit tests of the Python tools
tests/type_widths.c  compile-time record of type widths per compiler
tools/               Python gates and helpers called by the Makefile
docker/              pinned Python tool requirements for the image
LICENSES/            licence texts for REUSE
docs/                local only, gitignored: development guide, requirements spec, ADRs
README*.md           commercial and technical description in English, Japanese, Simplified and Hong Kong Chinese
build/               generated, never committed
```

## Build commands

```
make            release hex for each chip and debug ELF, in Docker
make size       firmware size
make analyse    clang-format, ruff, style gate, layer check, REUSE, type widths, cppcheck with MISRA, README figures,
                hex size and edge-handler cycle budget per chip, tool tests at 100 percent
make test       host tests at 100 percent line and branch, then the simavr suite at 100 percent instructions
                on each chip at three clocks, failing past 32 bytes of stack
make misra      the MISRA C:2012 gate alone, also part of analyse
make mutation   break the firmware in 11 known ways; each break must fail the build or the tests
make reproducible  build twice at different paths; both images must match byte for byte
make figures    rewrite the README figures block
make hooks      run analyse and test before every commit and check each commit message
make fuses      write lfuse 0xE4 and hfuse 0xD9 on the host, PROGRAMMER, PORT and MCU as needed
make flash      program the chip from the host, MCU=attiny4313 for that chip
make clean      remove build output
```

## Releases

semantic-release cuts a release from `main` after the `ci` workflow passes on a push, through [`.github/workflows/release.yml`](.github/workflows/release.yml). It releases only the commit CI verified, downloads the per-chip images that same CI run built and tested into a temporary directory, validates each with [`tools/check_hex.py`](tools/check_hex.py), and never rebuilds. Each release attaches both hex images with their SHA-256 files, the Sigstore bundle of its signed build provenance and an SPDX SBOM attested to both images, which meets SLSA Build Level 2. It then prepends the release notes to `CHANGELOG.md` and commits that file to `main` as `semantic-release-bot`, with the header `chore(release): <version> [skip ci]`, so the release commit starts no CI run and no second release; that commit is what lists semantic-release among the repository's contributors. Commit types decide the version, per [`.releaserc.json`](.releaserc.json): a breaking change is major, `feat` minor, `fix`, `perf` and `refactor` patch; `docs`, `test`, `build`, `ci`, `chore` and `style` release nothing. `v0.0.0` marks the history before automated releases. The release tooling is pinned in [`package.json`](package.json) and `pnpm-lock.yaml`; `conventional-changelog-conventionalcommits` stays on 9.x until `@semantic-release/release-notes-generator` accepts conventional-changelog-writer 9. Dependabot, per [`.github/dependabot.yml`](.github/dependabot.yml), groups weekly updates for actions, the Docker base image, the Python tools and the release tooling.

## Public repository

The repository is public. Secret scanning with push protection, Dependabot alerts and security updates, and private vulnerability reporting are on; [`SECURITY.md`](SECURITY.md) routes reports there. Rulesets forbid deleting or force-pushing `main` and deleting or moving `v*` tags. [`.github/workflows/codeql.yml`](.github/workflows/codeql.yml) runs CodeQL over the workflows, the C sources and the Python tools, and [`.github/workflows/scorecard.yml`](.github/workflows/scorecard.yml) publishes the OpenSSF Scorecard. Every file carries SPDX headers or a [`REUSE.toml`](REUSE.toml) annotation, and `make analyse` runs `reuse lint`. [`tools/commit_message.py`](tools/commit_message.py) checks every pushed commit header in CI and, after `make hooks`, locally. Nothing development-only is committed: `docs/` and `specs/` stay local, per hard rule 12. The GitHub description is copied from the README tagline and metrics, never written separately.

## Measuring a change

- **A refactor changes nothing, and the hex proves it.** Compare the SHA-256 of `build/attiny2313a/release/fd3206-write-unlock-attiny2313a.hex` before and after. Equal digests end the review; different ones mean the change is not a pure refactor and `make test` decides.
- **A mutant only counts if it builds.** When checking that a test catches a defect, confirm the mutated firmware compiled. A mutant rejected by `-Werror` produces no test output, which reads like silence, not like a catch.
- **Probe the registers before blaming the firmware.** When a scenario fails, print DDRB, PORTB and the pin state cycle by cycle from a small throwaway program linked against `tests/sim/board.c`. Twice now the defect was in the harness.
- **State the timing figure's origin.** An instruction count, a datasheet number and a simulation are three different claims. Say which one a number is.
- **Check the size of a coverage list, not only its result.** "0 instructions never executed" is only as strong as the list it was checked against. `wc -l build/attiny2313a/release/fd3206-write-unlock-attiny2313a.insn` belongs in any review of the coverage tooling.

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
| avr-ld keeps `.L` labels for relaxation, so objdump shows them as function headers; the coverage list stopped each function at its first local label and held 47 of 76 instructions | The suite still reported 0 instructions never executed | `.L` headers continue the enclosing function in `tools/list_instructions.py`, with a test |
| The simavr ioctl macros build their code from a `char`; on x86_64 `char` is signed, so CI failed `-Wsign-conversion` while the aarch64 build was clean | The same image passed every gate on the Mac | `make analyse` compiles the harness and host tests with both `-fsigned-char` and `-funsigned-char` |
| Moving register access behind assembly calls pushed the gate response past 10 us: three calls per main-loop pass plus the update | Every other scenario passed | the gate-response scenarios; the loop now makes one `fdswu_port_poll` call that services the watchdog and samples both ports |
| `_Noreturn`, valid C11, still broke MISRA: rule 1.4 lists it as an emergent feature | It compiled cleanly under `-pedantic-errors` | the MISRA gate in `make analyse`, calibrated against a planted `goto` |
| simavr re-raised every input with its pull-up enabled to high on each port or direction write, overriding the level the harness drove, so the heads never engaged once the inputs gained pull-ups | It looked like the pull-ups broke the gating | the harness declares the pins it drives with `AVR_IOCTL_IOPORT_SET_EXTERNAL`, which simavr gives priority over a pull-up, as real hardware does |
| The README said the ATtiny4313 runs the ATtiny2313A image; that image's start-up writes only SPL, so on the 4313, whose SPH resets to 1, the stack pointer lands at 0x1DF, past its RAM, and the first call crashes | The two chips share one datasheet and one pinout | each chip builds its own image, and the simavr suite runs each on its own core |
| The Arduino IDE toolchain for this part is avr-gcc 7.3, which has no C23 | The IDE route had worked for the gnu11 source | the IDE route was removed; hard rule 7 |

## Real hardware

Nothing in this repository can drive a drive or a programmer, so a hardware result exists only when someone reports one. Ask before assuming a hardware run happened. What only a drive settles:

| Question | Why simulation cannot |
|---|---|
| Does a single-file save still work with both chips writing the same heads | the FD3206P's own write stage is not modelled |
| Are the FD3206P head outputs open collector | the piggyback design assumes it and no document states it |
| Does a whole-disk write read back | the 2C33's decoding margin is not modelled |
| What the edge-to-head delay really is | simavr enters the interrupt without the input synchroniser delay |
| Do pins 14 and 15 idle at 5.5 V or less, and does a pulled head read 0.8 V or less | the head load and pull-ups sit on the drive board, which has no published schematic |
| Does the drive's power board also block writes | FMD-POWER-04, -05, some -02 boards and the Twin Famicom AN-500 power board carry their own write lockout, outside the FD3206P; see the power board steps in [`README.md`](README.md) |

## Emulators

simavr runs headless and opens nothing. Any other emulator used for a check runs headless too, with no window and no sound; a run someone asked to watch opens windowed and minimised, never full screen.

## What is not done

- Nothing has run on a drive.
- Whether single-file saves suffer from the FD3206P and the ATtiny writing the same heads out of phase is unknown. The classic GAL modchip has the same exposure.
