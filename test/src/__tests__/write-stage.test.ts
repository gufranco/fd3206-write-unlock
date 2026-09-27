import { resolve } from 'node:path';

import { afterEach, describe, expect, it } from 'vitest';

import { Board, NANOS_PER_CYCLE, Pin } from './board.js';
import { firmwareInstructionAddresses } from './coverage.js';

const BUILD_DIRECTORY = resolve(process.env['FIRMWARE_BUILD_DIR'] ?? '../build-emulator');
const FLASH_IMAGE = resolve(BUILD_DIRECTORY, 'fdswriteunlock.bin');
const FIRMWARE_ELF = resolve(BUILD_DIRECTORY, 'fdswriteunlock.elf');
const FIRMWARE_OBJECTS = resolve(BUILD_DIRECTORY, 'CMakeFiles/fdswriteunlock.dir/src');

const HALF_BIT_CELL_NANOS = 5187;
const FAST_HALF_BIT_CELL_NANOS = 4715;
const WRITE_PULSE_LOW_NANOS = 1000;
const SETTLE_NANOS = 5000;
const EDGE_LATENCY_LIMIT_NANOS = 3 * NANOS_PER_CYCLE;
const EDGE_JITTER_LIMIT_NANOS = NANOS_PER_CYCLE;
const GATE_RESPONSE_LIMIT_NANOS = 1000;
const IDLE_EDGE_COUNT = 100;
const WRITE_EDGE_COUNT = 1000;
const LOGGED_EDGE_COUNT = 200;
const SOAK_NANOS = 600_000_000;
const PHASE_SPREAD_CYCLES = 16;
const RANDOM_SEED = 0x3206;
const EVEN_IDLE_EDGES = 4;
const ODD_IDLE_EDGES = 5;
const SHORT_GATE_PULSE_NANOS = 200;
const RESPONSE_TIMEOUT_NANOS = 2_000_000;
const NANOS_PER_MICROSECOND = 1000;
const MICROS_PER_SECOND = 1_000_000;
const DURATION_TOLERANCE_MICROS = 3;
const WATCHDOG_CTRL = 0x40058000;
const WATCHDOG_ENABLE = 1 << 30;
const CONSOLE_TX_PIN = 0;
const GPIO_COUNT = 30;
const WRITE_REPORT = /write 1: (\d+) edges in (\d+) us, (\d+) edges\/s\r?\n/;
const STATUS_REPORT = /status: writing=(\w+) writes=(\d+) edges=(\d+)\r?\n/;

const HeadState = {
  None: 'none',
  Head1: 'head1',
  Head2: 'head2',
  Both: 'both',
} as const;

type HeadState = (typeof HeadState)[keyof typeof HeadState];

interface WriteConditions {
  readonly writeGate: boolean;
  readonly writeProtect: boolean;
  readonly ready: boolean;
}

const WRITING: WriteConditions = { writeGate: false, writeProtect: false, ready: false };
const GATE_CLOSED: WriteConditions = { ...WRITING, writeGate: true };
const PROTECTED: WriteConditions = { ...WRITING, writeProtect: true };
const NOT_READY: WriteConditions = { ...WRITING, ready: true };

let executedAddresses: ReadonlySet<number> = new Set();
let openBoards: readonly Board[] = [];
let randomState = RANDOM_SEED;

function nextRandom(): number {
  randomState ^= randomState << 13;
  randomState ^= randomState >>> 17;
  randomState ^= randomState << 5;
  return randomState >>> 0;
}

function bootedBoard(): Board {
  const board = new Board(FLASH_IMAGE);
  board.boot();
  openBoards = [...openBoards, board];
  return board;
}

function applyConditions(board: Board, conditions: WriteConditions): void {
  board.setLevel(Pin.WriteGate, conditions.writeGate);
  board.setLevel(Pin.WriteProtect, conditions.writeProtect);
  board.setLevel(Pin.Ready, conditions.ready);
  board.runNanos(SETTLE_NANOS);
}

function writePulse(board: Board, periodNanos: number): void {
  board.setLevel(Pin.WriteData, false);
  board.runNanos(WRITE_PULSE_LOW_NANOS);
  board.setLevel(Pin.WriteData, true);
  board.runNanos(periodNanos - WRITE_PULSE_LOW_NANOS);
}

function headState(board: Board): HeadState {
  const head1 = board.isDriven(Pin.Head1);
  const head2 = board.isDriven(Pin.Head2);
  if (head1 && head2) {
    return HeadState.Both;
  }
  if (head1) {
    return HeadState.Head1;
  }
  return head2 ? HeadState.Head2 : HeadState.None;
}

function headStatesAfterPulses(board: Board, count: number, periodNanos: number): readonly HeadState[] {
  return Array.from({ length: count }, () => {
    writePulse(board, periodNanos);
    return headState(board);
  });
}

