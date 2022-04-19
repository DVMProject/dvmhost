#!/bin/bash
R_PATH=/opt/dvm
pushd ${R_PATH} >/dev/null

if [ "`pwd`" != "${R_PATH}" ]; then exit 99; fi
if [ -z $1 ]; then exit 2; fi

CONFIG=$1
if [ ! -f ${R_PATH}/${CONFIG} ]; then exit 3; fi
PID_FILE=$2
if [ -z $2 ]; then
    STRIPPED_CONFIG=`basename ${CONFIG}`
    STRIPPED_CONFIG=${STRIPPED_CONFIG%.yml}
    PID_FILE=/tmp/dvmhost.${STRIPPED_CONFIG}.pid
fi

COMMAND="${R_PATH}/bin/dvmhost -c ${R_PATH}/${CONFIG}"
nice -n -20 ${COMMAND}

if [ -e /tmp/${CONFIG}.pid ]; then rm -f /tmp/${CONFIG}.pid; fi

PID=`pgrep -f "${R_PATH}/${CONFIG}"`

echo "${PID}" > $PID_FILE
pgrep -f "[d]vm-watchdog.*${PID_FILE}" >/dev/null
if [ $? -ne 0 ]; then
    ${R_PATH}/dvm-watchdog.sh $PID_FILE $CONFIG &
fi

popd >/dev/null
