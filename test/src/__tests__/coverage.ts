import { execFileSync } from 'node:child_process';
import { readdirSync } from 'node:fs';
import { join } from 'node:path';

const FUNCTION_SYMBOL_TYPES = new Set(['T', 't']);
const FUNCTION_HEADER = /^([0-9a-f]+) <([^>]+)>:$/;
const INSTRUCTION_LINE = /^\s*([0-9a-f]+):\t(?:[0-9a-f]{4} ?){1,2}\s*\t(\S+)/;
const NON_EXECUTED_MNEMONICS = new Set(['.word', '.short', '.byte', 'nop']);

function toolPath(tool: string): string {
  const directory = process.env['ARM_TOOLCHAIN_BIN'];
  return directory === undefined ? tool : join(directory, tool);
}

function run(tool: string, args: readonly string[]): string {
  return execFileSync(toolPath(tool), args, { encoding: 'utf8', maxBuffer: 64 * 1024 * 1024 });
}

function definedFunctions(objectDirectory: string): ReadonlySet<string> {
  const objects = readdirSync(objectDirectory)
    .filter((name) => name.endsWith('.obj') || name.endsWith('.o'))
    .map((name) => join(objectDirectory, name));
  const symbols = run('arm-none-eabi-nm', ['--defined-only', ...objects])
    .split('\n')
    .map((line) => line.trim().split(/\s+/))
    .filter((fields) => fields.length === 3 && FUNCTION_SYMBOL_TYPES.has(fields[1] ?? ''))
    .map((fields) => fields[2] ?? '');
  return new Set(symbols);
}

export function firmwareInstructionAddresses(elfPath: string, objectDirectory: string): ReadonlyMap<number, string> {
  const functions = definedFunctions(objectDirectory);
  const disassembly = run('arm-none-eabi-objdump', ['-d', elfPath]).split('\n');
  let current: string | undefined;
  const entries = disassembly.flatMap((line): Array<readonly [number, string]> => {
    const header = FUNCTION_HEADER.exec(line);
    if (header !== null) {
      current = functions.has(header[2] ?? '') ? header[2] : undefined;
      return [];
    }
    const instruction = INSTRUCTION_LINE.exec(line);
    if (current === undefined || instruction === null || NON_EXECUTED_MNEMONICS.has(instruction[2] ?? '')) {
      return [];
    }
    return [[Number.parseInt(instruction[1] ?? '', 16), current] as const];
  });
  return new Map(entries);
}
