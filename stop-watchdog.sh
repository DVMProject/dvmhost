#!/bin/bash
PID=`pgrep dvm-watchdog.sh`
pgrep dvm-watchdog.sh >/dev/null
if [ $? -eq 0 ]; then
    kill -9 $PID
fi

