#!/usr/bin/env bash

current_dir=${PWD##*/}

LCOV="./deps/lcov/bin/lcov"
GENHTML="./deps/lcov/bin/genhtml"
HTML_REPORT_DIR="/tmp/mediasoup-worker-lcov-report"
COVERAGE_INFO="/tmp/mediasoup-worker-lcov-report.info"

if [ "${current_dir}" != "worker" ] ; then
	echo ">>> [ERROR] $(basename $0) must be called from mediasoup/worker/ directory" >&2
	exit 1
fi

echo ">>> [INFO] clearing counters..."
$LCOV --directory ./ --zerocounters

echo ">>> [INFO] running tests..."
make test

echo ">>> [INFO] generating coverage info file..."
$LCOV --no-external --capture --directory ./ --output-file ${COVERAGE_INFO}

echo ">>> [INFO] removing subdirectories from coverage info file..."
$LCOV -r ${COVERAGE_INFO} "$(pwd)/deps/*" -o ${COVERAGE_INFO}
$LCOV -r ${COVERAGE_INFO} "$(pwd)/fbs/*" -o ${COVERAGE_INFO}
$LCOV -r ${COVERAGE_INFO} "$(pwd)/fuzzer/*" -o ${COVERAGE_INFO}
$LCOV -r ${COVERAGE_INFO} "$(pwd)/subprojects/*" -o ${COVERAGE_INFO}
$LCOV -r ${COVERAGE_INFO} "$(pwd)/test/*" -o ${COVERAGE_INFO}

echo ">>> [INFO] clearing previous report data..."
if [[ -d ${HTML_REPORT_DIR} ]] ; then
	rm -rf ${HTML_REPORT_DIR}
else
	mkdir ${HTML_REPORT_DIR}
fi

echo ">>> [INFO] generating HTML report..."
$GENHTML -o ${HTML_REPORT_DIR} ${COVERAGE_INFO}

echo ">>> [INFO] clearing coverage info file..."
rm ${COVERAGE_INFO}

echo ">>> [INFO] clearing counters..."
$LCOV --directory ./ --zerocounters

if type "open" &> /dev/null; then
	echo ">>> [INFO] opening HTML report..."
	open ${HTML_REPORT_DIR}/index.html
fi
