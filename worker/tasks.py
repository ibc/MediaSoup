# Ignore these pylint warnings, conventions and refactoring messages:
# - W0301: Unnecessary semicolon (unnecessary-semicolon)
# - W0622: Redefining built-in 'format' (redefined-builtin)
# - W0702: No exception type(s) specified (bare-except)
# - C0114: Missing module docstring (missing-module-docstring)
# - C0301: Line too long (line-too-long)
#
# pylint: disable=W0301,W0622,W0702,C0114,C0301

#
# This is a tasks.py file for the Python invoke package:
# https://docs.pyinvoke.org/.
#
# It's a replacement of our Makefile with same tasks.
#
# Usage:
#   pip install invoke
#   invoke --list
#

import sys;
import os;
import inspect;
import shutil;
# We import this from a custom location and pylint doesn't know.
from invoke import task, call; # pylint: disable=import-error

MEDIASOUP_BUILDTYPE = os.getenv('MEDIASOUP_BUILDTYPE') or 'Release';
WORKER_DIR = os.path.dirname(os.path.abspath(
    inspect.getframeinfo(inspect.currentframe()).filename
));
# NOTE: MEDIASOUP_OUT_DIR is overrided by build.rs.
MEDIASOUP_OUT_DIR = os.getenv('MEDIASOUP_OUT_DIR') or f'{WORKER_DIR}/out';
MEDIASOUP_INSTALL_DIR = os.getenv('MEDIASOUP_INSTALL_DIR') or f'{MEDIASOUP_OUT_DIR}/{MEDIASOUP_BUILDTYPE}';
BUILD_DIR = os.getenv('BUILD_DIR') or f'{MEDIASOUP_INSTALL_DIR}/build';
# Custom pip folder for invoke package.
# NOTE: We invoke `pip install` always with `--no-user` to make it not complain
# about "can not combine --user and --target".
PIP_INVOKE_DIR = f'{MEDIASOUP_OUT_DIR}/pip_invoke';
# Custom pip folder for meson and ninja packages.
PIP_MESON_NINJA_DIR = f'{MEDIASOUP_OUT_DIR}/pip_meson_ninja';
# Custom pip folder for pylint package.
# NOTE: We do this because using --target and --upgrade in `pip install` is not
# supported and a latter invokation entirely replaces the bin folder.
PIP_PYLINT_DIR = f'{MEDIASOUP_OUT_DIR}/pip_pylint';
# If available (only on some *nix systems), os.sched_getaffinity(0) gets set of
# CPUs the calling thread is restricted to. Instead, os.cpu_count() returns the
# total number of CPUs in a system (it doesn't take into account how many of them
# the calling thread can use).
NUM_CORES = len(os.sched_getaffinity(0)) if hasattr(os, 'sched_getaffinity') else os.cpu_count();
PYTHON = os.getenv('PYTHON') or sys.executable;
MESON = os.getenv('MESON') or f'{PIP_MESON_NINJA_DIR}/bin/meson';
MESON_VERSION = os.getenv('MESON_VERSION') or '1.5.0';
# MESON_ARGS can be used to provide extra configuration parameters to meson,
# such as adding defines or changing optimization options. For instance, use
# `MESON_ARGS="-Dms_log_trace=true -Dms_log_file_line=true" npm i` to compile
# worker with tracing and enabled.
# NOTE: On Windows make sure to add `--vsenv` or have MSVS environment already
# active if you override this parameter.
MESON_ARGS = os.getenv('MESON_ARGS') if os.getenv('MESON_ARGS') else '--vsenv' if os.name == 'nt' else '';
# Let's use a specific version of ninja to avoid buggy version 1.11.1:
# https://mediasoup.discourse.group/t/partly-solved-could-not-detect-ninja-v1-8-2-or-newer/
# https://github.com/ninja-build/ninja/issues/2211
NINJA_VERSION = os.getenv('NINJA_VERSION') or '1.10.2.4';
PYLINT_VERSION = os.getenv('PYLINT_VERSION') or '3.0.2';
NPM = os.getenv('NPM') or 'npm';
DOCKER = os.getenv('DOCKER') or 'docker';
# pty=True in ctx.run() is not available on Windows so if stdout is not a TTY
# let's assume PTY is not supported. Related issue in invoke project:
# https://github.com/pyinvoke/invoke/issues/561
PTY_SUPPORTED = sys.stdout.isatty();
# Use sh (widely supported, more than bash) if not in Windows.
SHELL = '/bin/sh' if not os.name == 'nt' else None;

