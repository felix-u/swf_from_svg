#!/usr/bin/env bash

set -e
set -u

zig cc -target x86_64-windows-gnu -std=c11 -fms-extensions -Wno-microsoft -Wno-assume \
    -O2 -DLINK_CRT=1 src/main.c \
    -obin/sfs-windows-x86_64.exe

zig cc -target x86_64-macos -std=c11 -fms-extensions -Wno-microsoft -Wno-assume \
    -O2 -DLINK_CRT=1 src/main.c \
    -obin/sfs-macos-x86_64.bin

zig cc -target aarch64-macos -std=c11 -fms-extensions -Wno-microsoft -Wno-assume \
    -O2 -DLINK_CRT=1 src/main.c \
    -obin/sfs-macos-aarch64.bin
