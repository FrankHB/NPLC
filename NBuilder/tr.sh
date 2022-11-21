#!/usr/bin/env bash
# (C) 2022 FrankHB.
# Test script for running the interpreter in release mode.

: "${CONF:="release"}"
: "${LOGOPT:="--log-file=leak-release.log"}"
: "${TOOLOPT:="--tool=callgrind --num-callers=96 --dump-instr=yes --collect-jumps=yes"}"

# XXX: Value of several variables may contain whitespaces.
# shellcheck disable=2086
valgrind $LOGOPT $TOOLOPT --fullpath-after= ".$CONF/NBuilder.exe" "$@"