# Disable `*.pyc` files creation.
os.environ['PYTHONDONTWRITEBYTECODE'] = 'true';

# Instruct meson where to look for ninja binary.
if os.name == 'nt':
    # Windows is, of course, special.
    os.environ['NINJA'] = f'{PIP_MESON_NINJA_DIR}/bin/ninja.exe';
else:
    os.environ['NINJA'] = f'{PIP_MESON_NINJA_DIR}/bin/ninja';

# Instruct Python where to look for modules it needs, such that meson actually
# runs from installed location.
# NOTE: On Windows we must use ; instead of : to separate paths.
PYTHONPATH = os.getenv('PYTHONPATH') or '';
if os.name == 'nt':
    os.environ['PYTHONPATH'] = f'{PIP_INVOKE_DIR};{PIP_MESON_NINJA_DIR};{PIP_PYLINT_DIR};{PYTHONPATH}';
else:
    os.environ['PYTHONPATH'] = f'{PIP_INVOKE_DIR}:{PIP_MESON_NINJA_DIR}:{PIP_PYLINT_DIR}:{PYTHONPATH}';


@task
def meson_ninja(ctx):
    """
    Install meson and ninja (also update Python pip and setuptools packages)
    """
    if os.path.isfile(MESON):
        return;

    # Updated pip and setuptools are needed for meson.
    # `--system` is not present everywhere and is only needed as workaround for
    # Debian-specific issue (copied from https://github.com/gluster/gstatus/pull/33),
    # fallback to command without `--system` if the first one fails.
    try:
        ctx.run(
            f'"{PYTHON}" -m pip install --system --upgrade --no-user --target "{PIP_MESON_NINJA_DIR}" pip setuptools',
            echo=True,
            hide=True,
            shell=SHELL
        );
    except:
        ctx.run(
            f'"{PYTHON}" -m pip install --upgrade --no-user --target "{PIP_MESON_NINJA_DIR}" pip setuptools',
            echo=True,
            pty=PTY_SUPPORTED,
            shell=SHELL
        );

    # Workaround for NixOS and Guix that don't work with pre-built binaries, see:
    # https://github.com/NixOS/nixpkgs/issues/142383.
    pip_build_binaries = '--no-binary :all:' if os.path.isfile('/etc/NIXOS') or os.path.isdir('/etc/guix') else '';

    # Install meson and ninja using pip into our custom location, so we don't
    # depend on system-wide installation.
    ctx.run(
        f'"{PYTHON}" -m pip install --upgrade --no-user --target "{PIP_MESON_NINJA_DIR}" {pip_build_binaries} meson=={MESON_VERSION} ninja=={NINJA_VERSION}',
        echo=True,
        pty=PTY_SUPPORTED,
        shell=SHELL
    );


@task(pre=[meson_ninja])
def setup(ctx, meson_args=MESON_ARGS):
    """
    Run meson setup
    """
    if MEDIASOUP_BUILDTYPE == 'Release':
        with ctx.cd(f'"{WORKER_DIR}"'):
            ctx.run(
                f'"{MESON}" setup --prefix "{MEDIASOUP_INSTALL_DIR}" --bindir "" --libdir "" --buildtype release -Db_ndebug=true {meson_args} "{BUILD_DIR}"',
                echo=True,
                pty=PTY_SUPPORTED,
                shell=SHELL
            );
    elif MEDIASOUP_BUILDTYPE == 'Debug':
        with ctx.cd(f'"{WORKER_DIR}"'):
            ctx.run(
                f'"{MESON}" setup --prefix "{MEDIASOUP_INSTALL_DIR}" --bindir "" --libdir "" --buildtype debug {meson_args} "{BUILD_DIR}"',
                echo=True,
                pty=PTY_SUPPORTED,
                shell=SHELL
            );
    else:
        with ctx.cd(f'"{WORKER_DIR}"'):
            ctx.run(
                f'"{MESON}" setup --prefix "{MEDIASOUP_INSTALL_DIR}" --bindir "" --libdir "" --buildtype {MEDIASOUP_BUILDTYPE} -Db_ndebug=if-release {meson_args} "{BUILD_DIR}"',
                echo=True,
                pty=PTY_SUPPORTED,
                shell=SHELL
            );