function edgeLatencies(board: Board, count: number): readonly number[] {
  return Array.from({ length: count }, () => {
    board.runNanos((nextRandom() % PHASE_SPREAD_CYCLES) * NANOS_PER_CYCLE);
    const edgeAt = board.nanos;
    writePulse(board, HALF_BIT_CELL_NANOS);
    return board.lastHeadChangeNanos - edgeAt;
  });
}

function responseNanos(board: Board, change: () => void, settled: () => boolean): number {
  const changedAt = board.nanos;
  change();
  board.runUntil(settled, RESPONSE_TIMEOUT_NANOS);
  return board.lastHeadChangeNanos - changedAt;
}

function consoleResponse(board: Board, action: () => void, expected: RegExp): RegExpExecArray | null {
  action();
  board.runUntil(() => expected.test(board.console), RESPONSE_TIMEOUT_NANOS);
  board.runNanos(SETTLE_NANOS);
  return expected.exec(board.console);
}

function outputEnabledPins(board: Board): readonly number[] {
  return Array.from({ length: GPIO_COUNT }, (_, pin) => pin).filter((pin) => board.isOutputEnabled(pin));
}

afterEach(() => {
  executedAddresses = new Set([
    ...executedAddresses,
    ...openBoards.flatMap((board) => [...board.executedAddresses]),
  ]);
  openBoards = [];
});

describe('write gating', () => {
  it.each([
    ['write gate is inactive', GATE_CLOSED],
    ['disk is write protected', PROTECTED],
    ['drive is not ready', NOT_READY],
  ])('leaves both heads undriven while the %s', (_, conditions) => {
    const board = bootedBoard();
    applyConditions(board, conditions);

    const states = headStatesAfterPulses(board, IDLE_EDGE_COUNT, HALF_BIT_CELL_NANOS);

    expect(states.filter((state) => state !== HeadState.None)).toStrictEqual([]);
    expect(board.isOutputEnabled(Pin.Head1)).toBe(false);
    expect(board.isOutputEnabled(Pin.Head2)).toBe(false);
  });

  it.each([
    ['write gate closes', Pin.WriteGate],
    ['write protect asserts', Pin.WriteProtect],
    ['ready drops', Pin.Ready],
  ] as const)('releases the heads within 1 us when the %s', (_, pin) => {
    const board = bootedBoard();
    applyConditions(board, WRITING);
    writePulse(board, HALF_BIT_CELL_NANOS);
    const drivenBefore = headState(board);

    const elapsed = responseNanos(board, () => board.setLevel(pin, true), () => headState(board) === HeadState.None);

    expect(drivenBefore).not.toBe(HeadState.None);
    expect(headState(board)).toBe(HeadState.None);
    expect(elapsed).toBeLessThanOrEqual(GATE_RESPONSE_LIMIT_NANOS);
  });

  it.each([
    ['head 1 after an even number of idle edges', EVEN_IDLE_EDGES, HeadState.Head1],
    ['head 2 after an odd number of idle edges', ODD_IDLE_EDGES, HeadState.Head2],
  ] as const)('drives %s within 1 us of the gate opening', (_, idleEdges, expected) => {
    const board = bootedBoard();
    applyConditions(board, GATE_CLOSED);
    headStatesAfterPulses(board, idleEdges, HALF_BIT_CELL_NANOS);

    const elapsed = responseNanos(
      board,
      () => board.setLevel(Pin.WriteGate, false),
      () => headState(board) !== HeadState.None,
    );

    expect(headState(board)).toBe(expected);
    expect(elapsed).toBeLessThanOrEqual(GATE_RESPONSE_LIMIT_NANOS);
  });

  it('lights the activity LED only while writing', () => {
    const board = bootedBoard();
    applyConditions(board, WRITING);
    const litWhileWriting = board.isDriven(Pin.ActivityLed);

    applyConditions(board, GATE_CLOSED);

    expect(litWhileWriting).toBe(true);
    expect(board.isDriven(Pin.ActivityLed)).toBe(false);
  });
});

describe('write data toggling', () => {
  it('drives exactly one head and switches heads on every falling edge at the fastest data rate', () => {
    const board = bootedBoard();
    applyConditions(board, WRITING);
    const initial = headState(board);

    const states = headStatesAfterPulses(board, WRITE_EDGE_COUNT, FAST_HALF_BIT_CELL_NANOS);

    const sequence = [initial, ...states];
    const repeats = sequence.slice(1).filter((state, index) => state === sequence[index]);
    expect(initial).toBe(HeadState.Head1);
    expect(states.filter((state) => state !== HeadState.Head1 && state !== HeadState.Head2)).toStrictEqual([]);
    expect(repeats).toStrictEqual([]);
  });

  it('ignores rising edges of write data', () => {
    const board = bootedBoard();
    applyConditions(board, WRITING);
    board.setLevel(Pin.WriteData, false);
    board.runNanos(WRITE_PULSE_LOW_NANOS);
    const afterFall = headState(board);

    board.setLevel(Pin.WriteData, true);
    board.runNanos(HALF_BIT_CELL_NANOS);

    expect(afterFall).toBe(HeadState.Head2);
    expect(headState(board)).toBe(afterFall);
  });

  it('switches heads within three PIO cycles of a falling edge with at most one cycle of jitter', () => {
    const board = bootedBoard();
    applyConditions(board, WRITING);

    const latencies = edgeLatencies(board, WRITE_EDGE_COUNT);

    expect(Math.max(...latencies)).toBeLessThanOrEqual(EDGE_LATENCY_LIMIT_NANOS);
    expect(Math.max(...latencies) - Math.min(...latencies)).toBeLessThanOrEqual(EDGE_JITTER_LIMIT_NANOS);
  });
});

