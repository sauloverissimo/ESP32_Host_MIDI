#!/bin/sh
# Extract the USB-MIDI packet builder from the shipped source so the tests
# exercise the real code instead of a hand-kept copy.
#
# A mirrored copy is worse than no test: the copy can stay correct while
# src/ regresses, and the suite still reports success.
set -e

SRC="${1:-src/USBConnection.cpp}"
OUT="${2:-extras/tests/packet_builder.inc}"

awk '
  /BEGIN PACKET_BUILDER/ { inblock = 1; next }
  /END PACKET_BUILDER/   { inblock = 0; found = 1; next }
  inblock                { print }
  END {
    if (!found) {
      print "extract: PACKET_BUILDER sentinels not found in source" > "/dev/stderr"
      exit 1
    }
  }
' "$SRC" > "$OUT"

if [ ! -s "$OUT" ]; then
  echo "extract: extracted block is empty" >&2
  exit 1
fi