@task
def clean(ctx): # pylint: disable=unused-argument
    """
    Clean the installation directory
    """
    shutil.rmtree(MEDIASOUP_INSTALL_DIR, ignore_errors=True);


@task
def clean_build(ctx): # pylint: disable=unused-argument
    """
    Clean the build directory
    """
    shutil.rmtree(BUILD_DIR, ignore_errors=True);


@task
def clean_pip(ctx): # pylint: disable=unused-argument
    """
    Clean the local pip setup
    """
    shutil.rmtree(PIP_MESON_NINJA_DIR, ignore_errors=True);
    shutil.rmtree(PIP_PYLINT_DIR, ignore_errors=True);


@task(pre=[meson_ninja])
def clean_subprojects(ctx):
    """
    Clean meson subprojects
    """
    with ctx.cd(f'"{WORKER_DIR}"'):
        ctx.run(
            f'"{MESON}" subprojects purge --include-cache --confirm',
            echo=True,
            pty=PTY_SUPPORTED,
            shell=SHELL
        );


@task
def clean_all(ctx):
    """
    Clean meson subprojects and all installed/built artificats
    """
    with ctx.cd(f'"{WORKER_DIR}"'):
        try:
            ctx.run(
                f'"{MESON}" subprojects purge --include-cache --confirm',
                echo=True,
                pty=PTY_SUPPORTED,
                shell=SHELL
            );
        except:
            pass;

        shutil.rmtree(MEDIASOUP_OUT_DIR, ignore_errors=True);
        shutil.rmtree('include/FBS', ignore_errors=True);


@task(pre=[meson_ninja])
def update_wrap_file(ctx, subproject):
    """
    Update the wrap file of a subproject
    """
    with ctx.cd(f'"{WORKER_DIR}"'):
        ctx.run(
            f'"{MESON}" subprojects update --reset {subproject}',
            echo=True,
            pty=PTY_SUPPORTED,
            shell=SHELL
        );


@task(pre=[setup])
def flatc(ctx):
    """
    Compile FlatBuffers FBS files
    """
    with ctx.cd(f'"{WORKER_DIR}"'):
        ctx.run(
            f'"{MESON}" compile -C "{BUILD_DIR}" flatbuffers-generator',
            echo=True,
            pty=PTY_SUPPORTED,
            shell=SHELL
        );


@task(pre=[setup, flatc], default=True)
def mediasoup_worker(ctx):
    """
    Compile mediasoup-worker binary
    """
    if os.getenv('MEDIASOUP_WORKER_BIN'):
        print('skipping mediasoup-worker compilation due to the existence of the MEDIASOUP_WORKER_BIN environment variable');
        return;

    with ctx.cd(f'"{WORKER_DIR}"'):
        ctx.run(
            f'"{MESON}" compile -C "{BUILD_DIR}" -j {NUM_CORES} mediasoup-worker',
            echo=True,
            pty=PTY_SUPPORTED,
            shell=SHELL
        );
    with ctx.cd(f'"{WORKER_DIR}"'):
        ctx.run(
            f'"{MESON}" install -C "{BUILD_DIR}" --no-rebuild --tags mediasoup-worker',
            echo=True,
            pty=PTY_SUPPORTED,
            shell=SHELL
        );


@task(pre=[setup, flatc])
def libmediasoup_worker(ctx):
    """
    Compile libmediasoup-worker library
    """
    with ctx.cd(f'"{WORKER_DIR}"'):
        ctx.run(
            f'"{MESON}" compile -C "{BUILD_DIR}" -j {NUM_CORES} libmediasoup-worker',
            echo=True,
            pty=PTY_SUPPORTED,
            shell=SHELL
        );
    with ctx.cd(f'"{WORKER_DIR}"'):
        ctx.run(
            f'"{MESON}" install -C "{BUILD_DIR}" --no-rebuild --tags libmediasoup-worker',
            echo=True,
            pty=PTY_SUPPORTED,
            shell=SHELL
        );


