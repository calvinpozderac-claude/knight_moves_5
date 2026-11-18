#!/bin/bash
cd "$(dirname "$0")"
for k in 100 104 108 112 116; do ./bbeam.sh t$k in/s9329.txt $k 20000 1; ./bbeam.sh p$k in/s9329.txt $k 20000; done
