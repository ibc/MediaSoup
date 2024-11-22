#!/usr/bin/env bash

set -e

WORKER_PWD=${PWD}
DEP=$1

current_dir_name=${WORKER_PWD##*/}
if [ "${current_dir_name}" != "worker" ] ; then
	echo ">>> [ERROR] $(basename $0) must be called from mediasoup/worker directory" >&2
	exit 1
fi

function get_dep()
{
	GIT_REPO="$1"
	GIT_TAG="$2"
	DEST="$3"

	echo ">>> [INFO] getting dep '${DEP}'..."

	if [ -d "${DEST}" ] ; then
		echo ">>> [INFO] deleting ${DEST}..."
		git rm -rf --ignore-unmatch ${DEST} > /dev/null
		rm -rf ${DEST}
	fi

	echo ">>> [INFO] cloning ${GIT_REPO}..."
	git clone ${GIT_REPO} ${DEST}

	cd ${DEST}

	echo ">>> [INFO] setting '${GIT_TAG}' git tag..."
	git checkout --quiet ${GIT_TAG}
	set -e

	echo ">>> [INFO] adding dep source code to the repository..."
	rm -rf .git
	git add .

	echo ">>> [INFO] got dep '${DEP}'"

	cd ${WORKER_PWD}
}

function get_lcov()
{
	GIT_REPO="https://github.com/linux-test-project/lcov.git"
	GIT_TAG="master"
	DEST="deps/lcov"

	get_dep "${GIT_REPO}" "${GIT_TAG}" "${DEST}"
}

function get_clang_fuzzer()
{
	NAME="clang+llvm-7.0.0-x86_64-linux-gnu-ubuntu-16.04"
	TAR_FILE="${NAME}.tar.xz"
	TAR_URL="http://releases.llvm.org/7.0.0/${TAR_FILE}"
	DEST="deps/clang-fuzzer"

	set -x

	rm -rf ${DEST}
	mkdir ${DEST}
	cd ${DEST}
	mkdir bin lib
	wget ${TAR_URL}
	tar xfJ ${TAR_FILE}
	rm -f ${TAR_FILE}
	mv ${NAME}/bin/* bin/
	mv ${NAME}/lib/clang lib/
	rm -rf ${NAME}
}

function get_fuzzer_corpora()
{
	GIT_REPO="https://github.com/RTC-Cartel/webrtc-fuzzer-corpora.git"
	GIT_TAG="master"
	DEST="deps/webrtc-fuzzer-corpora"

	get_dep "${GIT_REPO}" "${GIT_TAG}" "${DEST}"
}

case "${DEP}" in
	'-h')
		echo "Usage:"
		echo "  ./scripts/$(basename $0) [lcov|clang-fuzzer|fuzzer-corpora]"
		echo
		;;
	lcov)
		get_lcov
		;;
	clang-fuzzer)
		get_clang_fuzzer
		;;
	fuzzer-corpora)
		get_fuzzer_corpora
		;;
	*)
		echo ">>> [ERROR] unknown dep '${DEP}'" >&2
		exit 1
esac

if [ $? -eq 0 ] ; then
	echo ">>> [INFO] done"
else
	echo ">>> [ERROR] failed" >&2
	exit 1
fi
