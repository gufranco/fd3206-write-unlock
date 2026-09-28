#!/usr/bin/env bash
set -euo pipefail

if [[ -z ${4:-} ]]; then
	printf 'usage: %s <avr-objdump> <avr-nm> <firmware.elf> <object>...\n' "$0" >&2
	exit 2
fi

objdump_tool=$1
nm_tool=$2
elf=$3
shift 3

functions=$("$nm_tool" --defined-only "$@" | awk '$2 == "T" || $2 == "t" { print $3 }' | sort -u)
if [[ -z $functions ]]; then
	printf 'no functions defined in %s\n' "$*" >&2
	exit 1
fi

header_line='^([0-9a-f]+) <([^>]+)>:$'
tab=$'\t'
instruction_line="^ *([0-9a-f]+):${tab}[^${tab}]*${tab}([^${tab} ]+)"
current=''

"$objdump_tool" -d "$elf" | while IFS= read -r line; do
	if [[ $line =~ $header_line ]]; then
		current=''
		if grep -qx -- "${BASH_REMATCH[2]}" <<<"$functions"; then
			current=${BASH_REMATCH[2]}
		fi
		continue
	fi
	if [[ -n $current && $line =~ $instruction_line && ${BASH_REMATCH[2]} != nop && ${BASH_REMATCH[2]} != .word ]]; then
		printf '%d %s\n' "0x${BASH_REMATCH[1]}" "$current"
	fi
done
