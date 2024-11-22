#!/usr/bin/env bash

WORKER_PWD=${PWD}
DURATION_SEC=$1

current_dir_name=${WORKER_PWD##*/}
if [ "${current_dir_name}" != "worker" ] ; then
	echo "run-fuzzer.sh [ERROR] $(basename $0) must be called from mediasoup/worker directory" >&2
	exit 1
fi

if [ "$#" -eq 0 ] ; then
	echo "run-fuzzer.sh [ERROR] duration (in seconds) must be given as argument" >&2
	exit 1
fi

invoke fuzzer-run-all &

MEDIASOUP_WORKER_FUZZER_PID=$!

i=${DURATION_SEC}

until [ ${i} -eq 0 ]
do
	echo "run-fuzzer.sh [INFO] ${i} seconds left"
	if ! kill -0 ${MEDIASOUP_WORKER_FUZZER_PID} &> /dev/null ; then
		echo "run-fuzzer.sh [ERROR] mediasoup-worker-fuzzer died" >&2
		exit 1
	else
		((i=i-1))
		sleep 1
	fi
done

echo "run-fuzzer.sh [INFO] mediasoup-worker-fuzzer is still running after given ${DURATION_SEC} seconds so no fuzzer issues so far"

kill -SIGTERM ${MEDIASOUP_WORKER_FUZZER_PID} &> /dev/null
exit 0
