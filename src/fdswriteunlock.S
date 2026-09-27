#include "pins.h"

#define rZero r1
#define rNextHeads r2
#define rToggle r3
#define rWriteEnabled r4
#define rSavedStatus r5
#define rIsrScratch r16
#define rScratch r17
#define rWriteConditions r18
#define rHeads r19

#define ALL_ENABLED 0xFF
#define CLOCK_PRESCALER_UNLOCK _BV(CLKPCE)
#define CLOCK_DIVIDE_BY_ONE 0
#define WATCHDOG_UNLOCK (_BV(WDCE) | _BV(WDE))
#define WATCHDOG_RESET_16MS _BV(WDE)
#define INT0_ON_FALLING_EDGE _BV(ISC01)

    .section .text

    .global INT0_vect
INT0_vect:
    out _SFR_IO_ADDR(DDRB), rNextHeads
    in rSavedStatus, _SFR_IO_ADDR(SREG)
    com rToggle
    ldi rIsrScratch, HEAD2_MASK
    sbrc rToggle, 0
    ldi rIsrScratch, HEAD1_MASK
    and rIsrScratch, rWriteEnabled
    mov rNextHeads, rIsrScratch
    out _SFR_IO_ADDR(SREG), rSavedStatus
    reti

    .global main
main:
    cli
    clr rZero
    rcall configure_clock
    rcall configure_watchdog
    rcall configure_ports
    clr rToggle
    clr rWriteEnabled
    clr rNextHeads
    rcall configure_write_data_interrupt
    sei
poll_write_conditions:
    wdr
    ldi rWriteConditions, ALL_ENABLED
    sbic _SFR_IO_ADDR(READY_PIN), READY_BIT
    clr rWriteConditions
    sbic _SFR_IO_ADDR(WRITE_PROTECT_PIN), WRITE_PROTECT_BIT
    clr rWriteConditions
    sbic _SFR_IO_ADDR(WRITE_GATE_PIN), WRITE_GATE_BIT
    clr rWriteConditions
    cp rWriteConditions, rWriteEnabled
    breq poll_write_conditions
    tst rWriteConditions
    breq release_heads
engage_heads:
    cli
    ldi rHeads, HEAD1_MASK
    sbrc rToggle, 0
    ldi rHeads, HEAD2_MASK
    out _SFR_IO_ADDR(DDRB), rHeads
    ldi rHeads, HEAD2_MASK
    sbrc rToggle, 0
    ldi rHeads, HEAD1_MASK
    mov rNextHeads, rHeads
    mov rWriteEnabled, rWriteConditions
    sei
    rjmp poll_write_conditions
release_heads:
    out _SFR_IO_ADDR(DDRB), rZero
    cli
    clr rNextHeads
    clr rWriteEnabled
    out _SFR_IO_ADDR(DDRB), rZero
    sei
    rjmp poll_write_conditions

configure_clock:
    ldi rScratch, CLOCK_PRESCALER_UNLOCK
    ldi rIsrScratch, CLOCK_DIVIDE_BY_ONE
    sts _SFR_MEM_ADDR(CLKPR), rScratch
    sts _SFR_MEM_ADDR(CLKPR), rIsrScratch
    ret

configure_watchdog:
    wdr
    sts _SFR_MEM_ADDR(MCUSR), rZero
    ldi rScratch, WATCHDOG_UNLOCK
    ldi rIsrScratch, WATCHDOG_RESET_16MS
    sts _SFR_MEM_ADDR(WATCHDOG_REG), rScratch
    sts _SFR_MEM_ADDR(WATCHDOG_REG), rIsrScratch
    ret

configure_ports:
    out _SFR_IO_ADDR(DDRB), rZero
    out _SFR_IO_ADDR(DDRD), rZero
    ldi rScratch, UNUSED_PULLUPS_B
    out _SFR_IO_ADDR(PORTB), rScratch
    ldi rScratch, UNUSED_PULLUPS_D
    out _SFR_IO_ADDR(PORTD), rScratch
#ifdef UNUSED_PULLUPS_A
    out _SFR_IO_ADDR(DDRA), rZero
    ldi rScratch, UNUSED_PULLUPS_A
    out _SFR_IO_ADDR(PORTA), rScratch
#endif
#ifdef UNUSED_PULLUPS_C
    out _SFR_IO_ADDR(DDRC), rZero
    ldi rScratch, UNUSED_PULLUPS_C
    out _SFR_IO_ADDR(PORTC), rScratch
#endif
    ret

configure_write_data_interrupt:
    ldi rScratch, INT0_ON_FALLING_EDGE
    sts _SFR_MEM_ADDR(INT0_SENSE_REG), rScratch
    ldi rScratch, _BV(INTF0)
    sts _SFR_MEM_ADDR(EIFR), rScratch
    ldi rScratch, _BV(INT0)
    sts _SFR_MEM_ADDR(INT0_ENABLE_REG), rScratch
    ret

    .global firmware_end
firmware_end:
