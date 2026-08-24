#!/bin/bash
# Enable deauth injection on ESP32-C5 by patching the Wi-Fi blob's
# ieee80211_raw_frame_sanity_check() to always return 0.
# Overwrites the function's first 4 bytes with RISC-V `c.li a0,0; c.jr ra`.
# Original backed up to *.orig (restore: cp <lib>.orig <lib>).
# Run once, then: idf.py fullclean && idf.py build && idf.py flash
set -euo pipefail

LIB=$(find "$HOME/esp/esp-idf/components/esp_wifi/lib/esp32c5" -name libnet80211.a | head -1)
AR=$(find "$HOME/.espressif/tools" -name "riscv32-esp-elf-ar" | head -1)
OBJD=$(find "$HOME/.espressif/tools" -name "riscv32-esp-elf-objdump" | head -1)
[ -n "$LIB" ] && [ -n "$AR" ] || { echo "toolchain/lib not found"; exit 1; }
echo "lib: $LIB"

[ -f "$LIB.orig" ] || cp "$LIB" "$LIB.orig"
echo "backup: $LIB.orig"

WORK=$(mktemp -d); cd "$WORK"
"$AR" x "$LIB" ieee80211_output.o

OFF=13432   # 0x3478 = file offset of the function in ieee80211_output.o
python3 - "$OFF" <<'PY'
import sys
off=int(sys.argv[1]); p='ieee80211_output.o'
b=bytearray(open(p,'rb').read())
cur=bytes(b[off:off+4])
orig=bytes([0x79,0x71,0x06,0xd6])       # c.addi sp,sp,-48 ; c.sw ra,44(sp)
patch=bytes([0x01,0x45,0x82,0x80])      # c.li a0,0 ; c.jr ra  (return 0)
if cur==patch:
    print("already patched, nothing to do"); sys.exit(0)
if cur!=orig:
    print("UNEXPECTED bytes at 0x%x: %s (expected %s). Aborting."%(off,cur.hex(),orig.hex()))
    sys.exit(1)
b[off:off+4]=patch
open(p,'wb').write(b)
print("patched @0x%x: %s -> %s"%(off, cur.hex(), patch.hex()))
PY

echo "=== verify: function must now start with 'li a0,0' then 'ret' ==="
"$OBJD" -d ieee80211_output.o | grep -A3 "<ieee80211_raw_frame_sanity_check>:"

"$AR" r "$LIB" ieee80211_output.o
echo "DONE. Wi-Fi blob patched. Rebuild & flash next."
