# MISRA C:2012 Deviations (as of 2026-09-28)

`make misra` runs the cppcheck 2.22 MISRA addon over the firmware. It is advisory: MISRA C:2012 predates C23, and the addon cannot see through avr-libc's register and inline-assembly macros. Every finding it reports is listed here with its reason. A finding not in this table is a defect to fix.

Status: proposed, awaiting approval by the repository owner.

| Rule | Where | Count | Reason | Alternative considered |
|---|---|---|---|---|
| 11.4, integer converted to pointer | `src/main.c`, `src/assert.c`, every register access | 17 | avr-libc defines each I/O register as a dereferenced integer address. There is no other way to reach a register in C | Writing registers only from assembly: moves the same conversion out of the checker's sight and adds code |
| 17.3, function declared implicitly | `sei()`, `cli()`, `wdt_reset()` | 5 | These are avr-libc macros that expand to inline assembly; the addon does not expand `__asm__` and reports a call to an undeclared function. The compiler, with `-Werror` and C23 rules, rejects any real implicit declaration | none needed |
| 5.9, internal-linkage identifier reused | `include/fdswu/pins.h` | 7 | `static constexpr` constants in a header are one definition seen by several translation units, the C23 idiom for typed constants. See [ADR 0002](adr/0002-constants.md) | `#define`, which Power of 10 rule 8 limits; `enum`, whose constants are `int` and break rule 10.4 instead |
| 8.9, object could be defined at block scope | `include/fdswu/pins.h`, `src/main.c` constants | 11 | Named file-scope constants are the single source of truth for pin masks and register bits; moving each into the one function that uses it today splits the pin map across files | Block-scope constants: rejected for the reason given |

Fixed rather than deviated, on 2026-09-28: rules 10.1 and 10.4 on `_BV` shifts, now `constexpr uint8_t` masks; rules 12.3 and 14.2 on `ATOMIC_BLOCK`, whose hidden `for` loop and comma operator became an explicit `cli`, update, `sei`.

## Waiver record

- rule: MISRA C:2012 rules 11.4, 17.3, 5.9 and 8.9
- scope: the lines listed in the table above, in `src/main.c`, `src/assert.c` and `include/fdswu/pins.h`
- approved_by: pending
- rationale: as stated per row
- alternatives: as stated per row
- risk: a real rule 11.4 or 17.3 defect in new code would hide among the listed findings; the count per rule is recorded so a new finding changes the count
- revisit: when cppcheck's MISRA addon supports C23, or when MISRA C:2025 tooling is available