describe('start-up and supervision', () => {
  it('drives only the console transmit pin and the activity LED after start-up', () => {
    const board = bootedBoard();

    const outputs = outputEnabledPins(board);

    expect(outputs.filter((pin) => pin !== CONSOLE_TX_PIN)).toStrictEqual([Pin.ActivityLed]);
    expect(board.console).toContain('fdswriteunlock ready, power-on start');
  });

  it('keeps the watchdog armed through a long write without firing it', () => {
    const board = bootedBoard();
    applyConditions(board, WRITING);

    const states = headStatesAfterPulses(board, Math.floor(SOAK_NANOS / HALF_BIT_CELL_NANOS), HALF_BIT_CELL_NANOS);

    expect(board.readWord(WATCHDOG_CTRL) & WATCHDOG_ENABLE).toBe(WATCHDOG_ENABLE);
    expect(states.filter((state) => state === HeadState.None || state === HeadState.Both)).toStrictEqual([]);
    expect(board.emulatorMessages.filter((message) => /watchdog/i.test(message.text))).toStrictEqual([]);
    expect(board.console.match(/ready, /g)).toHaveLength(1);
  });
});

describe('console', () => {
  it('logs each finished write with its edge count, duration, and average edge rate', () => {
    const board = bootedBoard();
    const openedAt = board.nanos;
    applyConditions(board, WRITING);
    headStatesAfterPulses(board, LOGGED_EDGE_COUNT, HALF_BIT_CELL_NANOS);
    const closedAt = board.nanos;

    const logged = consoleResponse(board, () => board.setLevel(Pin.WriteGate, true), WRITE_REPORT);

    const [edges, durationMicros, rate] = (logged?.slice(1) ?? []).map(Number);
    const gateMicros = (closedAt - openedAt) / NANOS_PER_MICROSECOND;
    expect(edges).toBe(LOGGED_EDGE_COUNT);
    expect(Math.abs((durationMicros ?? 0) - gateMicros)).toBeLessThanOrEqual(DURATION_TOLERANCE_MICROS);
    expect(rate).toBe(Math.floor((LOGGED_EDGE_COUNT * MICROS_PER_SECOND) / (durationMicros ?? 1)));
  });

  it('ignores a write gate glitch shorter than the gate response time', () => {
    const board = bootedBoard();
    applyConditions(board, GATE_CLOSED);
    const quietSince = board.lastHeadChangeNanos;
    board.setLevel(Pin.WriteGate, false);
    board.runNanos(SHORT_GATE_PULSE_NANOS);

    board.setLevel(Pin.WriteGate, true);
    board.runNanos(RESPONSE_TIMEOUT_NANOS);

    expect(board.lastHeadChangeNanos).toBe(quietSince);
    expect(board.console).not.toContain('write 1:');
  });

  it('reports an ongoing write in the status line', () => {
    const board = bootedBoard();
    applyConditions(board, WRITING);

    const status = consoleResponse(board, () => board.send('s'), STATUS_REPORT);

    expect(status?.slice(1)).toStrictEqual(['yes', '1', '0']);
  });

  it('answers the status command with the write and edge totals', () => {
    const board = bootedBoard();
    applyConditions(board, WRITING);
    headStatesAfterPulses(board, LOGGED_EDGE_COUNT, HALF_BIT_CELL_NANOS);
    applyConditions(board, GATE_CLOSED);

    const status = consoleResponse(board, () => board.send('s'), STATUS_REPORT);

    expect(status?.slice(1)).toStrictEqual(['no', '1', String(LOGGED_EDGE_COUNT)]);
  });

  it('ignores console input other than the status command', () => {
    const board = bootedBoard();
    const before = board.console;

    board.send('x');
    board.runNanos(RESPONSE_TIMEOUT_NANOS);

    expect(board.console).toBe(before);
  });
});

describe('coverage', () => {
  it('executes every instruction of the firmware sources', () => {
    const instructions = firmwareInstructionAddresses(FIRMWARE_ELF, FIRMWARE_OBJECTS);

    const missed = [...instructions].filter(([address]) => !executedAddresses.has(address));

    expect(instructions.size).toBeGreaterThan(0);
    expect(missed.map(([address, name]) => `${name}@0x${address.toString(16)}`)).toStrictEqual([]);
  });
});
