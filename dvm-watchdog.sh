#!/bin/bash
R_PATH=/opt/dvm
pushd ${R_PATH}

if [ -z $1 ]; then exit 99; fi
PID_FILE=$1
CONFIG_FILE=`cat $PID_FILE`

while [ true ]; do
    pgrep -f "[d]vmhost.*${CONFIG_FILE}" >/dev/null
    if [ $? -ne 0 ]; then
        ${R_PATH}/start-dvm.sh ${CONFIG_FILE} >/dev/null
    fi
    sleep 5s
done

popd

