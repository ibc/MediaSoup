# TODO in mediasoup v2 (server-side)

* In `RtpStreamSend::StorePacket()` we should remove old packets (as `NackGenerator` does).

* Remove `producer.on('rtprawpacket')` and, instead, create a special `RtpConsumer` or something like that.

* Set a proper value for Producer::FullFrameRequestBlockTimeout (currently 1 second).

* Check `appData` everywhere.

* Remove DEPTH stuff.

* Rethink the auto close of the worker `Transport` when receiving a DTLS CLOSE ALERT (or when it's just TCP and the connection fails...). Yes/no? BTW it's generating some issues because the JS closes the Producer/Transport and gets `request failed [id:84026423, reason:"Consumer/Transport does not exist"]` (because it was already closed due to DTLS ALERT).

* Implement `mandatoryCodecPayloadTypes`.

* JS: Remove tons of useless debugs (for example in `Peer._createProducer()`).

* Producer JS API to request fullframe.