@task(pre=[setup, flatc])
def xcode(ctx):
    """
    Setup Xcode project
    """
    with ctx.cd(f'"{WORKER_DIR}"'):
        ctx.run(
            f'"{MESON}" setup --buildtype {MEDIASOUP_BUILDTYPE.lower()} --backend xcode "{MEDIASOUP_OUT_DIR}/xcode"',
            echo=True,
            pty=PTY_SUPPORTED,
            shell=SHELL
        );


@task
def lint(ctx):
    """
    Lint source code
    """
    with ctx.cd(f'"{WORKER_DIR}"'):
        ctx.run(
            f'"{NPM}" run lint --prefix scripts/',
            echo=True,
            pty=PTY_SUPPORTED,
            shell=SHELL
        );

    if not os.path.isdir(PIP_PYLINT_DIR):
        # Install pylint using pip into our custom location.
        ctx.run(
            f'"{PYTHON}" -m pip install --upgrade --no-user --target="{PIP_PYLINT_DIR}" pylint=={PYLINT_VERSION}',
            echo=True,
            pty=PTY_SUPPORTED,
            shell=SHELL
        );

    with ctx.cd(f'"{WORKER_DIR}"'):
        ctx.run(
            f'"{PYTHON}" -m pylint tasks.py',
            echo=True,
            pty=PTY_SUPPORTED,
            shell=SHELL
        );


@task
def format(ctx):
    """
    Format source code according to lint rules
    """
    with ctx.cd(f'"{WORKER_DIR}"'):
        ctx.run(
            f'"{NPM}" run format --prefix scripts/',
            echo=True,
            pty=PTY_SUPPORTED,
            shell=SHELL
        );


@task(pre=[setup, flatc])
def test(ctx):
    """
    Run worker tests
    """
    with ctx.cd(f'"{WORKER_DIR}"'):
        ctx.run(
            f'"{MESON}" compile -C "{BUILD_DIR}" -j {NUM_CORES} mediasoup-worker-test',
            echo=True,
            pty=PTY_SUPPORTED,
            shell=SHELL
        );
    with ctx.cd(f'"{WORKER_DIR}"'):
        ctx.run(
            f'"{MESON}" install -C "{BUILD_DIR}" --no-rebuild --tags mediasoup-worker-test',
            echo=True,
            pty=PTY_SUPPORTED,
            shell=SHELL
        );

    mediasoup_worker_test = 'mediasoup-worker-test.exe' if os.name == 'nt' else 'mediasoup-worker-test';
    mediasoup_test_tags = os.getenv('MEDIASOUP_TEST_TAGS') or '';

    with ctx.cd(f'"{WORKER_DIR}"'):
        ctx.run(
            f'"{BUILD_DIR}/{mediasoup_worker_test}" --invisibles --colour-mode=ansi {mediasoup_test_tags}',
            echo=True,
            pty=PTY_SUPPORTED,
            shell=SHELL
        );


@task(pre=[call(setup, meson_args=MESON_ARGS + ' -Db_sanitize=address -Db_lundef=false'), flatc])
def test_asan_address(ctx):
    """
    Run worker test with Address Sanitizer with '-fsanitize=address'
    """
    with ctx.cd(f'"{WORKER_DIR}"'):
        ctx.run(
            f'"{MESON}" compile -C "{BUILD_DIR}" -j {NUM_CORES} mediasoup-worker-test-asan-address',
            echo=True,
            pty=PTY_SUPPORTED,
            shell=SHELL
        );
    with ctx.cd(f'"{WORKER_DIR}"'):
        ctx.run(
            f'"{MESON}" install -C "{BUILD_DIR}" --no-rebuild --tags mediasoup-worker-test-asan-address',
            echo=True,
            pty=PTY_SUPPORTED,
            shell=SHELL
        );

    mediasoup_test_tags = os.getenv('MEDIASOUP_TEST_TAGS') or '';

    with ctx.cd(f'"{WORKER_DIR}"'):
        ctx.run(
            f'ASAN_OPTIONS=detect_leaks=1 "{BUILD_DIR}/mediasoup-worker-test-asan-address" --invisibles {mediasoup_test_tags}',
            echo=True,
            pty=PTY_SUPPORTED,
            shell=SHELL
        );


