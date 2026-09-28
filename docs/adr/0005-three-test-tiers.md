# 0005. Three test tiers with coverage gates

Date: 2026-09-28. Status: accepted.

## Context

The simulation suite runs the real release image, but it cannot turn assertions on without changing that image, and it measures instruction coverage, not branch coverage of the C logic. The Python tools decide what the coverage and figures checks mean, so a defect in them silently weakens every other gate.

## Decision

| Tier | Runs | Gate |
|---|---|---|
| Host | `tests/host/host_test.c` against `src/heads.c` and `src/conditions.c`, built with `FDSWU_DEBUG` and `--coverage` | gcovr 100 percent line and branch; the never-taken arm of `FDSWU_ASSERT` is excluded by pattern |
| Simulation | `tests/sim/sim_test.c` runs the release ELF in simavr 1.6: 13 scenarios | every instruction of every firmware function executed |
| Tools | `tests/tools/` against every script in `tools/` | coverage.py 100 percent line and branch |

The host tier checks the conditions function against an independent expression for all 65,536 port combinations and the heads plan for all four input combinations.

## Consequences

- A gate that drops below its threshold fails `make test` or `make analyse`, never only reports.
- The coverage list for the simulation tier is generated from the objects, so a new function is covered by default.
