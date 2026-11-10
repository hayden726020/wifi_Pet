#!/bin/sh
set -e
cd "$(dirname "$0")"
CC=${CC:-arm-linux-gnueabi-gcc}
$CC -O2 -static -s -o petcard petcard.c
$CC -O2 -static -s -o ui_hold ui_hold.c
cc -O2 -DPET_SIM -o petcard_sim sim.c
ls -l petcard ui_hold petcard_sim