@task(pre=[call(setup, meson_args=MESON_ARGS + ' -Db_sanitize=undefined -Db_lundef=false'), flatc])
def test_asan_undefined(ctx):
    """
    Run worker test with undefined Sanitizer with -fsanitize=undefined
    """
    with ctx.cd(f'"{WORKER_DIR}"'):
        ctx.run(
            f'"{MESON}" compile -C "{BUILD_DIR}" -j {NUM_CORES} mediasoup-worker-test-asan-undefined',
            echo=True,
            pty=PTY_SUPPORTED,
            shell=SHELL
        );
    with ctx.cd(f'"{WORKER_DIR}"'):
        ctx.run(
            f'"{MESON}" install -C "{BUILD_DIR}" --no-rebuild --tags mediasoup-worker-test-asan-undefined',
            echo=True,
            pty=PTY_SUPPORTED,
            shell=SHELL
        );

    mediasoup_test_tags = os.getenv('MEDIASOUP_TEST_TAGS') or '';

    with ctx.cd(f'"{WORKER_DIR}"'):
        ctx.run(
            f'ASAN_OPTIONS=detect_leaks=1 "{BUILD_DIR}/mediasoup-worker-test-asan-undefined" --invisibles {mediasoup_test_tags}',
            echo=True,
            pty=PTY_SUPPORTED,
            shell=SHELL
        );


@task(pre=[call(setup, meson_args=MESON_ARGS + ' -Db_sanitize=thread -Db_lundef=false'), flatc])
def test_asan_thread(ctx):
    """
    Run worker test with thread Sanitizer with -fsanitize=thread
    """
    with ctx.cd(f'"{WORKER_DIR}"'):
        ctx.run(
            f'"{MESON}" compile -C "{BUILD_DIR}" -j {NUM_CORES} mediasoup-worker-test-asan-thread',
            echo=True,
            pty=PTY_SUPPORTED,
            shell=SHELL
        );
    with ctx.cd(f'"{WORKER_DIR}"'):
        ctx.run(
            f'"{MESON}" install -C "{BUILD_DIR}" --no-rebuild --tags mediasoup-worker-test-asan-thread',
            echo=True,
            pty=PTY_SUPPORTED,
            shell=SHELL
        );

    mediasoup_test_tags = os.getenv('MEDIASOUP_TEST_TAGS') or '';

    with ctx.cd(f'"{WORKER_DIR}"'):
        ctx.run(
            f'ASAN_OPTIONS=detect_leaks=1 "{BUILD_DIR}/mediasoup-worker-test-asan-thread" --invisibles {mediasoup_test_tags}',
            echo=True,
            pty=PTY_SUPPORTED,
            shell=SHELL
        );


@task
def tidy(ctx):
    """
    Perform C++ checks with clang-tidy
    """
    mediasoup_tidy_checks = os.getenv('MEDIASOUP_TIDY_CHECKS') or '';
    mediasoup_tidy_files = os.getenv('MEDIASOUP_TIDY_FILES') or '';
    mediasoup_clang_tidy_dir = os.getenv('MEDIASOUP_CLANG_TIDY_DIR');

    # MEDIASOUP_CLANG_TIDY_DIR env variable is mandatory.
    # NOTE: sys.exit(text) exists the program with status code 1.
    if not mediasoup_clang_tidy_dir:
        sys.exit('missing MEDIASOUP_CLANG_TIDY_DIR env variable');

    if mediasoup_tidy_checks:
        mediasoup_tidy_checks = '-*,' + mediasoup_tidy_checks;

    if not mediasoup_tidy_files:
        mediasoup_tidy_files = 'src/*.cpp src/**/*.cpp src/**/**.cpp';

    with ctx.cd(f'"{WORKER_DIR}"'):
        ctx.run(
            f'"{PYTHON}" "{mediasoup_clang_tidy_dir}/run-clang-tidy" -clang-tidy-binary="{mediasoup_clang_tidy_dir}/clang-tidy" -clang-apply-replacements-binary="{mediasoup_clang_tidy_dir}/clang-apply-replacements" -p="{BUILD_DIR}" -j={NUM_CORES} -fix -checks={mediasoup_tidy_checks} {mediasoup_tidy_files}',
            echo=True,
            pty=PTY_SUPPORTED,
            shell=SHELL
        );


