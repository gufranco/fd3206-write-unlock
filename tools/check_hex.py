# SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com>
# SPDX-License-Identifier: MIT

import re
import sys
from dataclasses import dataclass
from pathlib import Path

RECORD = re.compile(r"^:((?:[0-9A-Fa-f]{2}){5,})$")
DATA_TYPE = 0x00
EOF_TYPE = 0x01
HEADER_BYTES = 4
ARGUMENT_COUNT = 4
MAX_BYTES_FLAG = "--max-bytes"
USAGE_ERROR = 2


class HexError(ValueError):
    pass


@dataclass(frozen=True, slots=True)
class Image:
    data_bytes: int
    end_address: int


@dataclass(frozen=True, slots=True)
class Record:
    kind: int
    address: int
    data: bytes


def decode(number: int, line: str) -> Record:
    match = RECORD.match(line)
    if not match:
        raise HexError(f"line {number}: not a record")
    raw = bytes.fromhex(match.group(1))
    if raw[0] != len(raw) - HEADER_BYTES - 1:
        raise HexError(f"line {number}: length byte {raw[0]} for {len(raw)} bytes")
    if sum(raw) & 0xFF:
        raise HexError(f"line {number}: checksum")
    kind = raw[3]
    if kind not in {DATA_TYPE, EOF_TYPE}:
        raise HexError(f"line {number}: record type {kind:02X}")
    return Record(kind, int.from_bytes(raw[1:3], "big"), raw[HEADER_BYTES:-1])


def parse(text: str) -> Image:
    lines = [line.strip() for line in text.splitlines() if line.strip()]
    records = [decode(number, line) for number, line in enumerate(lines, start=1)]
    ends = [index for index, record in enumerate(records) if record.kind == EOF_TYPE]
    if not ends:
        raise HexError("no end-of-file record")
    if ends[0] != len(records) - 1:
        raise HexError(f"line {ends[0] + 2}: after end-of-file")
    data = [record for record in records if record.kind == DATA_TYPE]
    return Image(
        data_bytes=sum(len(record.data) for record in data),
        end_address=max(
            (record.address + len(record.data) for record in data), default=0
        ),
    )


def main(arguments: list[str]) -> int:
    if len(arguments) != ARGUMENT_COUNT or arguments[2] != MAX_BYTES_FLAG:
        print("usage: check_hex.py <firmware.hex> --max-bytes <n>", file=sys.stderr)
        return USAGE_ERROR
    path = Path(arguments[1])
    limit = int(arguments[3])
    try:
        image = parse(path.read_text())
    except OSError as error:
        print(f"{path}: cannot read: {error}", file=sys.stderr)
        return 1
    except HexError as error:
        print(f"{path}: {error}", file=sys.stderr)
        return 1
    if image.end_address > limit:
        print(
            f"{path}: ends at byte {image.end_address}, past the {limit}-byte flash",
            file=sys.stderr,
        )
        return 1
    print(f"{path}: {image.data_bytes} bytes, valid Intel HEX within {limit} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
