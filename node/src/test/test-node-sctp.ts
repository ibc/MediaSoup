import * as dgram from 'node:dgram';
// @ts-expect-error -- sctp library doesn't have TS types.
import * as sctp from 'sctp';
import * as mediasoup from '../';
import { enhancedOnce } from '../enhancedEvents';
import { WorkerEvents } from '../types';

type TestContext = {
	worker?: mediasoup.types.Worker;
	router?: mediasoup.types.Router;
	plainTransport?: mediasoup.types.PlainTransport;
	dataProducer?: mediasoup.types.DataProducer;
	dataConsumer?: mediasoup.types.DataConsumer;
	udpSocket?: dgram.Socket;
	sctpSocket?: any;
	sctpSendStreamId?: number;
	sctpSendStream?: any;
};

const ctx: TestContext = {};

beforeEach(async () => {
	// Set node-sctp default PMTU to 1200.
	sctp.defaults({ PMTU: 1200 });

	ctx.worker = await mediasoup.createWorker({ disableLiburing: true });
	ctx.router = await ctx.worker.createRouter();
	ctx.plainTransport = await ctx.router.createPlainTransport({
		// https://github.com/nodejs/node/issues/14900.
		listenIp: '127.0.0.1',
		// So we don't need to call plainTransport.connect().
		comedia: true,
		enableSctp: true,
		numSctpStreams: { OS: 256, MIS: 256 },
	});

	// Node UDP socket for SCTP.
	ctx.udpSocket = dgram.createSocket({ type: 'udp4' });

	await new Promise<void>(resolve => {
		ctx.udpSocket!.bind(0, '127.0.0.1', resolve);
	});

	const remoteUdpIp = ctx.plainTransport.tuple.localAddress;
	const remoteUdpPort = ctx.plainTransport.tuple.localPort;
	const { OS, MIS } = ctx.plainTransport.sctpParameters!;

	await new Promise<void>((resolve, reject) => {
		ctx.udpSocket?.on('error', error => {
			reject(error);
		});

		ctx.udpSocket?.connect(remoteUdpPort, remoteUdpIp, () => {
			ctx.sctpSocket = sctp.connect({
				localPort: 5000, // Required for SCTP over UDP in mediasoup.
				port: 5000, // Required for SCTP over UDP in mediasoup.
				OS: OS,
				MIS: MIS,
				udpTransport: ctx.udpSocket,
			});

			resolve();
		});
	});

	// Wait for the SCTP association to be open.
	await Promise.race([
		enhancedOnce(ctx.sctpSocket, 'connect'),
		new Promise<void>((resolve, reject) =>
			setTimeout(() => reject(new Error('SCTP connection timeout')), 3000)
		),
	]);

	// Create an explicit SCTP outgoing stream with id 123 (id 0 is already used
	// by the implicit SCTP outgoing stream built-in the SCTP socket).
	ctx.sctpSendStreamId = 123;
	ctx.sctpSendStream = ctx.sctpSocket.createStream(ctx.sctpSendStreamId);

	// Create a DataProducer with the corresponding SCTP stream id.
	ctx.dataProducer = await ctx.plainTransport.produceData({
		sctpStreamParameters: {
			streamId: ctx.sctpSendStreamId,
			ordered: true,
		},
		label: 'node-sctp',
		protocol: 'foo & bar 😀😀😀',
	});

	// Create a DataConsumer to receive messages from the DataProducer over the
	// same plainTransport.
	ctx.dataConsumer = await ctx.plainTransport.consumeData({
		dataProducerId: ctx.dataProducer.id,
	});
});

afterEach(async () => {
	ctx.udpSocket?.close();
	ctx.sctpSocket?.end();
	ctx.worker?.close();

	if (ctx.worker?.subprocessClosed === false) {
		await enhancedOnce<WorkerEvents>(ctx.worker, 'subprocessclose');
	}

	// NOTE: For some reason we have to wait a bit for the SCTP stuff to release
	// internal things, otherwise Jest reports open handles. We don't care much
	// honestly.
	await new Promise(resolve => setTimeout(resolve, 1000));
});

