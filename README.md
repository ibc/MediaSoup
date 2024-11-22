# mediasoup v3

[![][mediasoup-banner]][mediasoup-website]

[![][npm-shield-mediasoup]][npm-mediasoup]
[![][crates-shield-mediasoup]][crates-mediasoup]
[![][github-actions-shield-mediasoup-node]][github-actions-mediasoup-node]
[![][github-actions-shield-mediasoup-worker]][github-actions-mediasoup-worker]
[![][github-actions-shield-mediasoup-rust]][github-actions-mediasoup-rust]
[![][codeql-shield-mediasoup]][codeql-mediasoup]
[![][opencollective-shield-mediasoup]][opencollective-mediasoup]

## Website and Documentation

- [mediasoup.org][mediasoup-website]

## Support Forum

- [mediasoup.discourse.group][mediasoup-discourse]

## Design Goals

mediasoup and its client side libraries are designed to accomplish with the following goals:

- Be a [SFU](https://webrtcglossary.com/sfu) (Selective Forwarding Unit).
- Support both WebRTC and plain RTP input and output.
- Be a Node.js module or Rust crate in server side.
- Be a tiny TypeScript and C++ libraries in client side.
- Be minimalist: just handle the media layer.
- Be signaling agnostic: do not mandate any signaling protocol.
- Be super low level API.
- Support all existing WebRTC endpoints.
- Enable integration with well known multimedia libraries/tools.

## Architecture

![][mediasoup-architecture]

## Use Cases

mediasoup and its client side libraries provide a super low level API. They are intended to enable different use cases and scenarios, without any constraint or assumption. Some of these use cases are:

- Group video chat applications.
- One-to-many (or few-to-many) broadcasting applications in real-time.
- RTP streaming.

## Features

- ECMAScript 6/Idiomatic Rust low level API.
- Multi-stream: multiple audio/video streams over a single ICE + DTLS transport.
- IPv6 ready.
- ICE / DTLS / RTP / RTCP over UDP and TCP.
- Simulcast and SVC support.
- Congestion control.
- Sender and receiver bandwidth estimation with spatial/temporal layers distribution algorithm.
- Data message exchange (via WebRTC DataChannels, SCTP over plain UDP, and direct termination in Node.js/Rust).
- Extremely powerful (media worker thread/subprocess coded in C++ on top of [libuv](https://libuv.org)).

## Demo Online

[![][mediasoup-demo-screenshot]][mediasoup-demo]

Try it at [v3demo.mediasoup.org](https://v3demo.mediasoup.org) ([source code](https://github.com/versatica/mediasoup-demo)).

## Authors

- Iñaki Baz Castillo [[website](https://inakibaz.me)|[github](https://github.com/ibc/)]
- José Luis Millán [[github](https://github.com/jmillan/)]
- Nazar Mokynskyi [[github](https://github.com/nazar-pc/)]

## Social

- Twitter: [@mediasoup_sfu](https://twitter.com/mediasoup_sfu)

## Sponsor

You can support mediasoup by [sponsoring][sponsor] it. Thanks!

## License

[ISC](./LICENSE)

[mediasoup-banner]: /art/mediasoup-banner.png
[mediasoup-website]: https://mediasoup.org
[mediasoup-discourse]: https://mediasoup.discourse.group
[npm-shield-mediasoup]: https://img.shields.io/npm/v/mediasoup.svg
[npm-mediasoup]: https://npmjs.org/package/mediasoup
[crates-shield-mediasoup]: https://img.shields.io/crates/v/mediasoup.svg
[crates-mediasoup]: https://crates.io/crates/mediasoup
[github-actions-shield-mediasoup-node]: https://github.com/versatica/mediasoup/actions/workflows/mediasoup-node.yaml/badge.svg
[github-actions-mediasoup-node]: https://github.com/versatica/mediasoup/actions/workflows/mediasoup-node.yaml
[github-actions-shield-mediasoup-worker]: https://github.com/versatica/mediasoup/actions/workflows/mediasoup-worker.yaml/badge.svg
[github-actions-mediasoup-worker]: https://github.com/versatica/mediasoup/actions/workflows/mediasoup-worker.yaml
[github-actions-shield-mediasoup-rust]: https://github.com/versatica/mediasoup/actions/workflows/mediasoup-rust.yaml/badge.svg
[github-actions-mediasoup-rust]: https://github.com/versatica/mediasoup/actions/workflows/mediasoup-rust.yaml
[codeql-shield-mediasoup]: https://github.com/versatica/mediasoup/actions/workflows/codeql.yml/badge.svg
[codeql-mediasoup]: https://github.com/versatica/mediasoup/actions/workflows/codeql.yml
[opencollective-shield-mediasoup]: https://img.shields.io/opencollective/all/mediasoup.svg
[opencollective-mediasoup]: https://opencollective.com/mediasoup/
[sponsor]: https://mediasoup.org/sponsor/
[mediasoup-architecture]: /art/mediasoup-v3-architecture-01.svg
[mediasoup-demo-screenshot]: /art/mediasoup-v3.png
[mediasoup-demo]: https://v3demo.mediasoup.org
