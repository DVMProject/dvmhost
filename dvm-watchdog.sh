#!/bin/bash
R_PATH=/opt/dvm
pushd ${R_PATH} >/dev/null

if [ -z $1 ]; then exit 1; fi
if [ -z $2 ]; then exit 1; fi

PID_FILE=$1
if [ ! -e $PID_FILE ]; then exit 98; fi
CONFIG=$2
if [ ! -f ${R_PATH}/${CONFIG} ]; then exit 3; fi

PID=`cat $PID_FILE`
while [ true ]; do
    PS_OUT=`ps --no-headers ${PID}`
    if [ ${#PS_OUT} -eq 0 ]; then
        logger "dvmhost PID ${PID} not running -- restarting ${PID_FILE} ${CONFIG}"
        ${R_PATH}/start-dvm.sh ${CONFIG} ${PID_FILE} >/dev/null
        ERRNO=$?
        if [ $ERRNO -ne 0 ]; then
            logger "dvmhost start-dvm.sh script failed to start; errno ${ERRNO}"
        fi
    fi
    sleep 5s
done

popd >/dev/null
