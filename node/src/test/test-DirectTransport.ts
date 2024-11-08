import * as mediasoup from '../';
import { enhancedOnce } from '../enhancedEvents';
import { DirectTransportEvents } from '../DirectTransportTypes';
import { WorkerEvents } from '../types';

type TestContext = {
	worker?: mediasoup.types.Worker;
	router?: mediasoup.types.Router;
};

const ctx: TestContext = {};

beforeEach(async () => {
	ctx.worker = await mediasoup.createWorker();
	ctx.router = await ctx.worker.createRouter();
});

afterEach(async () => {
	ctx.worker?.close();

	if (ctx.worker?.subprocessClosed === false) {
		await enhancedOnce<WorkerEvents>(ctx.worker, 'subprocessclose');
	}
});

test('router.createDirectTransport() succeeds', async () => {
	const onObserverNewTransport = jest.fn();

	ctx.router!.observer.once('newtransport', onObserverNewTransport);

	const directTransport = await ctx.router!.createDirectTransport({
		maxMessageSize: 1024,
		appData: { foo: 'bar' },
	});

	await expect(ctx.router!.dump()).resolves.toMatchObject({
		transportIds: [directTransport.id],
	});

	expect(onObserverNewTransport).toHaveBeenCalledTimes(1);
	expect(onObserverNewTransport).toHaveBeenCalledWith(directTransport);
	expect(typeof directTransport.id).toBe('string');
	expect(directTransport.closed).toBe(false);
	expect(directTransport.appData).toEqual({ foo: 'bar' });

	const dump = await directTransport.dump();

	expect(dump.id).toBe(directTransport.id);
	expect(dump.direct).toBe(true);
	expect(dump.producerIds).toEqual([]);
	expect(dump.consumerIds).toEqual([]);
	expect(dump.dataProducerIds).toEqual([]);
	expect(dump.dataConsumerIds).toEqual([]);
	expect(dump.recvRtpHeaderExtensions).toBeDefined();
	expect(typeof dump.rtpListener).toBe('object');

	directTransport.close();
	expect(directTransport.closed).toBe(true);
}, 2000);

test('router.createDirectTransport() with wrong arguments rejects with TypeError', async () => {
	await expect(
		// @ts-expect-error --- Testing purposes.
		ctx.router!.createDirectTransport({ maxMessageSize: 'foo' })
	).rejects.toThrow(TypeError);

	await expect(
		ctx.router!.createDirectTransport({ maxMessageSize: -2000 })
	).rejects.toThrow(TypeError);
}, 2000);

test('directTransport.getStats() succeeds', async () => {
	const directTransport = await ctx.router!.createDirectTransport();

	const stats = await directTransport.getStats();

	expect(Array.isArray(stats)).toBe(true);
	expect(stats.length).toBe(1);
	expect(stats[0].type).toBe('direct-transport');
	expect(stats[0].transportId).toBe(directTransport.id);
	expect(typeof stats[0].timestamp).toBe('number');
	expect(stats[0].bytesReceived).toBe(0);
	expect(stats[0].recvBitrate).toBe(0);
	expect(stats[0].bytesSent).toBe(0);
	expect(stats[0].sendBitrate).toBe(0);
	expect(stats[0].rtpBytesReceived).toBe(0);
	expect(stats[0].rtpRecvBitrate).toBe(0);
	expect(stats[0].rtpBytesSent).toBe(0);
	expect(stats[0].rtpSendBitrate).toBe(0);
	expect(stats[0].rtxBytesReceived).toBe(0);
	expect(stats[0].rtxRecvBitrate).toBe(0);
	expect(stats[0].rtxBytesSent).toBe(0);
	expect(stats[0].rtxSendBitrate).toBe(0);
	expect(stats[0].probationBytesSent).toBe(0);
	expect(stats[0].probationSendBitrate).toBe(0);
}, 2000);

test('directTransport.connect() succeeds', async () => {
	const directTransport = await ctx.router!.createDirectTransport();

	await expect(directTransport.connect()).resolves.toBeUndefined();
}, 2000);

