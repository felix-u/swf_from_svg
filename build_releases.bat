@echo off

if not exist bin (
    rmdir /s /q bin
    mkdir bin
)

clang -target x86_64-windows-msvc -std=c11 -fms-extensions -Wno-microsoft -Wno-assume ^
    -O2 -DLINK_CRT=1 src/main.c ^
    -obin/sfs-windows-x86_64.exe

zig cc -target x86_64-macos -std=c11 -fms-extensions -Wno-microsoft -Wno-assume ^
    -O2 -DLINK_CRT=1 src/main.c ^
    -obin/sfs-macos-x86_64.exe

zig cc -target aarch64-macos -std=c11 -fms-extensions -Wno-microsoft -Wno-assume ^
    -O2 -DLINK_CRT=1 src/main.c ^
    -obin/sfs-macos-aarch64.exe
