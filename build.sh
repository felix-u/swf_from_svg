#!/usr/bin/env bash

set -e
set -u

if [[ ! -f build.bin ]]; then
	clang -O0 -Wno-assume -Wno-microsoft -fms-extensions -DLINK_CRT=1 -DBUILD_DEBUG=1 -fdiagnostics-absolute-paths -std=c11 -ferror-limit=1 build.c -obuild.bin || exit 1
fi

./build.bin $@
exit $?
