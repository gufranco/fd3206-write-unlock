import { readFileSync } from 'node:fs';

import { Simulator } from 'rp2040js';

export const Pin = {
  WriteData: 3,
  WriteGate: 4,
  WriteProtect: 5,
  Ready: 6,
  Head1: 7,
  Head2: 8,
  ActivityLed: 25,
} as const;

export type PinNumber = (typeof Pin)[keyof typeof Pin];

export const NANOS_PER_CYCLE = 8;

const FLASH_VECTOR_TABLE = 0x10000100;
const RESET_VECTOR_OFFSET = 4;
const THUMB_BIT_MASK = 0xfffffffe;
const HEAD_PINS = [Pin.Head1, Pin.Head2] as const;
const IDLE_HIGH_INPUTS = [Pin.WriteData, Pin.WriteGate, Pin.WriteProtect, Pin.Ready] as const;
const BOOT_LIMIT_NANOS = 5_000_000;
const MAX_RECORDED_MESSAGES = 256;
const WATCHDOG_PERIPHERAL = 0x40058;
const READY_LINE = /ready, [^\n]*\n/;

export interface EmulatorMessage {
  readonly level: 'debug' | 'info' | 'warn' | 'error';
  readonly component: string;
  readonly text: string;
}

export class WatchdogResetError extends Error {
  constructor(atNanos: number) {
    super(`watchdog reset the chip at ${atNanos} ns`);
    this.name = 'WatchdogResetError';
  }
}

export class Board {
  private readonly simulator = new Simulator();
  private readonly executed = new Set<number>();
  private consoleText = '';
  private headChangedAt = 0;
  private messages: readonly EmulatorMessage[] = [];

  constructor(flashImagePath: string) {
    const mcu = this.simulator.rp2040;
    mcu.logger = {
      debug: (component, text) => this.record('debug', component, text),
      info: (component, text) => this.record('info', component, text),
      warn: (component, text) => this.record('warn', component, text),
      error: (component, text) => this.record('error', component, text),
    };
    const watchdog: object | undefined = mcu.peripherals[WATCHDOG_PERIPHERAL];
    if (watchdog === undefined || !('onWatchdogTrigger' in watchdog)) {
      throw new Error('rp2040js no longer exposes the watchdog reset hook');
    }
    watchdog.onWatchdogTrigger = () => {
      throw new WatchdogResetError(this.nanos);
    };
    mcu.flash.set(readFileSync(flashImagePath), 0);
    for (const pio of mcu.pio) {
      pio.run = () => undefined;
    }
    const enterException = mcu.core.exceptionEntry.bind(mcu.core);
    mcu.core.exceptionEntry = (exceptionNumber: number) => {
      enterException(exceptionNumber);
      this.executed.add(mcu.core.PC & THUMB_BIT_MASK);
    };
    mcu.core.VTOR = FLASH_VECTOR_TABLE;
    mcu.core.SP = mcu.readUint32(FLASH_VECTOR_TABLE);
    mcu.core.PC = mcu.readUint32(FLASH_VECTOR_TABLE + RESET_VECTOR_OFFSET) & THUMB_BIT_MASK;
    mcu.uart[0]!.onByte = (value) => {
      this.consoleText += String.fromCharCode(value);
    };
    for (const pin of HEAD_PINS) {
      mcu.gpio[pin]!.addListener(() => {
        this.headChangedAt = this.nanos;
      });
    }
    for (const pin of IDLE_HIGH_INPUTS) {
      this.setLevel(pin, true);
    }
  }

  boot(): void {
    if (!this.runUntil(() => READY_LINE.test(this.console), BOOT_LIMIT_NANOS)) {
      throw new Error(`firmware did not announce itself within ${BOOT_LIMIT_NANOS} ns: ${this.console}`);
    }
    for (const gpio of this.simulator.rp2040.gpio) {
      gpio.refreshInput();
    }
  }

  get emulatorMessages(): readonly EmulatorMessage[] {
    return this.messages;
  }

  readWord(address: number): number {
    return this.simulator.rp2040.readUint32(address);
  }

  get nanos(): number {
    return this.simulator.clock.nanos;
  }

  get console(): string {
    return this.consoleText;
  }

  get lastHeadChangeNanos(): number {
    return this.headChangedAt;
  }

  get executedAddresses(): ReadonlySet<number> {
    return this.executed;
  }

  setLevel(pin: PinNumber, high: boolean): void {
    this.simulator.rp2040.gpio[pin]!.setInputValue(high);
  }

  isDriven(pin: PinNumber): boolean {
    const gpio = this.simulator.rp2040.gpio[pin]!;
    return gpio.outputEnable && gpio.outputValue;
  }

  isOutputEnabled(pin: number): boolean {
    return this.simulator.rp2040.gpio[pin]!.outputEnable;
  }

  send(text: string): void {
    for (const character of text) {
      this.simulator.rp2040.uart[0]!.feedByte(character.charCodeAt(0));
    }
  }

  runNanos(duration: number): void {
    const end = this.nanos + duration;
    while (this.nanos < end) {
      this.step();
    }
  }

  runUntil(condition: () => boolean, limitNanos: number): boolean {
    const end = this.nanos + limitNanos;
    while (this.nanos < end) {
      if (condition()) {
        return true;
      }
      this.step();
    }
    return condition();
  }

  private record(level: EmulatorMessage['level'], component: string, text: string): void {
    if (this.messages.length >= MAX_RECORDED_MESSAGES) {
      return;
    }
    this.messages = [...this.messages, { level, component, text }];
  }

  private step(): void {
    const core = this.simulator.rp2040.core;
    if (core.waiting) {
      this.advance(1);
      return;
    }
    this.executed.add(core.PC & THUMB_BIT_MASK);
    this.advance(core.executeInstruction());
  }

  private advance(cycles: number): void {
    const mcu = this.simulator.rp2040;
    for (let cycle = 0; cycle < cycles; cycle++) {
      for (const pio of mcu.pio) {
        pio.step();
      }
      this.simulator.clock.tick(NANOS_PER_CYCLE);
    }
  }
}
