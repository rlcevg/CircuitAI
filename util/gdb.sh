#!/bin/sh
LD_PRELOAD=/usr/lib/libasan.so ASAN_OPTIONS=abort_on_error=1 gdb -x=gdb.cmd --args ./spring-headless head.script