test('dataProducer.send() succeeds', async () => {
	const directTransport = await ctx.router!.createDirectTransport();
	const dataProducer = await directTransport.produceData({
		label: 'foo',
		protocol: 'bar',
		appData: { foo: 'bar' },
	});
	const dataConsumer = await directTransport.consumeData({
		dataProducerId: dataProducer.id,
	});
	const numMessages = 200;
	const pauseSendingAtMessage = 10;
	const resumeSendingAtMessage = 20;
	const pauseReceivingAtMessage = 40;
	const resumeReceivingAtMessage = 60;
	const expectedReceivedNumMessages =
		numMessages -
		(resumeSendingAtMessage - pauseSendingAtMessage) -
		(resumeReceivingAtMessage - pauseReceivingAtMessage);

	let sentMessageBytes = 0;
	let effectivelySentMessageBytes = 0;
	let recvMessageBytes = 0;
	let numSentMessages = 0;
	let numReceivedMessages = 0;

	async function sendNextMessage(): Promise<void> {
		const id = ++numSentMessages;
		let message: Buffer | string;

		if (id === pauseSendingAtMessage) {
			await dataProducer.pause();
		} else if (id === resumeSendingAtMessage) {
			await dataProducer.resume();
		} else if (id === pauseReceivingAtMessage) {
			await dataConsumer.pause();
		} else if (id === resumeReceivingAtMessage) {
			await dataConsumer.resume();
		}

		// Send string (WebRTC DataChannel string).
		if (id < numMessages / 2) {
			message = String(id);
		}
		// Send string (WebRTC DataChannel binary).
		else {
			message = Buffer.from(String(id));
		}

		dataProducer.send(message);

		const messageSize = Buffer.from(message).byteLength;

		sentMessageBytes += messageSize;

		if (!dataProducer.paused && !dataConsumer.paused) {
			effectivelySentMessageBytes += messageSize;
		}

		if (id < numMessages) {
			void sendNextMessage();
		}
	}

	await new Promise<void>((resolve, reject) => {
		dataProducer.on('listenererror', (eventName, error) => {
			reject(
				new Error(
					`dataProducer 'listenererror' [eventName:${eventName}]: ${error.toString()}`
				)
			);
		});

		dataConsumer.on('listenererror', (eventName, error) => {
			reject(
				new Error(
					`dataConsumer 'listenererror' [eventName:${eventName}]: ${error.toString()}`
				)
			);
		});

		dataConsumer.on('message', (message, ppid) => {
			++numReceivedMessages;

			// message is always a Buffer.
			recvMessageBytes += message.byteLength;

			const id = Number(message.toString('utf8'));

			if (id === numMessages) {
				resolve();
			}
			// PPID of WebRTC DataChannel string.
			else if (id < numMessages / 2 && ppid !== 51) {
				reject(
					new Error(
						`ppid in message with id ${id} should be 51 but it is ${ppid}`
					)
				);
			}
			// PPID of WebRTC DataChannel binary.
			else if (id > numMessages / 2 && ppid !== 53) {
				reject(
					new Error(
						`ppid in message with id ${id} should be 53 but it is ${ppid}`
					)
				);
			}
		});

		void sendNextMessage();
	});

	expect(numSentMessages).toBe(numMessages);
	expect(numReceivedMessages).toBe(expectedReceivedNumMessages);
	expect(recvMessageBytes).toBe(effectivelySentMessageBytes);

	await expect(dataProducer.getStats()).resolves.toMatchObject([
		{
			type: 'data-producer',
			label: dataProducer.label,
			protocol: dataProducer.protocol,
			messagesReceived: numMessages,
			bytesReceived: sentMessageBytes,
		},
	]);

	await expect(dataConsumer.getStats()).resolves.toMatchObject([
		{
			type: 'data-consumer',
			label: dataConsumer.label,
			protocol: dataConsumer.protocol,
			messagesSent: expectedReceivedNumMessages,
			bytesSent: recvMessageBytes,
		},
	]);
}, 5000);

