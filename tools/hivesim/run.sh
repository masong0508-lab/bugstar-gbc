#!/bin/sh
# builds and runs the host balance simulator:  tools/hivesim/run.sh DAYS DIFF KIND SEED [v]
cd "$(dirname "$0")/../.." && gcc -std=gnu99 -O1 -w -Itests/stub -Isrc -o /tmp/hsim tools/hivesim/hsim.c && /tmp/hsim "$@"
