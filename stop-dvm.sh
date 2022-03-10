#!/bin/bash
PID=`pgrep dvmhost`
pgrep dvmhost >/dev/null
if [ $? -eq 0 ]; then
    kill -9 $PID
fi

