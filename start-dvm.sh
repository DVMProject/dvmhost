#!/bin/bash
R_PATH=/opt/dvm
pushd ${R_PATH}

if [ "`pwd`" != "${R_PATH}" ]; then exit 99; fi
if [ -z $1 ]; then exit 2; fi

CONFIG=$1
if [ ! -f ${R_PATH}/${CONFIG} ]; then exit 3; fi

COMMAND="${R_PATH}/dvmhost -c ${R_PATH}/${CONFIG}"
nice -n -20 ${COMMAND}

PID=`pgrep -f "${R_PATH}/${CONFIG}"`
PID_FILE=/tmp/${CONFIG}.pid
echo "${CONFIG}" > $PID_FILE

pgrep -f "[d]vm-watchdog.*${PID_FILE}" >/dev/null
if [ $? -ne 0 ]; then
    ${R_PATH}/dvm-watchdog.sh $PID_FILE &
fi

popd

