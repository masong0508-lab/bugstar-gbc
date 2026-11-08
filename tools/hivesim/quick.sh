#!/bin/sh
# quick balance check: quick.sh DAYS -> 3 world kinds x 2 seeds x 3 difficulties, one line each
D=${1:-60}; H="$(dirname "$0")"
gcc -std=gnu99 -O1 -w -I"$H/../../tests/stub" -I"$H/../../src" -o /tmp/hsim "$H/hsim.c" || exit 1
for d in 0 1 2; do for k in 0 5 9; do for s in 1 2; do /tmp/hsim $D $d $k $s | head -1; done; done; done
