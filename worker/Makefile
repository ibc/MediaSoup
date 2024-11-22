#
# make tasks for mediasoup-worker.
#
# NOTE: This Makefile is a proxy to pip invoke commands (see tasks.py).
#

PYTHON ?= $(shell command -v python3 2> /dev/null || echo python)
PIP_INVOKE_DIR = $(shell pwd)/pip_invoke

# Instruct Python where to look for invoke module.
ifeq ($(OS),Windows_NT)
	export PYTHONPATH := $(PIP_INVOKE_DIR);${PYTHONPATH}
else
	export PYTHONPATH := $(PIP_INVOKE_DIR):${PYTHONPATH}
endif

.PHONY:	\
	default \
	invoke \
	meson-ninja \
	setup \
	clean \
	clean-build \
	clean-pip \
	clean-subprojects \
	clean-all \
	update-wrap-file \
	mediasoup-worker \
	libmediasoup-worker \
	flatc \
	xcode \
	lint \
	format \
	test \
	test-asan-address \
	test-asan-undefined \
	test-asan-thread \
	tidy \
	fuzzer \
	fuzzer-run-all \
	docker \
	docker-run \
	docker-alpine \
	docker-alpine-run

default: mediasoup-worker

invoke:
ifeq ($(wildcard $(PIP_INVOKE_DIR)),)
	# Install pip invoke into custom location, so we don't depend on system-wide
	# installation.
	"$(PYTHON)" -m pip install --upgrade --no-user --target "$(PIP_INVOKE_DIR)" invoke
endif

meson-ninja: invoke
	"$(PYTHON)" -m invoke meson-ninja

setup: invoke
	"$(PYTHON)" -m invoke setup

clean: invoke
	"$(PYTHON)" -m invoke clean

clean-build: invoke
	"$(PYTHON)" -m invoke clean-build

clean-pip: invoke
	"$(PYTHON)" -m invoke clean-pip

clean-subprojects: invoke
	"$(PYTHON)" -m invoke clean-subprojects

clean-all: invoke
	"$(PYTHON)" -m invoke clean-all

# It requires the SUBPROJECT environment variable.
update-wrap-file: invoke
	"$(PYTHON)" -m invoke subprojects $(SUBPROJECT)

mediasoup-worker: invoke
	"$(PYTHON)" -m invoke mediasoup-worker

libmediasoup-worker: invoke
	"$(PYTHON)" -m invoke libmediasoup-worker

flatc: invoke
	"$(PYTHON)" -m invoke flatc

xcode: invoke
	"$(PYTHON)" -m invoke xcode

lint: invoke
	"$(PYTHON)" -m invoke lint

format: invoke
	"$(PYTHON)" -m invoke format

test: invoke
	"$(PYTHON)" -m invoke test

test-asan-address: invoke
	"$(PYTHON)" -m invoke test-asan-address

test-asan-undefined: invoke
	"$(PYTHON)" -m invoke test-asan-undefined

test-asan-thread: invoke
	"$(PYTHON)" -m invoke test-asan-thread

tidy: invoke
	"$(PYTHON)" -m invoke tidy

fuzzer: invoke
	"$(PYTHON)" -m invoke fuzzer

fuzzer-run-all: invoke
	"$(PYTHON)" -m invoke fuzzer-run-all

docker: invoke
	"$(PYTHON)" -m invoke docker

docker-run: invoke
	"$(PYTHON)" -m invoke docker-run

docker-alpine: invoke
	"$(PYTHON)" -m invoke docker-alpine

docker-alpine-run: invoke
	"$(PYTHON)" -m invoke docker-alpine-run
