#!/usr/bin/env bash
set -euo pipefail

if [[ -z ${3:-} || -n ${4:-} ]]; then
	printf 'usage: %s <avr-objdump> <avr-nm> <firmware.elf>\n' "$0" >&2
	exit 2
fi

objdump_tool=$1
nm_tool=$2
elf=$3

symbol_address() {
	local address
	address=$("$nm_tool" "$elf" | awk -v name="$1" '$3 == name { print $1 }')
	if [[ -z $address ]]; then
		printf 'symbol %s not found in %s\n' "$1" "$elf" >&2
		exit 1
	fi
	printf '%d\n' "0x$address"
}

first=$(symbol_address __vector_1)
last=$(symbol_address firmware_end)
instruction_line='^ *([0-9a-f]+):'

"$objdump_tool" -d "$elf" | while IFS= read -r line; do
	if [[ $line =~ $instruction_line ]]; then
		address=$(printf '%d' "0x${BASH_REMATCH[1]}")
		if ((address >= first && address < last)); then
			printf '%d\n' "$address"
		fi
	fi
done
