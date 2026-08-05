#!/usr/bin/env bash
# Reproduce a straight ascending insert run against a fresh db.bin.
#
# Usage:
#   tests/insert_range.sh <row_count> [binary_path]
#
# Example:
#   tests/insert_range.sh 27
set -euo pipefail

ROW_COUNT="${1:?Usage: $0 <row_count> [binary_path]}"
BINARY="${2:-cmake-build-debug/database_in_c}"

if [[ ! -x "$BINARY" ]]; then
    echo "Binary not found or not executable: $BINARY" >&2
    echo "Build the project in CLion first, or pass the binary path as the 2nd argument." >&2
    exit 1
fi

rm -f db.bin

{
    for ((i = 1; i <= ROW_COUNT; i++)); do
        echo "insert $i alice alice@example.com"
    done
    echo "select"
    echo ".btree"
} | "$BINARY"
