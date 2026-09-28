# 0001. C23 in a pinned Docker toolchain, no Arduino IDE build

Date: 2026-09-28. Status: accepted.

## Context

The firmware was gnu11 so that ATTinyCore could build it from the Arduino IDE. ATTinyCore 1.5.2 ships avr-gcc 7.3.0, which has no C23: no `constexpr`, no `bool` keyword, no `[[nodiscard]]` or `[[noreturn]]`, no digit separators. The owner asked for C23 and a quality bar that includes static analysis, coverage and reproducible tools. A local toolchain differs by machine: macOS and Debian ship different avr-gcc, simavr and cppcheck versions, and cppcheck understands `--std=c23` only from 2.22.

## Decision

- The language is C23, compiled by avr-gcc 14.2 from Debian trixie.
- Every build, check and test runs in one image: Debian trixie pinned by digest, packages pinned by version, cppcheck 2.22.0 built from a SHA-256-checked tag, Python tools pinned by hash.
- The image tag is a hash of `Dockerfile` and `docker/requirements-tools.txt`, so any toolchain change builds a new image and never reuses a stale one.
- `make` on the host delegates to the container through `tools/docker_make.py`. Only `fuses`, `flash`, `hooks` and `clean` run on the host.
- The Arduino IDE is still usable to load ArduinoISP onto an Arduino that then programs the ATtiny through `avrdude`.

## Alternatives rejected

| Option | Why not |
|---|---|
| Stay on gnu11 for the IDE | The owner chose C23; gnu11 has no `constexpr` or attributes |
| Newer ATTinyCore 2.x | Unreleased as a stable board package, and still not C23 |
| Host toolchains through Homebrew and apt | Versions drift by machine; macOS has no packaged simavr 1.6 or cppcheck 2.22 |

## Consequences

- Docker is a hard requirement for development.
- CI runs the same image, so a local pass and a CI pass mean the same thing.
- The first build takes minutes while cppcheck compiles; later builds reuse the image.
