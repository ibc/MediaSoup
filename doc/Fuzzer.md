# Fuzzer

Once we have built the `mediasoup-worker-fuzzer` target in a Linux environment with `fuzzer` capable clang (see `make fuzzer` documentation in [Building](Building.md)) we can then execute the fuzzer binary at `worker/out/Release/mediasoup-worker-fuzzer`.

**NOTE:** From now on, we assume we are in the mediasoup `worker` directory.

## Related documentation

- [libFuzzer documentation](http://llvm.org/docs/LibFuzzer.html)
- [libFuzzer Tutorial](https://github.com/google/fuzzer-test-suite/blob/master/tutorial/libFuzzerTutorial.md)
- [webrtcH4cKS ~ Lets get better at fuzzing in 2019](https://webrtchacks.com/lets-get-better-at-fuzzing-in-2019-heres-how/)
- [OSS-Fuzz](https://github.com/google/oss-fuzz) - Continuous fuzzing of open source software ("fuzz for me")

## Corpus files

The `deps/webrtc-fuzzer-corpora/corpora` directory has corpus directories taken from the [webrtc-fuzzer-corpora](https://github.com/RTC-Cartel/webrtc-fuzzer-corpora) project. They should be use to feed fuzzer with appropriate input.

However, given how `libFuzzer` [works](http://llvm.org/docs/LibFuzzer.html#options), the first directory given as command line parameter is not just used for reading corpus files, but also to store newly generated ones. So, it's recommended to pass `fuzzer/new-corpus` as first directory. Such a directory is gitignored.

## Crash reports

When the fuzzer detects an issue it generates a crash report file which contains the bytes given as input. Those files can be individually used later (instead of passing corpus directories) to the fuzzer to verify that the issue has been fixed.

The `fuzzer/reports` directory should be used to store those new crash reports. In order to use it, the following option must be given to the fuzzer executable:

```bash
-artifact_prefix=fuzzer/reports/
```

## Others

It's recommended to (also) pass the following options to the fuzzer:

- `-max_len=1400`: We don't need much more input size.

For memory leak detection enable the following environment variable:

- `LSAN_OPTIONS=verbosity=1:log_threads=1`

The mediasoup-worker fuzzer reads some custom environment variables to decide which kind of fuzzing perform:

- `MS_FUZZ_STUN=1`: Enable STUN fuzzer.
- `MS_FUZZ_DTLS=1`: Enable DTLS fuzzer.
- `MS_FUZZ_RTP=1`: Enable RTP fuzzer.
- `MS_FUZZ_RTCP=1`: Enable RTCP fuzzer.
- `MS_FUZZ_CODECS=1`: Enable audio/video codecs fuzzer.
- `MS_FUZZ_UTILS=1`: Enable C++ utils fuzzer.
- If none of them is given, then **all** fuzzers are enabled.

The log level can also be set by setting the `MS_FUZZ_LOG_LEVEL` environment variable to "debug", "warn" or "error" (it is "none" if unset).

## Usage examples

- Detect memory leaks and just fuzz STUN:

```bash
MS_FUZZ_STUN=1 LSAN_OPTIONS=verbosity=1:log_threads=1 ./out/Release/mediasoup-worker-fuzzer -artifact_prefix=fuzzer/reports/ -max_len=1400 fuzzer/new-corpus deps/webrtc-fuzzer-corpora/corpora/stun-corpus
```

- Detect memory leaks and just fuzz DTLS:

```bash
MS_FUZZ_DTLS=1 LSAN_OPTIONS=verbosity=1:log_threads=1 ./out/Release/mediasoup-worker-fuzzer -artifact_prefix=fuzzer/reports/ -max_len=1400 fuzzer/new-corpus
```

- Detect memory leaks and just fuzz RTP:

```bash
MS_FUZZ_RTP=1 LSAN_OPTIONS=verbosity=1:log_threads=1 ./out/Release/mediasoup-worker-fuzzer -artifact_prefix=fuzzer/reports/ -max_len=1400 fuzzer/new-corpus deps/webrtc-fuzzer-corpora/corpora/rtp-corpus
```

- Detect memory leaks and just fuzz RTCP:

```bash
MS_FUZZ_RTCP=1 LSAN_OPTIONS=verbosity=1:log_threads=1 ./out/Release/mediasoup-worker-fuzzer -artifact_prefix=fuzzer/reports/ -max_len=1400 fuzzer/new-corpus deps/webrtc-fuzzer-corpora/corpora/rtcp-corpus
```

- Detect memory leaks and just fuzz audio/video codecs:

```bash
MS_FUZZ_CODECS=1 LSAN_OPTIONS=verbosity=1:log_threads=1 ./out/Release/mediasoup-worker-fuzzer -artifact_prefix=fuzzer/reports/ -max_len=1400 fuzzer/new-corpus
```

- Detect memory leaks and just fuzz mediasoup-worker C++ utils:

```bash
MS_FUZZ_UTILS=1 LSAN_OPTIONS=verbosity=1:log_threads=1 ./out/Release/mediasoup-worker-fuzzer -artifact_prefix=fuzzer/reports/ -max_len=2000 fuzzer/new-corpus
```

- Detect memory leaks and fuzz everything with log level "warn":

```bash
MS_FUZZ_LOG_LEVEL=warn LSAN_OPTIONS=verbosity=1:log_threads=1 ./out/Release/mediasoup-worker-fuzzer -artifact_prefix=fuzzer/reports/ -max_len=1400 fuzzer/new-corpus deps/webrtc-fuzzer-corpora/corpora/stun-corpus deps/webrtc-fuzzer-corpora/corpora/rtp-corpus deps/webrtc-fuzzer-corpora/corpora/rtcp-corpus
```

- Verify that a specific crash is fixed:

```bash
LSAN_OPTIONS=verbosity=1:log_threads=1 ./out/Release/mediasoup-worker-fuzzer fuzzer/reports/crash-f39771f7a03c0e7e539d4e52f48f7adad8976404
```
