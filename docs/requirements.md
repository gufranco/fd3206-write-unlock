# Write Stage Requirements

The firmware drives the two write-head lines of a Famicom Disk System drive that uses the Mitsumi FD3206P controller, so that every write the RAM adapter requests reaches the disk. Each requirement below has a matching test in [`test/test_write_stage.c`](../test/test_write_stage.c).

## Signals

| Signal | Active level | FD3206P pin | ATtiny2313A pin |
|---|---|---|---|
| WRITE DATA | falling edge | 6 | 6, PD2 INT0 |
| /WRITE GATE | low | 4 | 4, PA1 |
| /WRITE PROTECT | low | 5 | 5, PA0 |
| /READY | low | 13 | 13, PB1 |
| Head 2 | low drives the head | 14 | 14, PB2 |
| Head 1 | low drives the head | 15 | 15, PB3 |

The ATtiny sits on the FD3206P, so each pin number is the same on both chips.

## Requirements

### Requirement: both head pins MUST be inputs unless /READY, /WRITE PROTECT and /WRITE GATE are all low
#### Scenario: any one condition inactive
- GIVEN one of the three conditions high and the other two low
- WHEN 100 falling edges arrive on WRITE DATA
- THEN both head pins are inputs after every edge

### Requirement: while writing, exactly one head pin MUST be low, and the low pin MUST change on every falling edge of WRITE DATA
#### Scenario: fastest legal data rate
- GIVEN all three conditions low
- WHEN 1000 falling edges arrive 4.7 us apart
- THEN after every edge exactly one head pin is low, and it is the other pin from the previous edge

### Requirement: a rising edge of WRITE DATA MUST NOT change the heads
#### Scenario: 1 us low pulse
- GIVEN all conditions low
- WHEN WRITE DATA falls, stays low 1 us, then rises
- THEN the heads change once, at the fall

### Requirement: the heads MUST be released within 10 us of any condition going high
#### Scenario: each condition, with no further data edges
- GIVEN one head pin low
- WHEN /WRITE GATE, /WRITE PROTECT or /READY goes high
- THEN both head pins are inputs within 10 us

Ten microseconds is one bit cell. A gate change falls in a gap between blocks of at least 480 bits, so the delay only trims or extends that gap.

### Requirement: exactly one head MUST be driven within 10 us of all conditions becoming low
#### Scenario: gate opens
- GIVEN /WRITE PROTECT and /READY low and the gate closed
- WHEN /WRITE GATE goes low
- THEN exactly one head pin is low within 10 us

### Requirement: the heads MUST end released after a write gate glitch
#### Scenario: 250 ns gate pulse
- GIVEN the gate closed
- WHEN /WRITE GATE goes low for 250 ns
- THEN both head pins are inputs afterwards

### Requirement: after start-up no pin SHALL be an output, and no pin soldered to the FD3206P SHALL have a pull-up
#### Scenario: idle inputs
- GIVEN all inputs high
- WHEN start-up finishes
- THEN every data direction register is zero, and the pull-ups on WRITE DATA, the three condition pins and both head pins are off

### Requirement: the firmware MUST run undivided from its clock and keep the watchdog armed
#### Scenario: after start-up
- GIVEN the firmware started
- WHEN the clock prescaler and watchdog registers are read
- THEN the prescaler is 1 and the watchdog enable bit is set

### Requirement: the watchdog MUST NOT fire during normal operation
#### Scenario: 600 ms write, ten times the 60 ms timeout
- GIVEN all conditions low
- WHEN edges arrive for 600 ms
- THEN no reset occurs and exactly one head pin is low after every edge

## Sources of the hardware facts

- FD3206P pads for +5 V, GND, /READY, /WRITE GATE, /WRITE PROTECT and WRITE DATA, and the head traces beside pins 14 and 15: the labelled solder-side photos in the Famicom World article "Famicom Disk System FD3206 Write Mod".
- Head drive behaviour, one head per flip-flop state and both released unless all three conditions hold: the 74LS76 and 74LS45 schematic in the same article.
- 96.4 kHz bit rate, 10 percent tolerance, about 1 us pulses, gaps of at least 480 bits: Brad Taylor, "Famicom Disk System technical reference", and the nesdev wiki "FDS disk format".
- ATtiny2313A pinout, INT0, GPIO registers, fuses: Microchip document 8246 and the avrdude 8 part database.
