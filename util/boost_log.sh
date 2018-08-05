#!/bin/sh
CMD="make CircuitAI" && strace -f -e trace=openat $CMD 2>&1 > /dev/null | grep /boost/ >> boost_files.txt
