# Changelog

# NEXT

# 0.17.1

- Update Rust toolchain channel to version 1.79.0 (PR #1409).
- Updates from mediasoup TypeScript `3.14.7..=3.14.10`.
- General mediasoup changes:
  - Worker: Add `enable_liburing` boolean option (`true` by default) to disable `io_uring` even if it's supported by the prebuilt `mediasoup-worker` and by current host (PR #1442).

# 0.17.0

- Updates from mediasoup TypeScript `3.13.18..=3.14.6`.
- General mediasoup changes:
  - Worker: Fix crash when closing `WebRtcServer` with active `WebRtcTransports` (PR #1390).
  - `Worker: Fix memory leak when using `WebRtcServer` with TCP enabled (PR #1389).
  - OPUS: Fix DTX detection (PR #1357).
  - `TransportListenInfo`: Add `portRange` (deprecate worker port range) (PR #1365).
  - Update worker FlatBuffers to 24.3.6-1 (fix cannot set temporal layer 0) (PR #1348).
  - Fix DTLS packets do not honor configured DTLS MTU (attempt 3) (PR #1345).
  - Add server side ICE consent checks to detect silent WebRTC disconnections (PR #1332).
  - `TransportListenInfo`: "announced ip" can also be a hostname (PR #1322).
  - `TransportListenInfo`: Rename "announced ip" to "announced address" (PR #1324).

# 0.16.0

- Updates from mediasoup TypeScript `3.13.13..=3.13.17`.
- General mediasoup changes:
  - `TransportListenInfo.announced_ip` can also be a hostname (PR #1322).
  - `TransportListenInfo.announced_ip` is now `announced_address`, `IceCandidate.ip` is now `IceCandidate.address` and `TransportTuple.local_ip` is not `TransportTuple.local_address` (PR #1324).

# 0.15.0

- Expose DataChannel string message as binary (PR #1289).

# 0.14.0

- Updates from mediasoup TypeScript `3.13.8..=3.13.12`.
- Update h264-profile-level-id dependency to 0.2.0.
- Fix docs build (PR #1271).
- Rename `data_consumer::on_producer_resume` to `data_consumer::on_data_producer_resume` (PR #1271).

# 0.13.0

- Updates from mediasoup TypeScript `3.13.0..=3.13.7`.
- General mediasoup changes:
  - Switch from JSON based messages to `flatbuffers` (PR #1064).
  - Enable `liburing` usage for Linux (kernel versions >= 6) (PR #1218).
  - Add pause/resume API in `DataProducer` and `DataConsumer` (PR #1104).
  - DataChannel subchannels feature (PR #1152).
  - `Worker`: Make DTLS fragment stay within MTU size range (PR #1156).
  - Replace make + Makefile with Python Invoke library + tasks.py (also fix installation under path with whitespaces) (PR #1239).

# 0.12.0

- Updates from mediasoup TypeScript `3.11.9..=3.12.16`.

# 0.11.4

- Fix consuming data producer from direct transport by data consumer on non-direct transport.

# 0.11.3

- Updates from mediasoup TypeScript `3.11.3..=3.11.8`.

# 0.11.2

- Updates from mediasoup TypeScript `3.10.11..=3.11.2`.

# 0.11.1

- Updates from mediasoup TypeScript `3.10.7..=3.10.10`.

# 0.11.0

- Updates from mediasoup TypeScript `3.10.2..=3.10.6`.

# 0.10.0

- Updates from mediasoup TypeScript `3.9.10..=3.10.1`.
- `WebRtcServer`: A new class that brings to `WebRtcTransports` the ability to listen on a single UDP/TCP port (PR #834, PR #845).
- Minor API breaking changes.

# 0.9.3

- Fix a segfaults in tests and under multithreaded executor.
- Fix another racy deadlock situation.
- Expose hierarchical dependencies of ownership of Rust data structures, now it is possible to call `consumer.transport().router().worker().worker_manager()`.
- General mediasoup changes:
  - ICE renomination support (PR #756).
  - Update `libuv` to 1.43.0.
  - TCC client optimizations for faster and more stable BWE (PR #712 by @ggarber).
  - Added support for RTP abs-capture-time header (PR #761 by @oto313).
  - Fix VP9 kSVC forwarding logic to not forward lower unneded layers (PR #778 by @ggarber).
  - Fix update bandwidth estimation configuration and available bitrate when updating max outgoing bitrate (PR #779 by @ggarber).
  - Optimize RTP header extension handling (PR #786).
  - `RateCalculator`: Reset optimization (PR #785).
  - Fix frozen video due to double call to `Consumer::UserOnTransportDisconnected()` (PR #788, thanks to @ggarber for exposing this issue in PR #787).

# 0.9.2

- Update `lru` dependency to fix security vulnerability

# 0.9.1

- Fix cleanup of build artifacts.
- Make `Transport` implement `Send`.
- Another fix to rare deadlock.
- Improved Windows support (doesn't require MSVS activation).

# 0.9.0

- Fix for receiving data over payload channel.
- Support thread initializer function for worker threads, can be used for pinning worker threads to CPU cores.
- Significant worker communication optimizations (especially latency).
- Switch from file descriptors to function calls when communicating with worker.
- Various optimizations that caused minor breaking changes to public API.
- Requests no longer have internal timeout, but they can now be cancelled, add your own timeouts on top if needed.
- Windows support.
- General mediasoup changes:
  - Replaces GYP build system with fully-functional Meson build system (PR #622).
  - `Consumer`: Modification of bitrate allocation algorithm (PR #708).
  - Single H264/H265 codec configuration in `supportedRtpCapabilities` (PR #718).

# 0.8.5

- Fix types for `round_trip_time` and `bitrate_by_layer` fields `ProducerStat` and `ConsumerStat`.
- Accumulation of worker fixes.

# 0.8.4

- Add Active Speaker Observer to prelude.
- Fix consumers preventing producers from being closed (regression introduced in 0.8.3).

# 0.8.3

- prelude module containing traits and structs that should be sufficient for most basic mediasoup-based apps.
- Dominant Speaker Event (PR #603 by @SteveMcFarlin).

### 0.8.2

- Support for optional fixed port on transports.

### 0.8.1

- Add convenience methods for getting information from `TransportTuple` enum, especially local IP/port.
- Add `mid` option in `ConsumerOptions` to provide way to override MID
- Add convenience method `ConsumerStats::consumer_stat()`.

### 0.8.0

- `NonClosingProducer` removed (use `PipedProducer` instead, they were identical).
- `RtpHeaderExtensionUri::as_str()` now takes `self` instead of `&self`.
- `kind` field of `RtpHeaderExtension` is no longer optional.
- Refactor `ScalabilityMode` from being a string to enum, make sure layers are not zero on type system level.
- Concrete types for info field of tracing events.

### 0.7.2

- Thread and memory safety fixes in mediasoup-sys.
- macOS support.
- `NonClosingProducer` renamed into `PipedProducer` with better docs.
- Internal restructuring of modules for better compatibility with IDEs.
- Feature level updated to mediasoup `3.7.6`.

### 0.7.0

- Switch from running C++ worker processes to worker threads using mediasoup-sys that wraps mediasoup-worker into library.
- Simplify `WorkerManager::new()` and `WorkerManager::with_executor()` API as the result of above.
- Support `rtxPacketsDiscarded` in `Producer` stats.
- Enable Rust 2018 idioms warnings.
- Make sure all public types have `Debug` implementation on them.
- Enforce docs on public types and add missing documentation.
- Remove `RtpCodecParametersParameters::new()` (`RtpCodecParametersParameters::default()` does the same thing).

### 0.6.0

Initial upstreamed release.
