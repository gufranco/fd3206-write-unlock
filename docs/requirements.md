# Write Stage Requirements

The firmware drives the two write-head lines of a Famicom Disk System drive that uses the Mitsumi FD3206P controller, so that every write the RAM adapter requests reaches the disk. Each requirement below has a matching test in [`test/src/__tests__/write-stage.test.ts`](../test/src/__tests__/write-stage.test.ts).

## Signals

| Signal | Active level | FD3206P pin | RP2040 pin |
|---|---|---|---|
| WRITE DATA | falling edge | 6 | GP3 |
| /WRITE GATE | low | 4 | GP4 |
| /WRITE PROTECT | low | 5 | GP5 |
| /READY | low | 13 | GP6 |
| Head 1 drive | high turns the transistor on | 15, head side | GP7 |
| Head 2 drive | high turns the transistor on | 14, head side | GP8 |

## Requirements

### Requirement: the head outputs MUST stay disabled unless /READY, /WRITE PROTECT and /WRITE GATE are all low
#### Scenario: any one condition inactive
- GIVEN one of the three conditions high and the other two low
- WHEN 100 falling edges arrive on WRITE DATA
- THEN neither head output is enabled after any edge

### Requirement: while writing, exactly one head MUST be driven, and the driven head MUST change on every falling edge of WRITE DATA
#### Scenario: fastest legal data rate
- GIVEN all three conditions low
- WHEN 1000 falling edges arrive 4.7 us apart
- THEN after every edge exactly one head is driven, and it is the other head from the previous edge

### Requirement: a rising edge of WRITE DATA MUST NOT change the heads
#### Scenario: 1 us low pulse
- GIVEN all conditions low
- WHEN WRITE DATA falls, stays low 1 us, then rises
- THEN the heads change once, at the fall

### Requirement: the head MUST switch within 24 ns of a falling edge, with at most 8 ns of variation
#### Scenario: edges at random phase
- GIVEN all conditions low
- WHEN 1000 edges arrive at random phase relative to the CPU
- THEN every edge-to-head delay is at most 24 ns and the spread is at most 8 ns

The emulator resolves PIO pin waits immediately, so this test proves the switch happens in the PIO without CPU involvement, not the silicon delay. The silicon delay is one to two PIO cycles and is checked with a logic analyzer on hardware.

### Requirement: the heads MUST be released within 1 us of any condition going high
#### Scenario: each condition, with no further data edges
- GIVEN one head driven
- WHEN /WRITE GATE, /WRITE PROTECT or /READY goes high
- THEN both head outputs are disabled within 1 us

### Requirement: when all conditions become low, the head selected by the edge parity MUST be driven within 1 us
#### Scenario: even edge count
- GIVEN 4 falling edges arrived while the gate was closed
- WHEN /WRITE GATE goes low
- THEN head 1 is driven within 1 us

#### Scenario: odd edge count
- GIVEN 5 falling edges arrived while the gate was closed
- WHEN /WRITE GATE goes low
- THEN head 2 is driven within 1 us

### Requirement: a write gate glitch shorter than the gate response time MUST NOT drive a head
#### Scenario: 200 ns gate pulse
- GIVEN the gate closed
- WHEN /WRITE GATE goes low for 200 ns
- THEN no head output changes and no write is logged

### Requirement: after start-up only the console transmit pin and the activity LED SHALL be outputs
#### Scenario: idle inputs
- GIVEN all inputs high
- WHEN start-up finishes
- THEN no other pin has its output enabled, and the console reports a power-on start

### Requirement: the activity LED MUST be lit exactly while writing
#### Scenario: open then close the gate
- GIVEN the firmware running
- WHEN the conditions go low and then the gate closes
- THEN the LED is lit while writing and off afterwards

### Requirement: the watchdog MUST be armed and MUST NOT fire during normal operation
#### Scenario: 600 ms write, longer than the 250 ms timeout
- GIVEN all conditions low
- WHEN edges arrive for 600 ms
- THEN the watchdog enable bit is set, it never fires, and exactly one ready line was printed

### Requirement: each finished write MUST be logged with its edge count, duration and average edge rate
#### Scenario: 200 edges
- GIVEN the gate open and 200 falling edges
- WHEN the gate closes
- THEN the console prints 200 edges, a duration within 3 us of the gate time, and the rate that count and duration give

### Requirement: the status command MUST report writing state, write count and edge total
#### Scenario: idle after one write
- GIVEN one finished write of 200 edges
- WHEN `s` is received
- THEN the console prints `writing=no writes=1 edges=200`

#### Scenario: during a write
- GIVEN the gate open and no edges yet
- WHEN `s` is received
- THEN the console prints `writing=yes writes=1 edges=0`

### Requirement: console input other than the status command MUST be ignored
#### Scenario: unknown character
- GIVEN the firmware idle
- WHEN `x` is received
- THEN nothing is printed

## Sources of the hardware facts

- FD3206P pads for +5 V, GND, /READY, /WRITE GATE, /WRITE PROTECT and WRITE DATA, and the head traces beside pins 14 and 15: the labelled solder-side photos in the Famicom World article "Famicom Disk System FD3206 Write Mod".
- Head drive behaviour, one head per flip-flop state and both released unless all three conditions hold: the 74LS76 and 74LS45 schematic in the same article.
- 96.4 kHz bit rate, 10 percent tolerance, about 1 us pulses: Brad Taylor, "Famicom Disk System technical reference", nesdev.org.
- RP2040 PIO, GPIO override, IO bank interrupts and PWM edge counting: the RP2040 datasheet and pico-sdk 2.3.1.