@task(pre=[call(setup, meson_args=MESON_ARGS + ' -Db_sanitize=address -Db_lundef=false'), flatc])
def fuzzer(ctx):
    """
    Build the mediasoup-worker-fuzzer binary (which uses libFuzzer)
    """

    # NOTE: We need to pass '-Db_sanitize=address' to enable fuzzer in all Meson
    # subprojects, so we pass it to the setup() task.

    with ctx.cd(f'"{WORKER_DIR}"'):
        ctx.run(
            f'"{MESON}" compile -C "{BUILD_DIR}" -j {NUM_CORES} mediasoup-worker-fuzzer',
            echo=True,
            pty=PTY_SUPPORTED,
            shell=SHELL
        );
    with ctx.cd(f'"{WORKER_DIR}"'):
        ctx.run(
            f'"{MESON}" install -C "{BUILD_DIR}" --no-rebuild --tags mediasoup-worker-fuzzer',
            echo=True,
            pty=PTY_SUPPORTED,
            shell=SHELL
        );


@task
def fuzzer_run_all(ctx):
    """
    Run all fuzzer cases
    """
    with ctx.cd(f'"{WORKER_DIR}"'):
        ctx.run(
            f'LSAN_OPTIONS=verbosity=1:log_threads=1 "{BUILD_DIR}/mediasoup-worker-fuzzer" -artifact_prefix=fuzzer/reports/ -max_len=1400 fuzzer/new-corpus deps/webrtc-fuzzer-corpora/corpora/stun-corpus deps/webrtc-fuzzer-corpora/corpora/rtp-corpus deps/webrtc-fuzzer-corpora/corpora/rtcp-corpus',
            echo=True,
            pty=PTY_SUPPORTED,
            shell=SHELL
        );


@task
def docker(ctx):
    """
    Build a Linux Ubuntu Docker image with fuzzer capable clang++
    """
    if os.getenv('DOCKER_NO_CACHE') == 'true':
        with ctx.cd(f'"{WORKER_DIR}"'):
            ctx.run(
                f'"{DOCKER}" build -f Dockerfile --no-cache --tag mediasoup/docker:latest .',
                echo=True,
                pty=PTY_SUPPORTED,
                shell=SHELL
            );
    else:
        with ctx.cd(f'"{WORKER_DIR}"'):
            ctx.run(
                f'"{DOCKER}" build -f Dockerfile --tag mediasoup/docker:latest .',
                echo=True,
                pty=PTY_SUPPORTED,
                shell=SHELL
            );


@task
def docker_run(ctx):
    """
    Run a container of the Ubuntu Docker image created in the docker task
    """
    with ctx.cd(f'"{WORKER_DIR}"'):
        ctx.run(
            f'"{DOCKER}" run --name=mediasoupDocker -it --rm --privileged --cap-add SYS_PTRACE -v "{WORKER_DIR}/../:/mediasoup" mediasoup/docker:latest',
            echo=True,
            pty=True, # NOTE: Needed to enter the terminal of the Docker image.
            shell=SHELL
        );


@task
def docker_alpine(ctx):
    """
    Build a Linux Alpine Docker image
    """
    if os.getenv('DOCKER_NO_CACHE') == 'true':
        with ctx.cd(f'"{WORKER_DIR}"'):
            ctx.run(
                f'"{DOCKER}" build -f Dockerfile.alpine --no-cache --tag mediasoup/docker-alpine:latest .',
                echo=True,
                pty=PTY_SUPPORTED,
                shell=SHELL
            );
    else:
        with ctx.cd(f'"{WORKER_DIR}"'):
            ctx.run(
                f'"{DOCKER}" build -f Dockerfile.alpine --tag mediasoup/docker-alpine:latest .',
                echo=True,
                pty=PTY_SUPPORTED,
                shell=SHELL
            );


@task
def docker_alpine_run(ctx):
    """
    Run a container of the Alpine Docker image created in the docker_alpine task
    """
    with ctx.cd(f'"{WORKER_DIR}"'):
        ctx.run(
            f'"{DOCKER}" run --name=mediasoupDockerAlpine -it --rm --privileged --cap-add SYS_PTRACE -v "{WORKER_DIR}/../:/mediasoup" mediasoup/docker-alpine:latest',
            echo=True,
            pty=True, # NOTE: Needed to enter the terminal of the Docker image.
            shell=SHELL
        );