test('dataProducer.send() with subchannels succeeds', async () => {
	const directTransport = await ctx.router!.createDirectTransport();
	const dataProducer = await directTransport.produceData();
	const dataConsumer1 = await directTransport.consumeData({
		dataProducerId: dataProducer.id,
		subchannels: [1, 11, 666],
	});
	const dataConsumer2 = await directTransport.consumeData({
		dataProducerId: dataProducer.id,
		subchannels: [2, 22, 666],
	});
	const expectedReceivedNumMessages1 = 7;
	const expectedReceivedNumMessages2 = 5;
	const receivedMessages1: string[] = [];
	const receivedMessages2: string[] = [];

	await new Promise<void>(resolve => {
		// Must be received by dataConsumer1 and dataConsumer2.
		dataProducer.send(
			'both',
			/* ppid */ undefined,
			/* subchannels */ undefined,
			/* requiredSubchannel */ undefined
		);

		// Must be received by dataConsumer1 and dataConsumer2.
		dataProducer.send(
			'both',
			/* ppid */ undefined,
			/* subchannels */ [1, 2],
			/* requiredSubchannel */ undefined
		);

		// Must be received by dataConsumer1 and dataConsumer2.
		dataProducer.send(
			'both',
			/* ppid */ undefined,
			/* subchannels */ [11, 22, 33],
			/* requiredSubchannel */ 666
		);

		// Must not be received by neither dataConsumer1 nor dataConsumer2.
		dataProducer.send(
			'none',
			/* ppid */ undefined,
			/* subchannels */ [3],
			/* requiredSubchannel */ 666
		);

		// Must not be received by neither dataConsumer1 nor dataConsumer2.
		dataProducer.send(
			'none',
			/* ppid */ undefined,
			/* subchannels */ [666],
			/* requiredSubchannel */ 3
		);

		// Must be received by dataConsumer1.
		dataProducer.send(
			'dc1',
			/* ppid */ undefined,
			/* subchannels */ [1],
			/* requiredSubchannel */ undefined
		);

		// Must be received by dataConsumer1.
		dataProducer.send(
			'dc1',
			/* ppid */ undefined,
			/* subchannels */ [11],
			/* requiredSubchannel */ 1
		);

		// Must be received by dataConsumer1.
		dataProducer.send(
			'dc1',
			/* ppid */ undefined,
			/* subchannels */ [666],
			/* requiredSubchannel */ 11
		);

		// Must be received by dataConsumer2.
		dataProducer.send(
			'dc2',
			/* ppid */ undefined,
			/* subchannels */ [666],
			/* requiredSubchannel */ 2
		);

		// Make dataConsumer2 also subscribe to subchannel 1.
		// NOTE: No need to await for this call.
		void dataConsumer2.setSubchannels([...dataConsumer2.subchannels, 1]);

		// Must be received by dataConsumer1 and dataConsumer2.
		dataProducer.send(
			'both',
			/* ppid */ undefined,
			/* subchannels */ [1],
			/* requiredSubchannel */ 666
		);

		dataConsumer1.on('message', message => {
			receivedMessages1.push(message.toString('utf8'));

			if (
				receivedMessages1.length === expectedReceivedNumMessages1 &&
				receivedMessages2.length === expectedReceivedNumMessages2
			) {
				resolve();
			}
		});

		dataConsumer2.on('message', message => {
			receivedMessages2.push(message.toString('utf8'));

			if (
				receivedMessages1.length === expectedReceivedNumMessages1 &&
				receivedMessages2.length === expectedReceivedNumMessages2
			) {
				resolve();
			}
		});
	});

	expect(receivedMessages1.length).toBe(expectedReceivedNumMessages1);
	expect(receivedMessages2.length).toBe(expectedReceivedNumMessages2);

	for (const message of receivedMessages1) {
		expect(['both', 'dc1'].includes(message)).toBe(true);
		expect(['dc2'].includes(message)).toBe(false);
	}

	for (const message of receivedMessages2) {
		expect(['both', 'dc2'].includes(message)).toBe(true);
		expect(['dc1'].includes(message)).toBe(false);
	}
}, 5000);

test('DirectTransport methods reject if closed', async () => {
	const directTransport = await ctx.router!.createDirectTransport();
	const onObserverClose = jest.fn();

	directTransport.observer.once('close', onObserverClose);
	directTransport.close();

	expect(onObserverClose).toHaveBeenCalledTimes(1);
	expect(directTransport.closed).toBe(true);

	await expect(directTransport.dump()).rejects.toThrow(Error);

	await expect(directTransport.getStats()).rejects.toThrow(Error);
}, 2000);

test('DirectTransport emits "routerclose" if Router is closed', async () => {
	const directTransport = await ctx.router!.createDirectTransport();
	const onObserverClose = jest.fn();

	directTransport.observer.once('close', onObserverClose);

	const promise = enhancedOnce<DirectTransportEvents>(
		directTransport,
		'routerclose'
	);

	ctx.router!.close();
	await promise;

	expect(onObserverClose).toHaveBeenCalledTimes(1);
	expect(directTransport.closed).toBe(true);
}, 2000);

test('DirectTransport emits "routerclose" if Worker is closed', async () => {
	const directTransport = await ctx.router!.createDirectTransport();
	const onObserverClose = jest.fn();

	directTransport.observer.once('close', onObserverClose);

	const promise = enhancedOnce<DirectTransportEvents>(
		directTransport,
		'routerclose'
	);

	ctx.worker!.close();
	await promise;

	expect(onObserverClose).toHaveBeenCalledTimes(1);
	expect(directTransport.closed).toBe(true);
}, 2000);
