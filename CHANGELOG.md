# Changelog

### NEXT

- Node: Expose `Index` interface in `types.indexTypes` or via `import { Index as MediasoupIndex } from 'mediasoup/lib/indexTypes'` ([PR #1485](https://github.com/versatica/mediasoup/pull/1485)).

### 3.15.2

- `Worker`: Fix crash when using colliding `portRange` values in different transports ([PR #1469](https://github.com/versatica/mediasoup/pull/1469)).

### 3.15.1

- Expose `extras` namespace which exports `EnhancedEventEmitter` and `enhancedOnce()` for now ([PR #1464](https://github.com/versatica/mediasoup/pull/1464)).

### 3.15.0

- Node: Add TypeScript interfaces for all exported classes ([PR #1463](https://github.com/versatica/mediasoup/pull/1463)).
- Node: Add new `transport.type` getter than returns `'webrtc' | 'plain' | 'pipe' | 'direct'` ([PR #1463](https://github.com/versatica/mediasoup/pull/1463)).
- Node: Add new `rtpObserver.type` getter than returns `'activespeaker' | 'audiolevel'` ([PR #1463](https://github.com/versatica/mediasoup/pull/1463)).

### 3.14.16

- `SimulcastConsumer`: Fix cannot switch layers if initial `tsReferenceSpatialLayer disappears` disappears ([PR #1459](https://github.com/versatica/mediasoup/pull/1459) by @Lynnworld).

### 3.14.15

- Update worker abseil-cpp dependency to 20240722.0 LTS (fixes compilation for FreeBSD systems) ([PR #1457](https://github.com/versatica/mediasoup/pull/1457), credits to @garrettboone).

### 3.14.14

- Sign self generated DTLS certificate with SHA256 ([PR #1450](https://github.com/versatica/mediasoup/pull/1450)).
- Node: Fix `mediasoup.types` exported types are empty ([PR #1453](https://github.com/versatica/mediasoup/pull/1453)).

### 3.14.13

- Node: Fix regression in exported `mediasoup.types` (classes are now exported as classes instead of types).

### 3.14.12

- `Worker`: Fix `io_uring` support detection ([PR #1445](https://github.com/versatica/mediasoup/pull/1445)).
- Mitigate libsrtp wraparound with loss decryption failure ([PR #1438](https://github.com/versatica/mediasoup/pull/1438)).
- Node: New `setLogEventListeners()` utility to get log events ([PR #1448](https://github.com/versatica/mediasoup/pull/1448)).

### 3.14.11

- `Worker`: Fix `disableLiburing` option in `WorkerSettings` ([PR #1444](https://github.com/versatica/mediasoup/pull/1444)).

### 3.14.10

- CI: Support Node 22 ([PR #1434](https://github.com/versatica/mediasoup/pull/1434)).
- Update ESLint to version 9 ([PR #1435](https://github.com/versatica/mediasoup/pull/1435)).
- `Worker`: Add `disableLiburing` boolean option (`false` by default) to disable `io_uring` even if it's supported by the prebuilt `mediasoup-worker` and by current host ([PR #1442](https://github.com/versatica/mediasoup/pull/1442)).

### 3.14.9

- Worker: Test, fix buffer overflow ([PR #1419](https://github.com/versatica/mediasoup/pull/1419)).
- Bump up Meson from 1.3.0 to 1.5.0 ([PR #1424](https://github.com/versatica/mediasoup/pull/1424)).
- Node: Export new `WorkerObserver`, `ProducerObserver`, etc. TypeScript types ([PR #1430](https://github.com/versatica/mediasoup/pull/1430)).
- Fix frozen video in simulcast due to wrong dropping of padding only packets ([PR #1431](https://github.com/versatica/mediasoup/pull/1431), thanks to @quanli168).

### 3.14.8

- Add support for 'playout-delay' RTP extension ([PR #1412](https://github.com/versatica/mediasoup/pull/1412) by @DavidNegro).

### 3.14.7

- `SimulcastConsumer`: Fix increase layer when current layer has not receive SR ([PR #1098](https://github.com/versatica/mediasoup/pull/1098) by @penguinol).
- Ignore RTP packets with empty payload ([PR #1403](https://github.com/versatica/mediasoup/pull/1403), credits to @ggarber).

### 3.14.6

- Worker: Fix potential double free when ICE consent check fails ([PR #1393](https://github.com/versatica/mediasoup/pull/1393)).

### 3.14.5

- Worker: Fix memory leak when using `WebRtcServer` with TCP enabled ([PR #1389](https://github.com/versatica/mediasoup/pull/1389)).
- Worker: Fix crash when closing `WebRtcServer` with active `WebRtcTransports` ([PR #1390](https://github.com/versatica/mediasoup/pull/1390)).

### 3.14.4

- Worker: Fix crash. `RtcpFeedback` parameter is optional ([PR #1387](https://github.com/versatica/mediasoup/pull/1387), credits to @Lynnworld).

### 3.14.3

- Worker: Fix possible value overflow in `FeedbackRtpTransport.cpp` ([PR #1386](https://github.com/versatica/mediasoup/pull/1386), credits to @Lynnworld).

### 3.14.2

- Update worker subprojects ([PR #1376](https://github.com/versatica/mediasoup/pull/1376)).
- OPUS: Fix DTX detection ([PR #1357](https://github.com/versatica/mediasoup/pull/1357)).
- Worker: Fix sending callback leaks ([PR #1383](https://github.com/versatica/mediasoup/pull/1383), credits to @Lynnworld).

### 3.14.1

- Node: Bring transport `rtpPacketLossReceived` and `rtpPacketLossSent` stats back ([PR #1371](https://github.com/versatica/mediasoup/pull/1371)).

### 3.14.0

- `TransportListenInfo`: Add `portRange` (deprecate worker port range) ([PR #1365](https://github.com/versatica/mediasoup/pull/1365)).
- Require Node.js >= 18 ([PR #1365](https://github.com/versatica/mediasoup/pull/1365)).

### 3.13.24

- Node: Fix missing `bitrateByLayer` field in stats of `RtpRecvStream` in Node ([PR #1349](https://github.com/versatica/mediasoup/pull/1349)).
- Update worker dependency libuv to 1.48.0.
- Update worker FlatBuffers to 24.3.6-1 (fix cannot set temporal layer 0) ([PR #1348](https://github.com/versatica/mediasoup/pull/1348)).

### 3.13.23

- Fix DTLS packets do not honor configured DTLS MTU (attempt 3) ([PR #1345](https://github.com/versatica/mediasoup/pull/1345)).

### 3.13.22

- Fix wrong publication of mediasoup NPM 3.13.21.

### 3.13.21

- Revert ([PR #1156](https://github.com/versatica/mediasoup/pull/1156)) "Make DTLS fragment stay within MTU size range" because it causes a memory leak ([PR #1342](https://github.com/versatica/mediasoup/pull/1342)).

### 3.13.20

- Add server side ICE consent checks to detect silent WebRTC disconnections ([PR #1332](https://github.com/versatica/mediasoup/pull/1332)).
- Fix regression (crash) in transport-cc feedback generation ([PR #1339](https://github.com/versatica/mediasoup/pull/1339)).

### 3.13.19

- Node: Fix `router.createWebRtcTransport()` with `listenIps` ([PR #1330](https://github.com/versatica/mediasoup/pull/1330)).

### 3.13.18

- Make transport-cc feedback work similarly to libwebrtc ([PR #1088](https://github.com/versatica/mediasoup/pull/1088) by @penguinol).
- `TransportListenInfo`: "announced ip" can also be a hostname ([PR #1322](https://github.com/versatica/mediasoup/pull/1322)).
- `TransportListenInfo`: Rename "announced ip" to "announced address" ([PR #1324](https://github.com/versatica/mediasoup/pull/1324)).
- CI: Add `macos-14`.

### 3.13.17

- Fix prebuilt worker download ([PR #1319](https://github.com/versatica/mediasoup/pull/1319) by @brynrichards).
- libsrtp: Update to v3.0-alpha version in our fork.

### 3.13.16

- Node: Add new `worker.on('subprocessclose')` event ([PR #1307](https://github.com/versatica/mediasoup/pull/1307)).

### 3.13.15

- Add worker prebuild binary for Linux kernel 6 ([PR #1300](https://github.com/versatica/mediasoup/pull/1300)).

### 3.13.14

- Avoid modification of user input data ([PR #1285](https://github.com/versatica/mediasoup/pull/1285)).
- `TransportListenInfo`: Add transport socket flags ([PR #1291](https://github.com/versatica/mediasoup/pull/1291)).
  - Note that `flags.ipv6Only` is `false` by default.
- `TransportListenInfo`: Ignore given socket flags if not suitable for given IP family or transport ([PR #1294](https://github.com/versatica/mediasoup/pull/1294)).
- Meson: Remove `-Db_pie=true -Db_staticpic=true` args ([PR #1293](https://github.com/versatica/mediasoup/pull/1293)).
- Add RTCP Sender Report trace event ([PR #1267](https://github.com/versatica/mediasoup/pull/1267) by @GithubUser8080).

### 3.13.13

- Worker: Do not use references for async callbacks ([PR #1274](https://github.com/versatica/mediasoup/pull/1274)).
- liburing: Enable zero copy ([PR #1273](https://github.com/versatica/mediasoup/pull/1273)).
- Fix build on musl based systems (such as Alpine Linux) ([PR #1279](https://github.com/versatica/mediasoup/pull/1279)).

### 3.13.12

- Worker: Disable `RtcLogger` usage if not enabled ([PR #1264](https://github.com/versatica/mediasoup/pull/1264)).
- npm installation: Don't require Python if valid worker prebuilt binary is fetched ([PR #1265](https://github.com/versatica/mediasoup/pull/1265)).
- Update h264-profile-level-id NPM dependency to 1.1.0.

### 3.13.11

- liburing: Avoid extra memcpy on RTP ([PR #1258](https://github.com/versatica/mediasoup/pull/1258)).
- libsrtp: Use our own fork with performance gain ([PR #1260](https://github.com/versatica/mediasoup/pull/1260)).
- `DataConsumer`: Add `addSubchannel()` and `removeSubchannel()` methods ([PR #1263](https://github.com/versatica/mediasoup/pull/1263)).
- Fix Rust `DataConsumer` ([PR #1262](https://github.com/versatica/mediasoup/pull/1262)).

### 3.13.10

- `tasks.py`: Always include `--no-user` in `pip install` commands to avoid the "can not combine --user and --target" error in Windows ([PR #1257](https://github.com/versatica/mediasoup/pull/1257)).

### 3.13.9

- Update worker liburing dependency to 2.4-2 ([PR #1254](https://github.com/versatica/mediasoup/pull/1254)).
- liburing: Enable by default ([PR 1255](https://github.com/versatica/mediasoup/pull/1255)).

### 3.13.8

- liburing: Enable liburing usage for SCTP data delivery ([PR 1249](https://github.com/versatica/mediasoup/pull/1249)).
- liburing: Disable by default ([PR 1253](https://github.com/versatica/mediasoup/pull/1253)).

### 3.13.7

- Update worker dependencies ([PR #1201](https://github.com/versatica/mediasoup/pull/1201)):
  - abseil-cpp 20230802.0-2
  - libuv 1.47.0-1
  - OpenSSL 3.0.8-2
  - usrsctp snapshot ebb18adac6501bad4501b1f6dccb67a1c85cc299
- Enable `liburing` usage for Linux (kernel versions >= 6) ([PR #1218](https://github.com/versatica/mediasoup/pull/1218)).

### 3.13.6

- Replace make + Makefile with Python Invoke library + tasks.py (also fix installation under path with whitespaces) ([PR #1239](https://github.com/versatica/mediasoup/pull/1239)).

### 3.13.5

- Fix RTCP SDES packet size calculation ([PR #1236](https://github.com/versatica/mediasoup/pull/1236) based on PR [PR #1234](https://github.com/versatica/mediasoup/pull/1234) by @ybybwdwd).
- RTCP Compound Packet: Use a single DLRR report to hold all ssrc info sub-blocks ([PR #1237](https://github.com/versatica/mediasoup/pull/1237)).

### 3.13.4

- Fix RTCP DLRR (Delay Since Last Receiver Report) block parsing ([PR #1234](https://github.com/versatica/mediasoup/pull/1234)).

### 3.13.3

- Node: Fix issue when 'pause'/'resume' events are not emitted ([PR #1231](https://github.com/versatica/mediasoup/pull/1231) by @douglaseel).

### 3.13.2

- FBS: `LayersChangeNotification` body must be optional (fixes a crash) ([PR #1227](https://github.com/versatica/mediasoup/pull/1227)).

### 3.13.1

- Node: Extract version from `package.json` using `require()` ([PR #1217](https://github.com/versatica/mediasoup/pull/1217) by @arcinston).

### 3.13.0

- Switch from JSON based messages to FlatBuffers ([PR #1064](https://github.com/versatica/mediasoup/pull/1064)).
- Add `TransportListenInfo` in all transports and send/recv buffer size options ([PR #1084](https://github.com/versatica/mediasoup/pull/1084)).
- Add optional `rtcpListenInfo` in `PlainTransportOptions` ([PR #1099](https://github.com/versatica/mediasoup/pull/1099)).
- Add pause/resume API in `DataProducer` and `DataConsumer` ([PR #1104](https://github.com/versatica/mediasoup/pull/1104)).
- DataChannel subchannels feature ([PR #1152](https://github.com/versatica/mediasoup/pull/1152)).
- Worker: Make DTLS fragment stay within MTU size range ([PR #1156](https://github.com/versatica/mediasoup/pull/1156), based on [PR #1143](https://github.com/versatica/mediasoup/pull/1143) by @vpnts-se).

### 3.12.16

- Fix `IceServer` crash when client uses ICE renomination ([PR #1182](https://github.com/versatica/mediasoup/pull/1182)).

### 3.12.15

- Fix NPM "postinstall" task in Windows ([PR #1187](https://github.com/versatica/mediasoup/pull/1187)).

### 3.12.14

- CI: Use Node.js version 20 ([PR #1177](https://github.com/versatica/mediasoup/pull/1177)).
- Use given `PYTHON` environment variable (if given) when running `worker/scripts/getmake.py` ([PR #1186](https://github.com/versatica/mediasoup/pull/1186)).

### 3.12.13

- Bump up Meson from 1.1.0 to 1.2.1 (fixes Xcode 15 build issues) ([PR #1163](https://github.com/versatica/mediasoup/pull/1163) by @arcinston).

### 3.12.12

- Support C++20 ([PR #1150](https://github.com/versatica/mediasoup/pull/1150) by @o-u-p).

### 3.12.11

- Google Transport Feedback: Read Reference Time field as 24bits signed as per spec ([PR #1145](https://github.com/versatica/mediasoup/pull/1145)).

### 3.12.10

- Node: Rename `WebRtcTransport.webRtcServerClosed()` to `listenServerClosed()` ([PR #1141](https://github.com/versatica/mediasoup/pull/1141) by @piranna).

### 3.12.9

- Fix RTCP SDES ([PR #1139](https://github.com/versatica/mediasoup/pull/1139)).

### 3.12.8

- Export `workerBin` absolute path ([PR #1123](https://github.com/versatica/mediasoup/pull/1123)).

### 3.12.7

- `SimulcastConsumer`: Fix lack of "layerschange" event when all streams in the producer die ([PR #1122](https://github.com/versatica/mediasoup/pull/1122)).

### 3.12.6

- Worker: Add `Transport::Destroying()` protected method ([PR #1114](https://github.com/versatica/mediasoup/pull/1114)).
- `RtpStreamRecv`: Fix jitter calculation ([PR #1117](https://github.com/versatica/mediasoup/pull/1117), thanks to @penguinol).
- Revert "Node: make types.ts only export types rather than the entire class/code" ([PR #1109](https://github.com/versatica/mediasoup/pull/1109)) because it requires `typescript` >= 5 in the apps that import mediasoup and we don't want to be that strict yet.

### 3.12.5

- `DataConsumer`: Fix removed 'bufferedamountlow' notification ([PR #1113](https://github.com/versatica/mediasoup/pull/1113)).

### 3.12.4

- Fix downloaded prebuilt binary check on Windows ([PR #1105](https://github.com/versatica/mediasoup/pull/1105) by @woodfe).

### 3.12.3

Migrate `npm-scripts.js` to `npm-scripts.mjs` (ES Module) ([PR #1093](https://github.com/versatica/mediasoup/pull/1093)).

### 3.12.2

- CI: Use `ubuntu-20.04` to build `mediasoup-worker` prebuilt on Linux ([PR #1092](https://github.com/versatica/mediasoup/pull/1092)).

### 3.12.1

- `mediasoup-worker` prebuild: Fallback to local building if fetched binary doesn't run on current host ([PR #1090](https://github.com/versatica/mediasoup/pull/1090)).

### 3.12.0

- Automate and publish prebuilt `mediasoup-worker` binaries ([PR #1087](https://github.com/versatica/mediasoup/pull/1087), thanks to @barlock for his work in ([PR #1083](https://github.com/versatica/mediasoup/pull/1083)).

### 3.11.26

- Worker: Fix NACK timer and avoid negative RTT ([PR #1082](https://github.com/versatica/mediasoup/pull/1082), thanks to @o-u-p for his work in ([PR #1076](https://github.com/versatica/mediasoup/pull/1076)).

### 3.11.25

- Worker: Require C++17, Meson >= 1.1.0 and update subprojects ([PR #1081](https://github.com/versatica/mediasoup/pull/1081)).

### 3.11.24

- `SeqManager`: Fix performance regression ([PR #1068](https://github.com/versatica/mediasoup/pull/1068), thanks to @vpalmisano for properly reporting).

### 3.11.23

- Node: Fix `appData` for `Transport` and `RtpObserver` parent classes ([PR #1066](https://github.com/versatica/mediasoup/pull/1066)).

### 3.11.22

- `RtpStreamRecv`: Only perform RTP inactivity check on simulcast streams ([PR #1061](https://github.com/versatica/mediasoup/pull/1061)).
- `SeqManager`: Properly remove old dropped entries ([PR #1054](https://github.com/versatica/mediasoup/pull/1054)).
- libwebrtc: Upgrade trendline estimator to improve low bandwidth conditions ([PR #1055](https://github.com/versatica/mediasoup/pull/1055) by @ggarber).
- libwebrtc: Fix bandwidth probation dead state ([PR #1031](https://github.com/versatica/mediasoup/pull/1031) by @vpalmisano).

### 3.11.21

- Fix check division by zero in transport congestion control ([PR #1049](https://github.com/versatica/mediasoup/pull/1049) by @ggarber).
- Fix lost pending statuses in transport CC feedback ([PR #1050](https://github.com/versatica/mediasoup/pull/1050) by @ggarber).

### 3.11.20

- `RtpStreamSend`: Reset RTP retransmission buffer upon RTP sequence number reset ([PR #1041](https://github.com/versatica/mediasoup/pull/1041)).
- `RtpRetransmissionBuffer`: Handle corner case in which received packet has lower seq than newest packet in the buffer but higher timestamp ([PR #1044](https://github.com/versatica/mediasoup/pull/1044)).
- `SeqManager`: Fix crash and add fuzzer ([PR #1045](https://github.com/versatica/mediasoup/pull/1045)).
- Node: Make `appData` TS typed and writable ([PR #1046](https://github.com/versatica/mediasoup/pull/1046), credits to @mango-martin).

### 3.11.19

- `SvcConsumer`: Properly handle VP9 K-SVC bandwidth allocation ([PR #1036](https://github.com/versatica/mediasoup/pull/1036) by @vpalmisano).

### 3.11.18

- `RtpRetransmissionBuffer`: Consider the case of packet with newest timestamp but "old" seq number ([PR #1039](https://github.com/versatica/mediasoup/pull/1039)).

### 3.11.17

- Add `transport.setMinOutgoingBitrate()` method ([PR #1038](https://github.com/versatica/mediasoup/pull/1038), credits to @ jcague).
- `RTC::RetransmissionBuffer`: Increase `RetransmissionBufferMaxItems` from 2500 to 5000.

### 3.11.16

- Fix `SeqManager`: Properly consider previous cycle dropped inputs ([PR #1032](https://github.com/versatica/mediasoup/pull/1032)).
- `RtpRetransmissionBuffer`: Get rid of not necessary `startSeq` private member ([PR #1029](https://github.com/versatica/mediasoup/pull/1029)).
- Node: Upgrade TypeScript to 5.0.2.

### 3.11.15

- `RtpRetransmissionBuffer`: Fix crash and add fuzzer ([PR #1028](https://github.com/versatica/mediasoup/pull/1028)).

### 3.11.14

- Refactor RTP retransmission buffer in a separate and testable `RTC::RetransmissionBuffer` class ([PR #1023](https://github.com/versatica/mediasoup/pull/1023)).

### 3.11.13

- `AudioLevelObserver`: Use multimap rather than map to avoid conflict if various Producers generate same audio level ([PR #1021](https://github.com/versatica/mediasoup/pull/1021), issue reported by @buptlsp).

### 3.11.12

- Fix jitter calculation ([PR #1019](https://github.com/versatica/mediasoup/pull/1019), credits to @alexciarlillo and @snnz).

### 3.11.11

- Add support for RTCP NACK in OPUS ([PR #1015](https://github.com/versatica/mediasoup/pull/1015)).

### 3.11.10

- Download and use MSYS/make locally for Windows postinstall ([PR #792](https://github.com/versatica/mediasoup/pull/792) by @snnz).

### 3.11.9

- Allow simulcast with a single encoding (and N temporal layers) ([PR #1013](https://github.com/versatica/mediasoup/pull/1013)).
- Update libsrtp to 2.5.0.

### 3.11.8

- `SimulcastConsumer::GetDesiredBitrate()`: Choose the highest bitrate among all Producer streams ([PR #992](https://github.com/versatica/mediasoup/pull/992)).
- `SimulcastConsumer`: Fix frozen video when syncing keyframe is discarded due to too high RTP timestamp extra offset needed ([PR #999](https://github.com/versatica/mediasoup/pull/999), thanks to @satoren for properly reporting the issue and helping with the solution).

### 3.11.7

- libwebrtc: Fix crash due to invalid `arrival_time` value ([PR #985](https://github.com/versatica/mediasoup/pull/985) by @ggarber).
- libwebrtc: Replace `MS_ASSERT()` with `MS_ERROR()` ([PR #988](https://github.com/versatica/mediasoup/pull/988)).

### 3.11.6

- Fix wrong `PictureID` rolling over in VP9 and VP8 ([PR #984](https://github.com/versatica/mediasoup/pull/984) by @jcague).

### 3.11.5

- Require Node.js >= 16 ([PR #973](https://github.com/versatica/mediasoup/pull/973)).
- Fix wrong `Consumer` bandwidth estimation under `Producer` packet loss ([PR #962](https://github.com/versatica/mediasoup/pull/962) by @ggarber).

### 3.11.4

- Node: Migrate tests to TypeScript ([PR #958](https://github.com/versatica/mediasoup/pull/958)).
- Node: Remove compiled JavaScript from repository and compile TypeScript code on NPM `prepare` script on demand when installed via git ([PR #954](https://github.com/versatica/mediasoup/pull/954)).
- Worker: Add `RTC::Shared` singleton for RTC entities ([PR #953](https://github.com/versatica/mediasoup/pull/953)).
- Update OpenSSL to 3.0.7.

### 3.11.3

- `ChannelMessageHandlers`: Make `RegisterHandler()` not remove the existing handler if another one with same `id` is given ([PR #952](https://github.com/versatica/mediasoup/pull/952)).

### 3.11.2

- Fix installation issue in Linux due to a bug in ninja latest version 1.11.1 ([PR #948](https://github.com/versatica/mediasoup/pull/948)).

### 3.11.1

- `ActiveSpeakerObserver`: Revert 'dominantspeaker' event changes in [PR #941](https://github.com/versatica/mediasoup/pull/941) to avoid breaking changes ([PR #947](https://github.com/versatica/mediasoup/pull/947)).

### 3.11.0

- `Transport`: Remove duplicate call to method ([PR #931](https://github.com/versatica/mediasoup/pull/931)).
- RTCP: Adjust maximum compound packet size ([PR #934](https://github.com/versatica/mediasoup/pull/934)).
- `DataConsumer`: Fix `bufferedAmount` type to be a number again ([PR #936](https://github.com/versatica/mediasoup/pull/936)).
- `ActiveSpeakerObserver`: Fix 'dominantspeaker' event by having a single `Producer` as argument rather than an array with a single `Producer` into it ([PR #941](https://github.com/versatica/mediasoup/pull/941)).
- `ActiveSpeakerObserver`: Fix memory leak ([PR #942](https://github.com/versatica/mediasoup/pull/942)).
- Fix some libwebrtc issues ([PR #944](https://github.com/versatica/mediasoup/pull/944)).
- Tests: Normalize hexadecimal data representation ([PR #945](https://github.com/versatica/mediasoup/pull/945)).
- `SctpAssociation`: Fix memory violation ([PR #943](https://github.com/versatica/mediasoup/pull/943)).

### 3.10.12

- Fix worker crash due to `std::out_of_range` exception ([PR #933](https://github.com/versatica/mediasoup/pull/933)).

### 3.10.11

- RTCP: Fix trailing space needed by `srtp_protect_rtcp()` ([PR #929](https://github.com/versatica/mediasoup/pull/929)).

### 3.10.10

- Fix the JSON serialization for the payload channel `rtp` event ([PR #926](https://github.com/versatica/mediasoup/pull/926) by @mhammo).

### 3.10.9

- RTCP enhancements ([PR #914](https://github.com/versatica/mediasoup/pull/914)).

### 3.10.8

- `Consumer`: use a bitset instead of a set for supported payload types ([PR #919](https://github.com/versatica/mediasoup/pull/919)).
- RtpPacket: optimize UpdateMid() ([PR #920](https://github.com/versatica/mediasoup/pull/920)).
- Little optimizations and modernization ([PR #916](https://github.com/versatica/mediasoup/pull/916)).
- Fix SIGSEGV at `RTC::WebRtcTransport::OnIceServerTupleRemoved()` ([PR #915](https://github.com/versatica/mediasoup/pull/915), credits to @ybybwdwd).
- `WebRtcServer`: Make `port` optional (if not given, a random available port from the `Worker` port range is used) ([PR #908](https://github.com/versatica/mediasoup/pull/908) by @satoren).

### 3.10.7

- Forward `abs-capture-time` RTP extension also for audio packets ([PR #911](https://github.com/versatica/mediasoup/pull/911)).

### 3.10.6

- Node: Define TypeScript types for `internal` and `data` objects ([PR #891](https://github.com/versatica/mediasoup/pull/891)).
- `Channel` and `PayloadChannel`: Refactor `internal` with a single `handlerId` ([PR #889](https://github.com/versatica/mediasoup/pull/889)).
- `Channel` and `PayloadChannel`: Optimize message format and JSON generation ([PR #893](https://github.com/versatica/mediasoup/pull/893)).
- New C++ `ChannelMessageHandlers` class ([PR #894](https://github.com/versatica/mediasoup/pull/894)).
- Fix Rust support after recent changes ([PR #898](https://github.com/versatica/mediasoup/pull/898)).
- Modify `FeedbackRtpTransport` and tests to be compliant with latest libwebrtc code, make reference time to be unsigned ([PR #899](https://github.com/versatica/mediasoup/pull/899) by @penguinol and @sarumjanuch).

### 3.10.5

- `RtpStreamSend`: Do not store too old RTP packets ([PR #885](https://github.com/versatica/mediasoup/pull/885)).
- Log error details in channel socket. ([PR #875](https://github.com/versatica/mediasoup/pull/875) by @mstyura).

### 3.10.4

- Do not clone RTP packets if not needed ([PR #850](https://github.com/versatica/mediasoup/pull/850)).
- Fix DTLS related crash ([PR #867](https://github.com/versatica/mediasoup/pull/867)).

### 3.10.3

- `SimpleConsumer`: Fix. Only process Opus codec ([PR #865](https://github.com/versatica/mediasoup/pull/865)).
- TypeScript: Improve `WebRtcTransportOptions` type to not allow `webRtcServer` and `listenIps`options at the same time ([PR #852](https://github.com/versatica/mediasoup/pull/852)).

### 3.10.2

- Fix release contents by including `meson_options.txt` ([PR #863](https://github.com/versatica/mediasoup/pull/863)).

### 3.10.1

- `RtpStreamSend`: Memory optimizations ([PR #840](https://github.com/versatica/mediasoup/pull/840)). Extracted from #675, by @nazar-pc.
- `SimpleConsumer`: Opus DTX ignore capabilities ([PR #846](https://github.com/versatica/mediasoup/pull/846)).
- Update `libuv` to 1.44.1: Fixes `libuv` build ([PR #857](https://github.com/versatica/mediasoup/pull/857)).

### 3.10.0

- `WebRtcServer`: A new class that brings to `WebRtcTransports` the ability to listen on a single UDP/TCP port ([PR #834](https://github.com/versatica/mediasoup/pull/834)).
- More SRTP crypto suites ([PR #837](https://github.com/versatica/mediasoup/pull/837)).
- Improve `EnhancedEventEmitter` ([PR #836](https://github.com/versatica/mediasoup/pull/836)).
- `TransportCongestionControlClient`: Allow setting max outgoing bitrate before `tccClient` is created ([PR #833](https://github.com/versatica/mediasoup/pull/833)).
- Update TypeScript version.

### 3.9.17

- `RateCalculator`: Fix old buffer items cleanup ([PR #830](https://github.com/versatica/mediasoup/pull/830) by @dsdolzhenko).
- Update TypeScript version.

### 3.9.16

- `SimulcastConsumer`: Fix spatial layer switch with unordered packets ([PR #823](https://github.com/versatica/mediasoup/pull/823) by @jcague).
- Update TypeScript version.

### 3.9.15

- `RateCalculator`: Revert Fix old buffer items cleanup ([PR #819](https://github.com/versatica/mediasoup/pull/819) by @dsdolzhenko).

### 3.9.14

- `NackGenerator`: Add a configurable delay before sending NACK ([PR #827](https://github.com/versatica/mediasoup/pull/827), credits to @penguinol).
- `SimulcastConsumer`: Fix a race condition in SimulcastConsumer ([PR #825](https://github.com/versatica/mediasoup/pull/825) by @dsdolzhenko).
- Add support for H264 SVC (#798 by @prtmD).
- `RtpStreamSend`: Support receive RTCP-XR RRT and send RTCP-XR DLRR ([PR #781](https://github.com/versatica/mediasoup/pull/781) by @aggresss).
- `RateCalculator`: Fix old buffer items cleanup ([PR #819](https://github.com/versatica/mediasoup/pull/819) by @dsdolzhenko).
- `DirectTransport`: Create a buffer to process RTP packets ([PR #730](https://github.com/versatica/mediasoup/pull/730) by @rtctt).
- Node: Improve `appData` TypeScript syntax and initialization.
- Allow setting max outgoing bitrate below the initial value ([PR #826](https://github.com/versatica/mediasoup/pull/826) by @ggarber).
- Update TypeScript version.

### 3.9.13

- `VP8`: Do not discard `TL0PICIDX` from Temporal Layers higher than 0 (PR @817 by @jcague).
- Update TypeScript version.

### 3.9.12

- `DtlsTransport`: Make DTLS negotiation run immediately ([PR #815](https://github.com/versatica/mediasoup/pull/815)).
- Update TypeScript version.

### 3.9.11

- Modify `SimulcastConsumer` to keep using layers without good scores ([PR #804](https://github.com/versatica/mediasoup/pull/804) by @ggarber).

### 3.9.10

- Update worker dependencies:
  - OpenSSL 3.0.2.
  - abseil-cpp 20211102.0.
  - nlohmann_json 3.10.5.
  - usrsctp snapshot 4e06feb01cadcd127d119486b98a4bd3d64aa1e7.
  - wingetopt 1.00.
- Update TypeScript version.
- Fix RTP marker bit not being reseted after mangling in each `Consumer` ([PR #811](https://github.com/versatica/mediasoup/pull/811) by @ggarber).

### 3.9.9

- Optimize RTP header extension handling ([PR #786](https://github.com/versatica/mediasoup/pull/786)).
- `RateCalculator`: Reset optimization ([PR #785](https://github.com/versatica/mediasoup/pull/785)).
- Fix frozen video due to double call to `Consumer::UserOnTransportDisconnected()` ([PR #788](https://github.com/versatica/mediasoup/pull/788), thanks to @ggarber for exposing this issue in [PR #787](https://github.com/versatica/mediasoup/pull/787)).

### 3.9.8

- Fix VP9 kSVC forwarding logic to not forward lower unneded layers ([PR #778](https://github.com/versatica/mediasoup/pull/778) by @ggarber).
- Fix update bandwidth estimation configuration and available bitrate when updating max outgoing bitrate ([PR #779](https://github.com/versatica/mediasoup/pull/779) by @ggarber).
- Replace outdated `random-numbers` package by native `crypto.randomInt()` ([PR #776](https://github.com/versatica/mediasoup/pull/776) by @piranna).
- Update TypeScript version.

### 3.9.7

- Typing event emitters in mediasoup Node ([PR #764](https://github.com/versatica/mediasoup/pull/764) by @unao).

### 3.9.6

- TCC client optimizations for faster and more stable BWE ([PR #712](https://github.com/versatica/mediasoup/pull/712) by @ggarber).
- Added support for RTP abs-capture-time header ([PR #761](https://github.com/versatica/mediasoup/pull/761) by @oto313).

### 3.9.5

- ICE renomination support ([PR #756](https://github.com/versatica/mediasoup/pull/756)).
- Update `libuv` to 1.43.0.

### 3.9.4

- Worker: Fix bad printing of error messages from Worker ([PR #750](https://github.com/versatica/mediasoup/pull/750) by @j1elo).

### 3.9.3

- Single H264/H265 codec configuration in `supportedRtpCapabilities` ([PR #718](https://github.com/versatica/mediasoup/pull/718)).
- Improve Windows support by not requiring MSVC configuration ([PR #741](https://github.com/versatica/mediasoup/pull/741)).

### 3.9.2

- `pipeToRouter()`: Reuse same `PipeTransport` when possible ([PR #697](https://github.com/versatica/mediasoup/pull/697)).
- Add `worker.died` boolean getter.
- Update TypeScript version to 4.X.X and use `target: "esnext"` so transpilation of ECMAScript private fields (`#xxxxx`) don't use `WeakMaps` tricks but use standard syntax instead.
- Use more than one core for compilation on Windows ([PR #709](https://github.com/versatica/mediasoup/pull/709)).
- `Consumer`: Modification of bitrate allocation algorithm ([PR #708](https://github.com/versatica/mediasoup/pull/708)).

### 3.9.1

- NixOS friendly build process ([PR #683](https://github.com/versatica/mediasoup/pull/683)).
- Worker: Emit "died" event before observer "close" ([PR #684](https://github.com/versatica/mediasoup/pull/684)).
- Transport: Hide debug message for RTX RTCP-RR packets ([PR #688](https://github.com/versatica/mediasoup/pull/688)).
- Update `libuv` to 1.42.0.
- Improve Windows support ([PR #692](https://github.com/versatica/mediasoup/pull/692)).
- Avoid build commands when MEDIASOUP_WORKER_BIN is set ([PR #695](https://github.com/versatica/mediasoup/pull/695)).

### 3.9.0

- Replaces GYP build system with fully-functional Meson build system ([PR #622](https://github.com/versatica/mediasoup/pull/622)).
- Worker communication optimization (aka removing netstring dependency) ([PR #644](https://github.com/versatica/mediasoup/pull/644)).
- Move TypeScript and compiled JavaScript code to a new `node` folder.
- Use ES6 private fields.
- Require Node.js version >= 12.

### 3.8.4

- OPUS multi-channel (Surround sound) support ([PR #647](https://github.com/versatica/mediasoup/pull/647)).
- Add `packetLoss` stats to transport ([PR #648](https://github.com/versatica/mediasoup/pull/648) by @ggarber).
- Fixes for active speaker observer ([PR #655](https://github.com/versatica/mediasoup/pull/655) by @ggarber).
- Fix big endian issues ([PR #639](https://github.com/versatica/mediasoup/pull/639)).

### 3.8.3

- Fix wrong `size_t*` to `int*` conversion in 64bit Big-Endian hosts ([PR #637](https://github.com/versatica/mediasoup/pull/637)).

### 3.8.2

- `ActiveSpeakerObserver`: Fix crash due to a `nullptr` ([PR #634](https://github.com/versatica/mediasoup/pull/634)).

### 3.8.1

- `SimulcastConsumer`: Fix RTP timestamp when switching layers ([PR #626](https://github.com/versatica/mediasoup/pull/626) by @penguinol).

### 3.8.0

- Update `libuv` to 1.42.0.
- Use non-ASM OpenSSL on Windows ([PR #614](https://github.com/versatica/mediasoup/pull/614)).
- Fix minor memory leak caused by non-virtual destructor ([PR #625](https://github.com/versatica/mediasoup/pull/625)).
- Dominant Speaker Event ([PR #603](https://github.com/versatica/mediasoup/pull/603) by @SteveMcFarlin).

### 3.7.19

- Update `libuv` to 1.41.0.
- C++:
  - Move header includes ([PR #608](https://github.com/versatica/mediasoup/pull/608)).
  - Enhance debugging on channel request/notification error ([PR #607](https://github.com/versatica/mediasoup/pull/607)).

### 3.7.18

- Support for optional fixed port on transports ([PR #593](https://github.com/versatica/mediasoup/pull/593) by @nazar-pc).
- Upgrade and optimize OpenSSL dependency ([PR #598](https://github.com/versatica/mediasoup/pull/598) by @vpalmisano):
  - OpenSSL upgraded to version 1.1.1k.
  - Enable the compilation of assembly extensions for OpenSSL.
  - Optimize the worker build (`-O3`) and disable the debug flag (`-g`).

### 3.7.17

- Introduce `PipeConsumerOptions` to avoid incorrect type information on `PipeTransport.consume()` arguments.
- Make `ConsumerOptions.rtpCapabilities` field required as it should have always been.

### 3.7.16

- Add `mid` option in `ConsumerOptions` to provide way to override MID ([PR #586](https://github.com/versatica/mediasoup/pull/586) by @mstyura).

### 3.7.15

- `kind` field of `RtpHeaderExtension` is no longer optional. It must be 'audio' or 'video'.
- Refactor API inconsistency in internal RTP Observer communication with worker.

### 3.7.14

- Update `usrsctp` to include a "possible use after free bug" fix (commit [here](https://github.com/sctplab/usrsctp/commit/0f8d58300b1fdcd943b4a9dd3fbd830825390d4d)).

### 3.7.13

- Fix build on FreeBSD ([PR #585](https://github.com/versatica/mediasoup/pull/585) by @smortex).

### 3.7.12

- `mediasoup-worker`: Fix memory leaks on error exit ([PR #581](https://github.com/versatica/mediasoup/pull/581)).

### 3.7.11

- Fix `DepUsrSCTP::Checker::timer` not being freed on `Worker` close ([PR #576](https://github.com/versatica/mediasoup/pull/576)). Thanks @nazar-pc for discovering this.

### 3.7.10

- Remove clang tools binaries from regular installation.

### 3.7.9

- Code clean up.

### 3.7.8

- `PayloadChannel`: Copy received messages into a separate buffer to avoid memory corruption if the message is later modified ([PR #570](https://github.com/versatica/mediasoup/pull/570) by @aggresss).

### 3.7.7

- Thread and memory safety fixes needed for mediasoup-rust ([PR #562](https://github.com/versatica/mediasoup/pull/562) by @nazar-pc).
- mediasoup-rust support on macOS ([PR #567](https://github.com/versatica/mediasoup/pull/567) by @nazar-pc).
- mediasoup-rust release 0.7.2.

### 3.7.6

- `Transport`: Implement new `setMaxOutgoingBitrate()` method ([PR #555](https://github.com/versatica/mediasoup/pull/555) by @t-mullen).
- `SctpAssociation`: Don't warn if SCTP send buffer is full.
- Rust: Update modules structure and other minor improvements for Rust version ([PR #558](https://github.com/versatica/mediasoup/pull/558)).
- `mediasoup-worker`: Avoid duplicated basenames so that `libmediasoup-worker` is compilable on macOS ([PR #557](https://github.com/versatica/mediasoup/pull/557)).

### 3.7.5

- SctpAssociation: provide 'sctpsendbufferfull' reason on send error (#552).

### 3.7.4

- Improve `RateCalculator` ([PR #547](https://github.com/versatica/mediasoup/pull/547) by @vpalmisano).

### 3.7.3

- Make worker M1 compilable.

### 3.7.2

- `RateCalculator` optimization ([PR #538](https://github.com/versatica/mediasoup/pull/538) by @vpalmisano).

### 3.7.1

- `SimulcastConsumer`: Fix miscalculation when increasing layer ([PR #541](https://github.com/versatica/mediasoup/pull/541) by @penguinol).
- Rust version with thread-based worker ([PR #540](https://github.com/versatica/mediasoup/pull/540)).

### 3.7.0

- Welcome to `mediasoup-rust`! Authored by @nazar-pc (PRs #518 and #533).
- Update `usrsctp`.

### 3.6.37

- Fix crash if empty `fingerprints` array is given in `webrtcTransport.connect()` (issue #537).

### 3.6.36

- `Producer`: Add new stats field 'rtxPacketsDiscarded' ([PR #536](https://github.com/versatica/mediasoup/pull/536)).

### 3.6.35

- `XxxxConsumer.hpp`: make `IsActive()` return `true` (even if `Producer`'s score is 0) when DTX is enabled ([PR #534](https://github.com/versatica/mediasoup/pull/534) due to issue #532).

### 3.6.34

- Fix crash (regression, issue #529).

### 3.6.33

- Add missing `delete cb` that otherwise would leak ([PR #527](https://github.com/versatica/mediasoup/pull/527) based on [PR #526](https://github.com/versatica/mediasoup/pull/526) by @vpalmisano).
- `router.pipeToRouter()`: Fix possible inconsistency in `pipeProducer.paused` status (as discussed in this [thread](https://mediasoup.discourse.group/t/concurrency-architecture/2515/) in the mediasoup forum).
- Update `nlohmann/json` to 3.9.1.
- Update `usrsctp`.
- Enhance Jitter calculation.

### 3.6.32

- Fix notifications from `mediasoup-worker` being processed before responses received before them (issue #501).

### 3.6.31

- Move `bufferedAmount` from `dataConsumer.dump()` to `dataConsumer.getStats()`.

### 3.6.30

- Add `pipe` option to `transport.consume()`([PR #494](https://github.com/versatica/mediasoup/pull/494)).
  - So the receiver will get all streams from the `Producer`.
  - It works for any kind of transport (but `PipeTransport` which is always like this).
- Add `LICENSE` and `PATENTS` files in `libwebrtc` dependency (issue #495).
- Added `worker/src/Utils/README_BASE64_UTILS` (issue #497).
- Update `usrsctp`.

### 3.6.29

- Fix wrong message about `rtcMinPort` and `rtcMaxPort`.
- Update deps.
- Improve `EnhancedEventEmitter.safeAsPromise()` (although not used).

### 3.6.28

- Fix replacement of `__MEDIASOUP_VERSION__` in `lib/index.d.ts` (issue #483).
- `worker/scripts/configure.py`: Handle 'mips64' ([PR #485](https://github.com/versatica/mediasoup/pull/485)).

### 3.6.27

- Allow the `mediasoup-worker` process to inherit all environment variables (issue #480).

### 3.6.26

- BWE tweaks and debug logs.

### 3.6.25

- SCTP fixes ([PR #479](https://github.com/versatica/mediasoup/pull/479)).

### 3.6.24

- Update `awaitqueue` dependency.

### 3.6.23

- Fix yet another memory leak in Node.js layer due to `PayloadChannel` event listener not being removed.

### 3.6.22

- `Transport.cpp`: Provide transport congestion client with RTCP Receiver Reports (#464).
- Update `libuv` to 1.40.0.
- Update Node deps.
- `SctpAssociation.cpp`: increase `sctpBufferedAmount` before sending any data (#472).

### 3.6.21

- Fix memory leak in Node.js layer due to `PayloadChannel` event listener not being removed (related to #463).

### 3.6.20

- Remove `-fwrapv` when building `mediasoup-worker` in `Debug` mode (issue #460).
- Add `MEDIASOUP_MAX_CORES` to limit `NUM_CORES` during `mediasoup-worker` build ([PR #462](https://github.com/versatica/mediasoup/pull/462)).

### 3.6.19

- Update `usrsctp` dependency.
- Update `typescript-eslint` deps.
- Update Node deps.

### 3.6.18

- Fix `ortc.getConsumerRtpParameters()` RTX codec comparison issue ([PR #453](https://github.com/versatica/mediasoup/pull/453)).
- RtpObserver: expose `RtpObserverAddRemoveProducerOptions` for `addProducer()` and `removeProducer()` methods.

### 3.6.17

- Update `libuv` to 1.39.0.
- Update Node deps.
- SimulcastConsumer: Prefer the highest spatial layer initially ([PR #450](https://github.com/versatica/mediasoup/pull/450)).
- RtpStreamRecv: Set RtpDataCounter window size to 6 secs if DTX (#451)

### 3.6.16

- `SctpAssociation.cpp`: Fix `OnSctpAssociationBufferedAmount()` call.
- Update deps.
- New API to send data from Node throught SCTP DataConsumer.

### 3.6.15

- Avoid SRTP leak by deleting invalid SSRCs after STRP decryption (issue #437, thanks to @penguinol for reporting).
- Update `usrsctp` dep.
- DataConsumer 'bufferedAmount' implementation ([PR #442](https://github.com/versatica/mediasoup/pull/442)).

### 3.6.14

- Fix `usrsctp` vulnerability ([PR #439](https://github.com/versatica/mediasoup/pull/439)).
- Fix issue #435 (thanks to @penguinol for reporting).
- `TransportCongestionControlClient.cpp`: Enable periodic ALR probing to recover faster from network issues.

- Update `nlohmann::json` C++ dep to 3.9.0.

### 3.6.13

- RTP on `DirectTransport` (issue #433, [PR #434](https://github.com/versatica/mediasoup/pull/434)):
  - New API `producer.send(rtpPacket: Buffer)`.
  - New API `consumer.on('rtp', (rtpPacket: Buffer)`.
  - New API `directTransport.sendRtcp(rtcpPacket: Buffer)`.
  - New API `directTransport.on('rtcp', (rtpPacket: Buffer)`.

### 3.6.12

- Release script.

### 3.6.11

- `Transport`: rename `maxSctpSendBufferSize` to `sctpSendBufferSize`.

### 3.6.10

- `Transport`: Implement `maxSctpSendBufferSize`.
- Update `libuv` to 1.38.1.

### 3.6.9

- `Transport::ReceiveRtpPacket()`: Call `RecvStreamClosed(packet->GetSsrc())` if received RTP packet does not match any `Producer`.
- `Transport::HandleRtcpPacket()`: Ensure `Consumer` is found for received NACK Feedback packets.
- Fix issue #408.

### 3.6.8

- Fix SRTP leak due to streams not being removed when a `Producer` or `Consumer` is closed.
  - [PR #428](https://github.com/versatica/mediasoup/pull/428) (fixes issues #426).
  - Credits to credits to @penguinol for reporting and initial work at [PR #427](https://github.com/versatica/mediasoup/pull/427).
- Update `nlohmann::json` C++ dep to 3.8.0.
- C++: Enhance `const` correctness.

### 3.6.7

- `ConsumerScore`: Add `producerScores`, scores of all RTP streams in the producer ordered by encoding (just useful when the producer uses simulcast).
  - [PR #421](https://github.com/versatica/mediasoup/pull/421) (fixes issues #420).
- Hide worker executable console in Windows.
  - [PR #419](https://github.com/versatica/mediasoup/pull/419) (credits to @BlueMagnificent).
- `RtpStream.cpp`: Fix wrong `std::round()` usage.
  - Issue #423.

### 3.6.6

- Update `usrsctp` library.
- Update ESLint and TypeScript related dependencies.

### 3.6.5

- Set `score:0` when `dtx:true` is set in an `encoding` and there is no RTP for some seconds for that RTP stream.
  - Fixes #415.

### 3.6.4

- `gyp`: Fix CLT version detection in OSX Catalina when XCode app is not installed.
  - [PR #413](https://github.com/versatica/mediasoup/pull/413) (credits to @enimo).

### 3.6.3

- Modernize TypeScript.

### 3.6.2

- Fix crash in `Transport.ts` when closing a `DataConsumer` created on a `DirectTransport`.

### 3.6.1

- Export new `DirectTransport` in `types`.
- Make `DataProducerOptions` optional (not needed when in a `DirectTransport`).

### 3.6.0

- SCTP/DataChannel termination:
  - [PR #409](https://github.com/versatica/mediasoup/pull/409)
  - Allow the Node application to directly send text/binary messages to `mediasoup-worker` C++ process so others can consume them using `DataConsumers`.
  - And vice-versa: allow the Node application to directly consume in Node messages send by `DataProducers`.
- Add `WorkerLogTag` TypeScript enum and also add a new 'message' tag into it.

### 3.5.15

- Simulcast and SVC: Better computation of desired bitrate based on `maxBitrate` field in the `producer.rtpParameters.encodings`.

### 3.5.14

- Update deps, specially `uuid` and `@types/uuid` that had a TypeScript related bug.
- `TransportCongestionClient.cpp`: Improve sender side bandwidth estimation by do not reporting `this->initialAvailableBitrate` as available bitrate due to strange behavior in the algorithm.

### 3.5.13

- Simplify `GetDesiredBitrate()` in `SimulcastConsumer` and `SvcConsumer`.
- Update `libuv` to 1.38.0.

### 3.5.12

- `SeqManager.cpp`: Improve performance.
  - [PR #398](https://github.com/versatica/mediasoup/pull/398) (credits to @penguinol).

### 3.5.11

- `SeqManager.cpp`: Fix a bug and improve performance.
  - Fixes issue #395 via [PR #396](https://github.com/versatica/mediasoup/pull/396) (credits to @penguinol).
- Drop Node.js 8 support. Minimum supported Node.js version is now 10.
- Upgrade `eslint` and `jest` major versions.

### 3.5.10

- `SimulcastConsumer.cpp`: Fix `IncreaseLayer()` method (fixes #394).
- Udpate Node deps.

### 3.5.9

- `libwebrtc`: Apply patch by @sspanak and @Ivaka to avoid crash. Related issue: #357.
- `PortManager.cpp`: Do not use `UV_UDP_RECVMMSG` in Windows due to a bug in `libuv` 1.37.0.
- Update Node deps.

### 3.5.8

- Enable `UV_UDP_RECVMMSG`:
  - Upgrade `libuv` to 1.37.0.
  - Use `uv_udp_init_ex()` with `UV_UDP_RECVMMSG` flag.
  - Add our own `uv.gyp` now that `libuv` has removed support for GYP (fixes #384).

### 3.5.7

- Fix crash in `mediasoup-worker` due to conversion from `uint64_t` to `int64_t` (used within `libwebrtc` code. Fixes #357.
- Update `usrsctp` library.
- Update Node deps.

### 3.5.6

- `SeqManager.cpp`: Fix video lag after a long time.
  - Fixes #372 (thanks @penguinol for reporting it and giving the solution).

### 3.5.5

- `UdpSocket.cpp`: Revert `uv__udp_recvmmsg()` usage since it notifies about received UDP packets in reverse order. Feature on hold until fixed.

### 3.5.4

- `Transport.cpp`: Enable transport congestion client for the first video Consumer, no matter it's uses simulcast, SVC or a single stream.
- Update `libuv` to 1.35.0.
- `UdpSocket.cpp`: Ensure the new libuv's `uv__udp_recvmmsg()` is used, which is more efficient.

### 3.5.3

- `PlainTransport`: Remove `multiSource` option. It was a hack nobody should use.

### 3.5.2

- Enable MID RTP extension in mediasoup to receivers direction (for consumers).
  - This **requires** mediasoup-client 3.5.2 to work.

### 3.5.1

- `PlainTransport`: Fix event name: 'rtcpTuple' => 'rtcptuple'.

### 3.5.0

- `PipeTransport`: Add support for SRTP and RTP retransmission (RTX + NACK). Useful when connecting two mediasoup servers running in different hosts via pipe transports.
- `PlainTransport`: Add support for SRTP.
- Rename `PlainRtpTransport` to `PlainTransport` everywhere (classes, methods, TypeScript types, etc). Keep previous names and mark them as DEPRECATED.
- Fix vulnarability in IPv6 parser.

### 3.4.13

- Update `uuid` dep to 7.0.X (new API).
- Fix crash due wrong array index in `PipeConsumer::FillJson()`.
  - Fixes #364

### 3.4.12

- TypeScript: generate `es2020` instead of `es6`.
- Update `usrsctp` library.
  - Fixes #362 (thanks @chvarlam for reporting it).

### 3.4.11

- `IceServer.cpp`: Reject received STUN Binding request with 487 if remote peer indicates ICE-CONTROLLED into it.

### 3.4.10

- `ProducerOptions`: Rename `keyFrameWaitTime` option to `keyFrameRequestDelay` and make it work as expected.

### 3.4.9

- Add `Utils::Json::IsPositiveInteger()` to not rely on `is_number_unsigned()` of json lib, which is unreliable due to its design.
- Avoid ES6 `export default` and always use named `export`.
- `router.pipeToRouter()`: Ensure a single `PipeTransport` pair is created between `router1` and `router2`.
  - Since the operation is async, it may happen that two simultaneous calls to `router1.pipeToRouter({ producerId: xxx, router: router2 })` would end up generating two pairs of `PipeTranports`. To prevent that, let's use an async queue.
- Add `keyFrameWaitTime` option to `ProducerOptions`.
- Update Node and C++ deps.

### 3.4.8

- `libsrtp.gyp`: Fix regression in mediasoup for Windows.
  - `libsrtp.gyp`: Modernize it based on the new `BUILD.gn` in Chromium.
  - `libsrtp.gyp`: Don't include "test" and other targets.
  - Assume `HAVE_INTTYPES_H`, `HAVE_INT8_T`, etc. in Windows.
  - Issue details: https://github.com/sctplab/usrsctp/issues/353
- `gyp` dependency: Add support for Microsoft Visual Studio 2019.
  - Modify our own `gyp` sources to fix the issue.
  - CL uploaded to GYP project with the fix.
  - Issue details: https://github.com/sctplab/usrsctp/issues/347

### 3.4.7

- `PortManager.cpp`: Do not limit the number of failed `bind()` attempts to 20 since it does not work well in scenarios that launch tons of `Workers` with same port range. Instead iterate all ports in the range given to the Worker.
- Do not copy `catch.hpp` into `test/include/` but make the GYP `mediasoup-worker-test` target include the corresponding folder in `deps/catch`.

### 3.4.6

- Update libsrtp to 2.3.0.
- Update ESLint and TypeScript deps.

### 3.4.5

- Update deps.
- Fix text in `./github/Bug_Report.md` so it no longer references the deprecated mailing list.

### 3.4.4

- `Transport.cpp`: Ignore RTCP SDES packets (we don't do anything with them anyway).
- `Producer` and `Consumer` stats: Always show `roundTripTime` (even if calculated value is 0) after a `roundTripTime` > 0 has been seen.

### 3.4.3

- `Transport.cpp`: Fix RTCP FIR processing:
  - Instead of looking at the media ssrc in the common header, iterate FIR items and look for associated `Consumers` based on ssrcs in each FIR item.
  - Fixes #350 (thanks @j1elo for reporting and documenting the issue).

### 3.4.2

- `SctpAssociation.cpp`: Improve/fix logs.
- Improve Node `EventEmitter` events inline documentation.
- `test-node-sctp.js`: Wait for SCTP association to be open before sending data.

### 3.4.1

- Improve `mediasoup-worker` build system by using `sh` instead of `bash` and default to 4 cores (thanks @smoke, [PR #349](https://github.com/versatica/mediasoup/pull/349)).

### 3.4.0

- Add `worker.getResourceUsage()` API.
- Update OpenSSL to 1.1.1d.
- Update `libuv` to 1.34.0.
- Update TypeScript version.

### 3.3.8

- Update usrsctp dependency (it fixes a potential wrong memory access).
  - More details in the reported issue: https://github.com/sctplab/usrsctp/issues/408

### 3.3.7

- Fix `version` getter.

### 3.3.6

- `SctpAssociation.cpp`: Initialize the `usrsctp` socket in the class constructor. Fixes #348.

### 3.3.5

- Fix usage of a deallocated `RTC::TcpConnection` instance under heavy CPU usage due to mediasoup deleting the instance in the middle of a receiving iteration. Fixes #333.
  - More details in the commit: https://github.com/versatica/mediasoup/commit/49824baf102ab6d2b01e5bca565c29b8ac0fec22

### 3.3.4

- IPv6 fix: Use `INET6_ADDRSTRLEN` instead of `INET_ADDRSTRLEN`.

### 3.3.3

- Add `consumer.setPriority()` and `consumer.priority` API to prioritize how the estimated outgoing bitrate in a transport is distributed among all video consumers (in case there is not enough bitrate to satisfy them).
- Make video `SimpleConsumers` play the BWE game by helping in probation generation and bitrate distribution.
- Add `consumer.preferredLayers` getter.
- Rename `enablePacketEvent()` and "packet" event to `enableTraceEvent()` and "trace" event (sorry SEMVER).
- Transport: Add a new "trace" event of type "bwe" with detailed information about bitrates.

### 3.3.2

- Improve "packet" event by not firing both "keyframe" and "rtp" types for the same RTP packet.

### 3.3.1

- Add type "keyframe" as a valid type for "packet" event in `Producers` and `Consumers`.

### 3.3.0

- Add transport-cc bandwidth estimation and congestion control in sender and receiver side.
- Run in Windows.
- Rewrite to TypeScript.
- Tons of improvements.

### 3.2.5

- Fix TCP leak (#325).

### 3.2.4

- `PlainRtpTransport`: Fix comedia mode.

### 3.2.3

- `RateCalculator`: improve efficiency in `GetRate()` method (#324).

### 3.2.2

- `RtpDataCounter`: use window size of 2500 ms instead of 1000 ms.
  - Fixes false "lack of RTP" detection in some screen sharing usages with simulcast.
  - Fixes #312.

### 3.2.1

- Add RTCP Extended Reports for RTT calculation on receiver RTP stream (thanks @yangjinechofor for initial pull request #314).
- Make `mediasoup-worker` compile in Armbian Debian Buster (thanks @krishisola, fixes #321).

### 3.2.0

- Add DataChannel support via DataProducers and DataConsumers (#10).
- SRTP: Add support for AEAD GCM (#320).

### 3.1.7

- `PipeConsumer.cpp`: Fix RTCP generation (thanks @vpalmisano).

### 3.1.6

- VP8 and H264: Fix regression in 3.1.5 that produces lot of changes in current temporal layer detection.

### 3.1.5

- VP8 and H264: Allow packets without temporal layer information even if N temporal layers were announced.

### 3.1.4

- Add `-fPIC` in `cflags` to compile in x86-64. Fixes #315.

### 3.1.3

- Set the sender SSRC on PLI and FIR requests [related thread](https://mediasoup.discourse.group/t/broadcasting-a-vp8-rtp-stream-from-gstreamer/93).

### 3.1.2

- Workaround to detect H264 key frames when Chrome uses external encoder (related [issue](https://bugs.chromium.org/p/webrtc/issues/detail?id=10746)). Fixes #313.

### 3.1.1

- Improve `GetBitratePriority()` method in `SimulcastConsumer` and `SvcConsumer` by checking the total bitrate of all temporal layers in a given producer stream or spatial layer.

### 3.1.0

- Add SVC support. It includes VP9 full SVC and VP9 K-SVC as implemented by libwebrtc.
- Prefer Python 2 (if available) over Python 3. This is because there are yet pending issues with gyp + Python 3.

### 3.0.12

- Do not require Python 2 to compile mediasoup worker (#207). Both Python 2 and 3 can now be used.

### 3.0.11

- Codecs: Improve temporal layer switching in VP8 and H264.
- Skip worker compilation if `MEDIASOUP_WORKER_BIN` environment variable is given (#309). This makes it possible to install mediasoup in platforms in which, somehow, gcc > 4.8 is not available during `npm install mediasoup` but it's available later.
- Fix `RtpStreamRecv::TransmissionCounter::GetBitrate()`.

### 3.0.10

- `parseScalabilityMode()`: allow "S" as spatial layer (and not just "L"). "L" means "dependent spatial layer" while "S" means "independent spatial layer", which is used in K-SVC (VP9, AV1, etc).

### 3.0.9

- `RtpStreamSend::ReceiveRtcpReceiverReport()`: improve `rtt` calculation if no Sender Report info is reported in received Received Report.
- Update `libuv` to version 1.29.1.

### 3.0.8

- VP8 & H264: Improve temporal layer switching.

### 3.0.7

- RTP frame-marking: Add some missing checks.

### 3.0.6

- Fix regression in proxied RTP header extensions.

### 3.0.5

- Add support for frame-marking RTP extensions and use it to enable temporal layers switching in H264 codec (#305).

### 3.0.4

- Improve RTP probation for simulcast/svc consumers by using proper RTP retransmission with increasing sequence number.

### 3.0.3

- Simulcast: Improve timestamps extra offset handling by having a map of extra offsets indexed by received timestamps. This helps in case of packet retransmission.

### 3.0.2

- Simulcast: proper RTP stream switching by rewriting packet timestamp with a new timestamp calculated from the SenderReports' NTP relationship.

### 3.0.1

- Fix crash in `SimulcastConsumer::IncreaseLayer()` with Safari and H264 (#300).

### 3.0.0

- v3 is here!

### 2.6.19

- `RtpStreamSend.cpp`: Fix a crash in `StorePacket()` when it receives an old packet and there is no space left in the storage buffer (thanks to zkfun for reporting it and providing us with the solution).
- Update deps.

### 2.6.18

- Fix usage of a deallocated `RTC::TcpConnection` instance under heavy CPU usage due to mediasoup deleting the instance in the middle of a receiving iteration.

### 2.6.17

- Improve build system by using all available CPU cores in parallel.

### 2.6.16

- Don't mandate server port range to be >= 99.

### 2.6.15

- Fix NACK retransmissions.

### 2.6.14

- Fix TCP leak (#325).

### 2.6.13

- Make `mediasoup-worker` compile in Armbian Debian Buster (thanks @krishisola, fixes #321).
- Update deps.

### 2.6.12

- Fix RTCP Receiver Report handling.

### 2.6.11

- Update deps.
- Simulcast: Increase profiles one by one unless explicitly forced (fixes #188).

### 2.6.10

- `PlainRtpTransport.js`: Add missing methods and events.

### 2.6.9

- Remove a potential crash if a single `encoding` is given in the Producer `rtpParameters` and it has a `profile` value.

### 2.6.8

- C++: Verify in libuv static callbacks that the associated C++ instance has not been deallocated (thanks @artushin and @mariat-atg for reporting and providing valuable help in #258).

### 2.6.7

- Fix wrong destruction of Transports in Router.cpp that generates 100% CPU usage in `mediasoup-worker` processes.

### 2.6.6

- Fix a port leak when a WebRtcTransport is remotely closed due to a DTLS close alert (thanks @artushin for reporting it in #259).

### 2.6.5

- RtpPacket: Fix Two-Byte header extensions parsing.

### 2.6.4

- Upgrade again to OpenSSL 1.1.0j (20 Nov 2018) after adding a workaround for issue [#257](https://github.com/versatica/mediasoup/issues/257).

### 2.6.3

- Downgrade OpenSSL to version 1.1.0h (27 Mar 2018) until issue [#257](https://github.com/versatica/mediasoup/issues/257) is fixed.

### 2.6.2

- C++: Remove all `Destroy()` class methods and no longer do `delete this`.
- Update libuv to 1.24.1.
- Update OpenSSL to 1.1.0g.

### 2.6.1

- worker: Internal refactor and code cleanup.
- Remove announced support for certain RTCP feedback types that mediasoup does nothing with (and avoid forwarding them to the remote RTP sender).
- fuzzer: fix some wrong memory access in `RtpPacket::Dump()` and `StunMessage::Dump()` (just used during development).

### 2.6.0

- Integrate [libFuzzer](http://llvm.org/docs/LibFuzzer.html) into mediasoup (documentation in the `doc` folder). Extensive testing done. Several heap-buffer-overflow and memory leaks fixed.

### 2.5.6

- `Producer.cpp`: Remove `UpdateRtpParameters()`. It was broken since Consumers
  were not notified about profile removed and so on, so they may crash.
- `Producer.cpp: Remove some maps and simplify streams handling by having a
single `mapSsrcRtpStreamInfo`. Just keep `mapActiveProfiles`because`GetActiveProfiles()` method needs it.
- `Producer::MayNeedNewStream()`: Ignore new media streams with new SSRC if
  its RID is already in use by other media stream (fixes #235).
- Fix a bad memory access when using two byte RTP header extensions.

### 2.5.5

- `Server.js`: If a worker crashes make sure `_latestWorkerIdx` becomes 0.

### 2.5.4

- `server.Room()`: Assign workers incrementally or explicitly via new `workerIdx` argument.
- Add `server.numWorkers` getter.

### 2.5.3

- Don't announce `muxId` nor RTP MID extension support in `Consumer` RTP parameters.

### 2.5.2

- Enable RTP MID extension again.

### 2.5.1

- Disable RTP MID extension until [#230](https://github.com/versatica/mediasoup/issues/230) is fixed.

### 2.5.0

- Add RTP MID extension support.

### 2.4.6

- Do not close `Transport` on ICE disconnected (as it would prevent ICE restart on "recv" TCP transports).

### 2.4.5

- Improve codec matching.

### 2.4.4

- Fix audio codec matching when `channels` parameter is not given.

### 2.4.3

- Make `PlainRtpTransport` not leak if port allocation fails (related issue [#224](https://github.com/versatica/mediasoup/issues/224)).

### 2.4.2

- Fix a crash in when no more RTP ports were available (see related issue [#222](https://github.com/versatica/mediasoup/issues/222)).

### 2.4.1

- Update dependencies.

### 2.4.0

- Allow non WebRTC peers to create plain RTP transports (no ICE/DTLS/SRTP but just plain RTP and RTCP) for sending and receiving media.

### 2.3.3

- Fix C++ syntax to avoid an error when building the worker with clang 8.0.0 (OSX 10.11.6).

### 2.3.2

- `Channel.js`: Upgrade `REQUEST_TIMEOUT` to 20 seconds to avoid timeout errors when the Node or worker thread usage is too high (related to this [issue](https://github.com/versatica/mediasoup-client/issues/48)).

### 2.3.1

- H264: Check if there is room for the indicated NAL unit size (thanks @ggarber).
- H264: Code cleanup.

### 2.3.0

- Add new "spy" feature. A "spy" peer cannot produce media and is invisible for other peers in the room.

### 2.2.7

- Fix H264 simulcast by properly detecting when the profile switching should be done.
- Fix a crash in `Consumer::GetStats()` (see related issue [#196](https://github.com/versatica/mediasoup/issues/196)).

### 2.2.6

- Add H264 simulcast capability.

### 2.2.5

- Avoid calling deprecated (NOOP) `SSL_CTX_set_ecdh_auto()` function in OpenSSL >= 1.1.0.

### 2.2.4

- [Fix #4](https://github.com/versatica/mediasoup/issues/4): Avoid DTLS handshake fragmentation.

### 2.2.3

- [Fix #196](https://github.com/versatica/mediasoup/issues/196): Crash in `Consumer::getStats()` due to wrong `targetProfile`.

### 2.2.2

- Improve [issue #209](https://github.com/versatica/mediasoup/issues/209).

### 2.2.1

- [Fix #209](https://github.com/versatica/mediasoup/issues/209): `DtlsTransport`: don't crash when signaled fingerprint and DTLS fingerprint do not match (thanks @yangjinecho for reporting it).

### 2.2.0

- Update Node and C/C++ dependencies.

### 2.1.0

- Add `localIP` option for `room.createRtpStreamer()` and `transport.startMirroring()` [[PR #199](https://github.com/versatica/mediasoup/pull/199)](https://github.com/versatica/mediasoup/pull/199).

### 2.0.16

- Improve C++ usage (remove "warning: missing initializer for member" [-Wmissing-field-initializers]).
- Update Travis-CI settings.

### 2.0.15

- Make `PlainRtpTransport` also send RTCP SR/RR reports (thanks @artushin for reporting).

### 2.0.14

- [Fix #193](https://github.com/versatica/mediasoup/issues/193): `preferTcp` not honored (thanks @artushin).

### 2.0.13

- Avoid crash when no remote IP/port is given.

### 2.0.12

- Add `handled` and `unhandled` events to `Consumer`.

### 2.0.11

- [Fix #185](https://github.com/versatica/mediasoup/issues/185): Consumer: initialize effective profile to 'NONE' (thanks @artushin).
- [Fix #186](https://github.com/versatica/mediasoup/issues/186): NackGenerator code being executed after instance deletion (thanks @baiyufei).

### 2.0.10

- [Fix #183](https://github.com/versatica/mediasoup/issues/183): Always reset the effective `Consumer` profile when removed (thanks @thehappycoder).

### 2.0.9

- Make ICE+DTLS more flexible by allowing sending DTLS handshake when ICE is just connected.

### 2.0.8

- Disable stats periodic retrieval also on remote closure of `Producer` and `WebRtcTransport`.

### 2.0.7

- [Fix #180](https://github.com/versatica/mediasoup/issues/180): Added missing include `cmath` so that `std::round` can be used (thanks @jacobEAdamson).

### 2.0.6

- [Fix #173](https://github.com/versatica/mediasoup/issues/173): Avoid buffer overflow in `()` (thanks @lightmare).
- Improve stream layers management in `Consumer` by using the new `RtpMonitor` class.

### 2.0.5

- [Fix #164](https://github.com/versatica/mediasoup/issues/164): Sometimes video freezes forever (no RTP received in browser at all).
- [Fix #160](https://github.com/versatica/mediasoup/issues/160): Assert error in `RTC::Consumer::GetStats()`.

### 2.0.4

- [Fix #159](https://github.com/versatica/mediasoup/issues/159): Don’t rely on VP8 payload descriptor flags to assure the existence of data.
- [Fix #160](https://github.com/versatica/mediasoup/issues/160): Reset `targetProfile` when the corresponding profile is removed.

### 2.0.3

- worker: Fix crash when VP8 payload has no `PictureId`.

### 2.0.2

- worker: Remove wrong `assert` on `Producer::DeactivateStreamProfiles()`.

### 2.0.1

- Update README file.

### 2.0.0

- New design based on `Producers` and `Consumer` plus a mediasoup protocol and the **mediasoup-client** client side SDK.

### 1.2.8

- Fix a crash due to RTX packet processing while the associated `NackGenerator` is not yet created.

### 1.2.7

- Habemus RTX ([RFC 4588](https://tools.ietf.org/html/rfc4588)) for proper RTP retransmission.

### 1.2.6

- Fix an issue in `buffer.toString()` that makes mediasoup fail in Node 8.
- Update libuv to version 1.12.0.

### 1.2.5

- Add support for [ICE renomination](https://tools.ietf.org/html/draft-thatcher-ice-renomination).

### 1.2.4

- Fix a SDP negotiation issue when the remote peer does not have compatible codecs.

### 1.2.3

- Add video codecs supported by Microsoft Edge.

### 1.2.2

- `RtpReceiver`: generate RTCP PLI when "rtpraw" or "rtpobject" event listener is set.

### 1.2.1

- `RtpReceiver`: fix an error producing packets when "rtpobject" event is set.

### 1.2.0

- `RtpSender`: allow `disable()`/`enable()` without forcing SDP renegotiation (#114).

### 1.1.0

- Add `Room.on('audiolevels')` event.

### 1.0.2

- Set a maximum value of 1500 bytes for packet storage in `RtpStreamSend`.

### 1.0.1

- Avoid possible segfault if `RemoteBitrateEstimator` generates a bandwidth estimation with zero SSRCs.

### 1.0.0

- First stable release.
