#!/bin/sh
# Host-only runner: also fails if the test binary is replaced by the command
# under test (the old execl defect produced no RESULT line but exited 0).
set -eu
cd "$(dirname "$0")"
cc -O2 -o /tmp/petcard_shutdown_test shutdown_test.c
out=$(/tmp/petcard_shutdown_test)
printf '%s\n' "$out"
printf '%s\n' "$out" | grep -q '^RESULT PASS$'
count=$(printf '%s\n' "$out" | grep -c '^PASS')
[ "$count" -ge 19 ]
echo "shutdown regression: $count checks passed"
