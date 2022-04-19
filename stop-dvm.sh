#!/bin/bash
PID=`pgrep dvmhost`
pgrep dvmhost >/dev/null
if [ $? -eq 0 ]; then
    kill -15 $PID
    sleep 2s
    pgrep dvmhost >/dev/null
    if [ $? -eq 0 ]; then
        kill -9 $PID
    fi
fi

PID=`pgrep dvm-watchdog.sh`
pgrep dvm-watchdog.sh >/dev/null
if [ $? -eq 0 ]; then
    kill -9 $PID
fi

