# Closures

Some considerations:

- Any JS `xxxxx.yyyyyClosed()` method is equivalent to the corresponding C++ `~Xxxxx()` destructor. They both silently destroy things without generating any internal notifications/events.
  - _NOTE:_ Yes, the JS `xxxxx.yyyyyClosed()` produces **public** JS event `xxxxx.on('yyyyyclose')`, but that's not internal stuff.

## JS worker.close()

- Public API.
- Kills the mediasoup-worker process (if not already died) via signal.
- Iterates all JS Routers and calls `router.workerClosed()`.

## mediasoup-worker process dies unexpectely

- The JS Worker emits public JS `worker.on('died')`.
- Iterates all JS Routers and calls `router.workerClosed()`.

## C++ Worker::Close()

- Called when the mediasoup-worker process is killed.
- Iterates all C++ Routers and calls `delete router`.

## JS router.workerClosed()

- Private API.
- Emits public JS `router.on('workerclose')`.

## JS router.close()

- Public API.
- Sends channel request `WORKER_CLOSE_ROUTER`:
  - Processed by the C++ Worker.
  - It removes the C++ Router from its map.
  - It calls C++ `delete router`.
- Iterates all JS Transports and calls `transport.routerClosed()`.
- Emits private JS `router.on('@close')` (so the JS Worker cleans its map).

## C++ ~Router() destructor

- Iterates all C++ Transports and calls `delete transport`.

## JS transport.routerClosed()

- Private API.
- Iterates all JS Producers and calls `producer.transportClosed()`.
- Iterates all JS Consumers and calls `consumer.transportClosed()`.
- Emits public JS `transport.on('routerclose')`.

## JS transport.close()

- Public API.
- Sends channel request `ROUTER_CLOSE_TRANSPORT`.
  - Processed by the C++ Router.
  - It calls C++ `transport->Close()` (so the C++ Transport will notify the C++ Router about closed Producers and Consumers in that Transport).
  - It removes the C++ Transport from its map.
  - It calls C++ `delete transport`.
- Iterates all JS Producers and calls `producer.transportClosed()`.
- For each JS Producer, the JS Transport emits private JS `transport.on('@producerclose')` (so the JS Router cleans its maps).
- Iterates all JS Consumers and calls `consumer.transportClosed()`.
- Emits private JS `transport.on('@close')` (so the JS Router cleans its map).

## C++ ~Transport() destructor

- Iterates all C++ Producers and calls `delete producer`.
- Iterates all C++ Consumer and calls `delete consumer`.

## C++ Transport::Close()

- Iterates all C++ Producers. For each Producer:
  - Removes it from its map of Producers.
  - Calls its `listener->OnTransportProducerClosed(this, producer)` (so the C++ Router cleans its maps and calls `consumer->ProducerClosed()` on its associated Consumers).
  - Calls `delete producer`.
- It clears its map of C++ Producers.
- Iterates all C++ Consumer. For each Consumer:
  - Removes it from its map of Consumers.
  - Call its `listener->OnTransportConsumerClosed(this, consumer)` (so the C++ Router cleans its maps).
  - Calls `delete consumer`.
- It clears its map of C++ Consumers.

_NOTE:_ If a Transport holds a Producer and a Consumer associated to that Producer, ugly things may happen when calling `Transport::Close()`:

- While iterating the C++ Producers as above, the C++ Consumer would be deleted (via `Consumer::ProducerClosed()`).
  - As far as it's properly removed from the `Transport::mapConsumers` everything would be ok when later iterating the map of Consumers.
- Must ensure that, in this scenario, the JS event `consumer.on('producerclose')` is not called since `consumer.on('transportclose')` is supposed to happen before.
  - This won't happen since the JS `Consumer` has removed its channel notifications within its `transportClosed()` method.

## C++ Router::OnTransportProducerClosed(transport, producer)

- Gets the set of C++ Consumers associated to the closed Producer in its `mapProducerConsumers`. For each Consumer:
  - Calls `consumer->ProducerClosed()`.
- Deletes the entry in `mapProducerConsumers` with key `producer`.
- Deletes the entry in `mapProducers` with key `producer->id`.

## C++ Router::OnTransportConsumerClosed(transport, consumer)

- Get the associated C++ Producer from `mapConsumerProducer`.
- Remove the closed C++ Consumer from the set of Consumers in the corresponding `mapProducerConsumers` entry for the given Producer.
- Deletes the entry in `mapConsumerProducer` with key `consumer`.

## JS producer.transportClosed()

- Private API.
- Emits public JS `producer.on('transportclose')`.

## JS producer.close()

- Public API.
- Sends channel request `TRANSPORT_CLOSE_PRODUCER`.
  - Processed by the C++ Transport.
  - Removes it from its map of Producers.
  - Calls its `listener->OnTransportProducerClosed(this, producer)` (so the C++ Router cleans its maps and calls `consumer->ProducerClose()` on its associated Consumers).
  - Calls `delete producer`.
- Emits private JS `producer.on('@close')` (so the JS Transport cleans its map and will also emit private JS `transport.on('@producerclose')` so the JS Router cleans its map).

## C++ ~Producer() destructor

- Destroys its stuff.

## JS consumer.transportClosed()

- Private API.
- Emits public JS `consumer.on('transportclose')`.

## JS consumer.close()

- Public API.
- Sends channel request `TRANSPORT_CLOSE_CONSUMER`.
  - Processed by the C++ Transport.
  - Removes it from its map of Consumers.
  - Calls its `listener->OnTransportConsumerClosed(this, consumer)` (so the C++ Router cleans its maps).
  - Calls `delete consumer`.
- Emits private JS `consumer.on('@close')` (so the JS Transport cleans its map).

## C++ ~Consumer() destructor

- Destroys its stuff.

## C++ Consumer::ProducerClosed()

- Called from the C++ Router within the `Router::OnTransportProducerClosed()` listener.
- Send a channel notification `producerclose` to the JS Consumer.
  - The JS Consumer emits private JS `consumer.on('@produceclose')` (so the JS Transport cleans its map).
  - The JS Consumer emits public JS `consumer.on('produceclose')`.
- Notifies its C++ Transport via `listener->onConsumerProducerClosed()` which:
  - cleans its map of Consumers,
  - notifies the Router via `listener->OnTransportConsumerClosed()`), and
  - deletes the Consumer.
