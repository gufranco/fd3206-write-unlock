# Write Stage Requirements

The firmware drives the two write-head lines of a Famicom Disk System drive that uses the Mitsumi FD3206P controller, so that every write the RAM adapter requests reaches the disk.

## Inputs and outputs

| Signal | Active level | FD3206P pin | Role |
|---|---|---|---|
| WRITE DATA | falling edge | 6 | One head transition per falling edge |
| /WRITE GATE | low | 4 | The RAM adapter is writing |
| /WRITE PROTECT | low | 5 | The disk accepts writes |
| /READY | low | 13 | The head is on the recording area |
| HEAD 1 | pulled low | 15 | Write winding 1 |
| HEAD 2 | pulled low | 14 | Write winding 2 |

## Requirements

### Requirement: the heads MUST float unless /READY, /WRITE PROTECT and /WRITE GATE are all low
#### Scenario: gate inactive
- GIVEN /READY low, /WRITE PROTECT low, /WRITE GATE high
- WHEN 100 falling edges arrive on WRITE DATA
- THEN neither head line is driven after any edge

#### Scenario: write protected
- GIVEN /READY low, /WRITE GATE low, /WRITE PROTECT high
- WHEN 100 falling edges arrive
- THEN neither head line is driven

#### Scenario: not ready
- GIVEN /WRITE GATE low, /WRITE PROTECT low, /READY high
- WHEN 100 falling edges arrive
- THEN neither head line is driven

### Requirement: while writing, exactly one head MUST be pulled low, and the pulled head MUST change on every falling edge of WRITE DATA
#### Scenario: fastest legal data rate
- GIVEN all three write conditions active
- WHEN 1000 falling edges arrive 4.7 us apart
- THEN after every edge exactly one head is pulled low and it is the other head from the previous edge

### Requirement: a rising edge of WRITE DATA MUST NOT change the heads
#### Scenario: 1 us low pulse
- GIVEN all write conditions active
- WHEN WRITE DATA falls, stays low 1 us, then rises
- THEN the heads change once, at the fall

### Requirement: head lines MUST only be pulled low or left floating, never driven high
#### Scenario: any state
- GIVEN any combination of inputs
- WHEN the head outputs are inspected
- THEN their output latch is low and only their direction changes

### Requirement: the head MUST switch within 1.5 us of a falling edge, with at most 0.375 us of variation
#### Scenario: edges at random phase
- GIVEN all write conditions active
- WHEN 1000 edges arrive at random phase relative to the firmware loop
- THEN every edge-to-head delay is at most 1.5 us and the spread is at most 0.375 us

### Requirement: the heads MUST float within 5 us of any write condition going inactive, with no further data edges
#### Scenario: each condition
- GIVEN one head pulled low
- WHEN /WRITE GATE, /WRITE PROTECT or /READY goes high and WRITE DATA stays idle
- THEN both heads float within 5 us

### Requirement: when all write conditions become active, the head selected by the edge parity MUST be pulled low within 5 us
#### Scenario: even edge count
- GIVEN 4 falling edges arrived while the gate was closed
- WHEN /WRITE GATE goes low
- THEN head 1 is pulled low within 5 us

#### Scenario: odd edge count
- GIVEN 5 falling edges arrived while the gate was closed
- WHEN /WRITE GATE goes low
- THEN head 2 is pulled low within 5 us

### Requirement: after power-up no pin SHALL be driven
#### Scenario: start-up
- GIVEN power applied and the gate closed
- WHEN start-up finishes
- THEN every port direction register is zero

### Requirement: the firmware MUST run undivided from its clock and keep a watchdog armed
#### Scenario: long write
- GIVEN all write conditions active
- WHEN 20000 edges arrive over about 100 ms
- THEN the clock prescaler is 1, the watchdog enable bit is set and no reset occurs

## Sources of the hardware facts

- Controller pads for +5V, GND, /READY, /WRITE GATE, /WRITE PROTECT and WRITE DATA, and the head traces beside pins 14 and 15: the labelled solder-side photos in the Famicom World article "Famicom Disk System FD3206 Write Mod".
- Head drive behaviour, one head per flip-flop state and both floating unless all three conditions hold: the 74LS76 plus 74LS45 schematic in the same article.
- 96.4 kHz bit rate, 10 percent tolerance, about 1 us pulses: Brad Taylor, "Famicom Disk System technical reference", nesdev.org.
- ATtiny2313A pinout and electrical limits: Microchip document 8246. Fuse meanings: the avrdude 8 part database.
