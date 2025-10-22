#!/bin/bash

# host_elf="src/main/resources/native/lib/x86_64-linux-gnu/libdpx.so"
# dev_elf=".log/libdpx.elf"
# dev_asm=".log/libdpx.asm"

host_elf="build/example/dpa_msgq"
dev_elf=".log/dpa_msgq.elf"
dev_asm=".log/dpa_msgq.asm"

dpacc-extract "$host_elf" -o "$dev_elf"

dpa-objdump -sSdxl --mcpu=nv-dpa-bf3 "$dev_elf" >"$dev_asm"