test('ordered DataProducer delivers all SCTP messages to the DataConsumer', async () => {
	const onStream = jest.fn();
	const numMessages = 200;
	let sentMessageBytes = 0;
	let recvMessageBytes = 0;
	let numSentMessages = 0;
	let numReceivedMessages = 0;

	// It must be zero because it's the first DataConsumer on the plainTransport.
	expect(ctx.dataConsumer!.sctpStreamParameters?.streamId).toBe(0);

	await new Promise<void>((resolve, reject) => {
		sendNextMessage();

		function sendNextMessage(): void {
			const id = ++numSentMessages;
			const data = Buffer.from(String(id));

			// Set ppid of type WebRTC DataChannel string.
			if (id < numMessages / 2) {
				// @ts-expect-errors --- sctp library needs `ppid` field.`
				data.ppid = sctp.PPID.WEBRTC_STRING;
			}
			// Set ppid of type WebRTC DataChannel binary.
			else {
				// @ts-expect-errors --- sctp library needs `ppid` field.
				data.ppid = sctp.PPID.WEBRTC_BINARY;
			}

			ctx.sctpSendStream!.write(data);
			sentMessageBytes += data.byteLength;

			if (id < numMessages) {
				sendNextMessage();
			}
		}

		ctx.sctpSocket!.on('stream', onStream);

		// Handle the generated SCTP incoming stream and SCTP messages receives on it.
		// @ts-expect-error --- Custom event of sctp library.
		ctx.sctpSocket.on('stream', (stream, streamId) => {
			// It must be zero because it's the first SCTP incoming stream (so first
			// DataConsumer).
			if (streamId !== 0) {
				reject(new Error(`streamId should be 0 but it is ${streamId}`));

				return;
			}

			stream.on('data', (data: Buffer) => {
				++numReceivedMessages;
				recvMessageBytes += data.byteLength;

				const id = Number(data.toString('utf8'));
				// @ts-expect-errors --- sctp library uses `ppid` field.
				const ppid = data.ppid;

				if (id !== numReceivedMessages) {
					reject(
						new Error(
							`id ${id} in message should match numReceivedMessages ${numReceivedMessages}`
						)
					);
				} else if (id === numMessages) {
					resolve();
				} else if (id < numMessages / 2 && ppid !== sctp.PPID.WEBRTC_STRING) {
					reject(
						new Error(
							`ppid in message with id ${id} should be ${sctp.PPID.WEBRTC_STRING} but it is ${ppid}`
						)
					);
				} else if (id > numMessages / 2 && ppid !== sctp.PPID.WEBRTC_BINARY) {
					reject(
						new Error(
							`ppid in message with id ${id} should be ${sctp.PPID.WEBRTC_BINARY} but it is ${ppid}`
						)
					);

					return;
				}
			});
		});
	});

	expect(onStream).toHaveBeenCalledTimes(1);
	expect(numSentMessages).toBe(numMessages);
	expect(numReceivedMessages).toBe(numMessages);
	expect(recvMessageBytes).toBe(sentMessageBytes);

	await expect(ctx.dataProducer!.getStats()).resolves.toMatchObject([
		{
			type: 'data-producer',
			label: ctx.dataProducer!.label,
			protocol: ctx.dataProducer!.protocol,
			messagesReceived: numMessages,
			bytesReceived: sentMessageBytes,
		},
	]);

	await expect(ctx.dataConsumer!.getStats()).resolves.toMatchObject([
		{
			type: 'data-consumer',
			label: ctx.dataConsumer!.label,
			protocol: ctx.dataConsumer!.protocol,
			messagesSent: numMessages,
			bytesSent: recvMessageBytes,
		},
	]);
}, 10000);
