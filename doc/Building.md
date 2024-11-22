# Building

This document is intended for mediasoup developers.

## NPM scripts

The `package.json` file in the main folder includes the following scripts:

### `npm run typescript:build`

Compiles mediasoup TypeScript code (`node/src` folder) JavaScript and places it into the `node/lib` directory.

### `npm run typescript:watch`

Compiles mediasoup TypeScript code (`node/src` folder) JavaScript, places it into the `node/lib` directory an watches for changes in the TypeScript files.

### `npm run worker:build`

Builds the `mediasoup-worker` binary. It invokes `invoke`below.

### `npm run worker:prebuild`

Creates a prebuilt of `mediasoup-worker` binary in the `worker/prebuild` folder.

### `npm run lint`

Runs both `npm run lint:node` and `npm run lint:worker`.

### `npm run lint:node`

Validates mediasoup JavaScript files using [ESLint](https://eslint.org).

### `npm run lint:worker`

Validates mediasoup worker C++ files using [clang-format](https://clang.llvm.org/docs/ClangFormat.html). It invokes `invoke lint` below.

### `npm run format`

Runs both `npm run format:node` and `npm run format:worker`.

### `npm run format:node`

Format TypeScript and JavaScript code using [Prettier](https://prettier.io).

### `npm run format:worker`

Rewrites mediasoup worker C++ files using [clang-format](https://clang.llvm.org/docs/ClangFormat.html). It invokes `invoke format` below.

### `npm run flatc`

Runs both `npm run flatc:node` and `npm run flatc:worker`.

### `npm run flatc:node`

Compiles [FlatBuffers](https://github.com/google/flatbuffers) `.fbs` files in `worker/fbs` to TypeScript code.

### `npm run flatc:worker`

Compiles [FlatBuffers](https://github.com/google/flatbuffers) `.fbs` files in `worker/fbs` to C++ code.

### `npm run test`

Runs both `npm run test:node` and `npm run test:worker`.

### `npm run test:node`

Runs [Jest](https://jestjs.io) test units located at `node/test` folder.

Jest command arguments can be given using `--` as follows:

```bash
npm run test:node -- --testPathPattern "test-Worker.ts" --testNamePattern "createWorker"
```

### `npm run test:worker`

Runs [Catch2](https://github.com/catchorg/Catch2) test units located at `worker/test` folder. It invokes `invoke test` below.

### `npm run coverage:node`

Same as `npm run test:node` but it also opens a browser window with JavaScript coverage results.

### `npm run release:check`

Runs linters and tests in Node and C++ code.

## Rust

The only special feature in Rust case is special environment variable "KEEP_BUILD_ARTIFACTS", that when set to "1" will allow incremental recompilation of changed C++ sources during hacking on mediasoup.

It is not necessary for normal usage of mediasoup as a dependency.

## Python Invoke and `tasks.py` file

mediasoup uses Python [Invoke](https://www.pyinvoke.org) library for managing and organizing tasks in the `worker` folder (mediasoup worker C++ subproject). `Invoke` is basically a replacemente of `make` + `Makefile` written in Python. mediasoup automatically installs `Invoke` in a local custom path during the installation process (in both Node and Rust) so the user doesn't need to worry about it.

Tasks are defined in `worker/tasks.py`. For development purposes, developers or contributors can install `Invoke` using `pip3 install invoke` and run tasks below within the `worker` folder.

See all the tasks by running `invoke --list` within the `worker` folder.

_NOTE:_ For some of these tasks to work, npm dependencies of `worker/scripts/package.json` must be installed:

```bash
npm ci --prefix worker/scripts
```

### `invoke` (default task)

Alias of `invoke mediasoup-worker` task below.

### `invoke meson-ninja`

Installs `meson` and `ninja` into a local custom path.

### `invoke clean`

Cleans built objects and binaries.

### `invoke clean-build`

Cleans built objects and other artifacts, but keeps `mediasoup-worker` binary in place.

### `invoke clean-pip`

Cleans `meson` and `ninja` installed in local prefix with pip.

### `invoke clean-subprojects`

Cleans subprojects downloaded with Meson.

### `invoke clean-all`

Cleans built objects and binaries, `meson` and `ninja` installed in local prefix with pip and all subprojects downloaded with Meson.

### `invoke update-wrap-file [subproject]`

Updates the wrap file of a subproject (those in `worker/subprojects` folder) with Meson. After updating it, `invoke setup` must be called by passing `MESON_ARGS="--reconfigure"` environment variable. Usage example:

```bash
cd worker
invoke update-wrap-file openssl
MESON_ARGS="--reconfigure" invoke setup
```

### `invoke mediasoup-worker`

Builds the `mediasoup-worker` binary at `worker/out/Release`.

If the "MEDIASOUP_MAX_CORES" environment variable is set, the build process will use that number of CPU cores. Otherwise it will auto-detect the number of cores in the machine.

"MEDIASOUP_BUILDTYPE" environment variable controls build types, "Release" and "Debug" are presets optimized for those use cases. Other build types are possible too, but they are not presets and will require "MESON_ARGS" use to customize build configuration.

Check the meaning of useful macros in the `worker/include/Logger.hpp` header file if you want to enable tracing or other debug information.

Binary is built at `worker/out/MEDIASOUP_BUILDTYPE/build`.

In order to instruct the mediasoup Node.js module to use the "Debug" mediasoup-worker` binary, an environment variable must be set before running the Node.js application:

```bash
MEDIASOUP_BUILDTYPE=Debug node myapp.js
```

If the "MEDIASOUP_WORKER_BIN" environment variable is set (it must be an absolute file path), mediasoup will use the it as `mediasoup-worker` binary and **won't** compile the binary:

```bash
MEDIASOUP_WORKER_BIN="/home/xxx/src/foo/mediasoup-worker" node myapp.js
```

### `invoke libmediasoup-worker`

Builds the `libmediasoup-worker` static library at `worker/out/Release`.

"MEDIASOUP_MAX_CORES"` and "MEDIASOUP_BUILDTYPE" environment variables from above still apply for static library build.

### `invoke xcode`

Builds a Xcode project for the mediasoup worker subproject.

### `invoke lint`

Validates mediasoup worker C++ files using [clang-format](https://clang.llvm.org/docs/ClangFormat.html) and rules in `worker/.clang-format`.

### `invoke format`

Rewrites mediasoup worker C++ files using [clang-format](https://clang.llvm.org/docs/ClangFormat.html).

### `invoke test`

Builds and runs the `mediasoup-worker-test` binary at `worker/out/Release` (or at `worker/out/Debug` if the "MEDIASOUP_BUILDTYPE" environment variable is set to "Debug"), which uses [Catch2](https://github.com/catchorg/Catch2) to run test units located at `worker/test` folder.

### `invoke test-asan-address`

Run test with Address Sanitizer with `-fsanitize=address`.

### `invoke test-asan-undefined`

Run test with Address Sanitizer with `-fsanitize=undefined`.

### `invoke test-asan-thread`

Run test with Address Sanitizer with `-fsanitize=thread`.

### `invoke tidy`

Runs [clang-tidy](http://clang.llvm.org/extra/clang-tidy) and performs C++ code checks following `worker/.clang-tidy` rules.

**Requirements:**

- `invoke clean` and `invoke mediasoup-worker` must have been called first.
- [clang-tools-extra](https://clang.llvm.org/extra) is required.
  - In OSX install it with `brew install llvm`.
  - In linux the package name is `clang-tools-extra`.

**Environment variables:**

- "MEDIASOUP_TIDY_CHECKS": Comma separated list of checks. Overrides the checks defined in `worker/.clang-tidy` file.
- "MEDIASOUP_TIDY_FILES": Space separated source files to process, including their path. All `.cpp` files will be processes by default.
- "MEDIASOUP_CLANG_TIDY_DIR": Path to directory containing clang tools (`run-clang-tidy`, `clang-tidy`, `clang-apply-replacements`).

**Usage example in macOS:**

```bash
MEDIASOUP_CLANG_TIDY_DIR=/usr/local/opt/llvm/bin invoke tidy
```

### `invoke fuzzer`

Builds the `mediasoup-worker-fuzzer` binary (which uses [libFuzzer](http://llvm.org/docs/LibFuzzer.html)) at `worker/out/Release` (or at `worker/out/Debug/` if the "MEDIASOUP_BUILDTYPE" environment variable is set to "Debug").

**Requirements:**

- Linux with fuzzer capable clang++.
- "CC" environment variable must point to `clang`.
- "CXX" environment variable must point to `clang++`.

Read the [Fuzzer](Fuzzer.md) documentation for detailed information.

### `invoke fuzzer-run-all`

Runs all fuzzer cases.

### `invoke docker`

Builds a Linux Ubuntu Docker image with fuzzer capable clang++ and all dependencies to run mediasoup.

**NOTE:** Before running this command, a specific version of Linux clang must be downloaded. To get it, run:

```bash
cd worker
scripts/get-dep.sh clang-fuzzer
```

### `invoke docker-run`

Runs a container of the Ubuntu Docker image created with `invoke docker`. It automatically executes a `bash` session in the `/mediasoup` directory, which is a Docker volume that points to the mediasoup root folder.

**NOTE:** To install and run mediasoup in the container, previous installation (if any) must be properly cleaned by entering the `worker` directory and running `invoke clean-all`.

### `invoke docker-alpine`

Builds a Linux Alpine Docker image with all dependencies to run mediasoup.

### `invoke docker-alpine-run`

Runs a container of the Alpine Docker image created with `invoke docker-alpine`. It automatically executes an `ash` session in the `/mediasoup` directory, which is a Docker volume that points to the mediasoup root folder.

**NOTE:** To install and run mediasoup in the container, previous installation (if any) must be properly cleaned by entering the `worker` directory and running `invoke clean-all`.

## Makefile

The `worker` folder contains a `Makefile` file for the mediasoup worker C++ subproject. It acts as a proxy to the `Invoke` tasks defined in `tasks.py`. The `Makefile` file exists to help developers or contributors that prefer keep using `make` commands.

All tasks defined in `tasks.py` (see above) are available in `Makefile`. There is only one exception:

- The `update-wrap-file` needs a "SUBPROJECT" environment variable indicating the subproject to update. Usage example:
  ```bash
  cd worker
  make update-wrap-file SUBPROJECT=openssl
  ```
