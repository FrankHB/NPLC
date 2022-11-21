#!/usr/bin/env bash
# (C) 2022 FrankHB.
# Test script for running the interpreter in debug mode.

: "${CONF:="debug"}"
: "${LOGOPT:="--log-file=leak.log"}"
: "${TOOLOPT:="--leak-check=full --num-callers=96 --main-stacksize=65536"}"

# XXX: Value of several variables may contain whitespaces.
# shellcheck disable=2086
valgrind $LOGOPT $TOOLOPT --fullpath-after= ".$CONF/NBuilder.exe" "$@"

