import * as flatbuffers from 'flatbuffers';
import * as mediasoup from '../';
import { enhancedOnce } from '../enhancedEvents';
import { WorkerEvents, ProducerEvents } from '../types';
import { Producer } from '../Producer';
import { UnsupportedError } from '../errors';
import * as utils from '../utils';
import {
	Notification,
	Body as NotificationBody,
	Event,
} from '../fbs/notification';
import * as FbsProducer from '../fbs/producer';

type TestContext = {
	mediaCodecs: mediasoup.types.RtpCodecCapability[];
	audioProducerOptions: mediasoup.types.ProducerOptions;
	videoProducerOptions: mediasoup.types.ProducerOptions;
	worker?: mediasoup.types.WorkerInterface;
	router?: mediasoup.types.RouterInterface;
	webRtcTransport1?: mediasoup.types.WebRtcTransportInterface;
	webRtcTransport2?: mediasoup.types.WebRtcTransportInterface;
};

const ctx: TestContext = {
	mediaCodecs: utils.deepFreeze<mediasoup.types.RtpCodecCapability[]>([
		{
			kind: 'audio',
			mimeType: 'audio/opus',
			clockRate: 48000,
			channels: 2,
			parameters: {
				foo: '111',
			},
		},
		{
			kind: 'video',
			mimeType: 'video/VP8',
			clockRate: 90000,
		},
		{
			kind: 'video',
			mimeType: 'video/H264',
			clockRate: 90000,
			parameters: {
				'level-asymmetry-allowed': 1,
				'packetization-mode': 1,
				'profile-level-id': '4d0032',
				foo: 'bar',
			},
			rtcpFeedback: [], // Will be ignored.
		},
	]),
	audioProducerOptions: utils.deepFreeze<mediasoup.types.ProducerOptions>({
		kind: 'audio',
		rtpParameters: {
			mid: 'AUDIO',
			codecs: [
				{
					mimeType: 'audio/opus',
					payloadType: 0,
					clockRate: 48000,
					channels: 2,
					parameters: {
						useinbandfec: 1,
						usedtx: 1,
						foo: 222.222,
						bar: '333',
					},
				},
			],
			headerExtensions: [
				{
					uri: 'urn:ietf:params:rtp-hdrext:sdes:mid',
					id: 10,
				},
				{
					uri: 'urn:ietf:params:rtp-hdrext:ssrc-audio-level',
					id: 12,
				},
			],
			// Missing encodings on purpose.
			rtcp: {
				cname: 'audio-1',
			},
		},
		appData: { foo: 1, bar: '2' },
	}),
	videoProducerOptions: utils.deepFreeze<mediasoup.types.ProducerOptions>({
		kind: 'video',
		rtpParameters: {
			mid: 'VIDEO',
			codecs: [
				{
					mimeType: 'video/h264',
					payloadType: 112,
					clockRate: 90000,
					parameters: {
						'packetization-mode': 1,
						'profile-level-id': '4d0032',
					},
					rtcpFeedback: [
						{ type: 'nack' },
						{ type: 'nack', parameter: 'pli' },
						{ type: 'goog-remb' },
					],
				},
				{
					mimeType: 'video/rtx',
					payloadType: 113,
					clockRate: 90000,
					parameters: { apt: 112 },
				},
			],
			headerExtensions: [
				{
					uri: 'urn:ietf:params:rtp-hdrext:sdes:mid',
					id: 10,
				},
				{
					uri: 'urn:3gpp:video-orientation',
					id: 13,
				},
			],
			encodings: [
				{ ssrc: 22222222, rtx: { ssrc: 22222223 }, scalabilityMode: 'L1T3' },
				{ ssrc: 22222224, rtx: { ssrc: 22222225 } },
				{ ssrc: 22222226, rtx: { ssrc: 22222227 } },
				{ ssrc: 22222228, rtx: { ssrc: 22222229 } },
			],
			rtcp: {
				cname: 'video-1',
			},
		},
		appData: { foo: 1, bar: '2' },
	}),
};

beforeEach(async () => {
	ctx.worker = await mediasoup.createWorker();
	ctx.router = await ctx.worker.createRouter({ mediaCodecs: ctx.mediaCodecs });
	ctx.webRtcTransport1 = await ctx.router.createWebRtcTransport({
		listenIps: ['127.0.0.1'],
	});
	ctx.webRtcTransport2 = await ctx.router.createWebRtcTransport({
		listenIps: ['127.0.0.1'],
	});
});

afterEach(async () => {
	ctx.worker?.close();

	if (ctx.worker?.subprocessClosed === false) {
		await enhancedOnce<WorkerEvents>(ctx.worker, 'subprocessclose');
	}
});

test('webRtcTransport1.produce() succeeds', async () => {
	const onObserverNewProducer = jest.fn();

	ctx.webRtcTransport1!.observer.once('newproducer', onObserverNewProducer);

	const audioProducer = await ctx.webRtcTransport1!.produce(
		ctx.audioProducerOptions
	);

	expect(onObserverNewProducer).toHaveBeenCalledTimes(1);
	expect(onObserverNewProducer).toHaveBeenCalledWith(audioProducer);
	expect(typeof audioProducer.id).toBe('string');
	expect(audioProducer.closed).toBe(false);
	expect(audioProducer.kind).toBe('audio');
	expect(typeof audioProducer.rtpParameters).toBe('object');
	expect(audioProducer.type).toBe('simple');
	// Private API.
	expect(typeof audioProducer.consumableRtpParameters).toBe('object');
	expect(audioProducer.paused).toBe(false);
	expect(audioProducer.score).toEqual([]);
	expect(audioProducer.appData).toEqual({ foo: 1, bar: '2' });

	await expect(ctx.router!.dump()).resolves.toMatchObject({
		mapProducerIdConsumerIds: [{ key: audioProducer.id, values: [] }],
		mapConsumerIdProducerId: [],
	});

	await expect(ctx.webRtcTransport1!.dump()).resolves.toMatchObject({
		id: ctx.webRtcTransport1!.id,
		producerIds: [audioProducer.id],
		consumerIds: [],
	});
}, 2000);

test('webRtcTransport2.produce() succeeds', async () => {
	const onObserverNewProducer = jest.fn();

	ctx.webRtcTransport2!.observer.once('newproducer', onObserverNewProducer);

	const videoProducer = await ctx.webRtcTransport2!.produce(
		ctx.videoProducerOptions
	);

	expect(onObserverNewProducer).toHaveBeenCalledTimes(1);
	expect(onObserverNewProducer).toHaveBeenCalledWith(videoProducer);
	expect(typeof videoProducer.id).toBe('string');
	expect(videoProducer.closed).toBe(false);
	expect(videoProducer.kind).toBe('video');
	expect(typeof videoProducer.rtpParameters).toBe('object');
	expect(videoProducer.type).toBe('simulcast');
	// Private API.
	expect(typeof videoProducer.consumableRtpParameters).toBe('object');
	expect(videoProducer.paused).toBe(false);
	expect(videoProducer.score).toEqual([]);
	expect(videoProducer.appData).toEqual({ foo: 1, bar: '2' });

	const dump = await ctx.router!.dump();

	expect(dump.mapProducerIdConsumerIds).toEqual(
		expect.arrayContaining([{ key: videoProducer.id, values: [] }])
	);

	expect(dump.mapConsumerIdProducerId.length).toBe(0);

	await expect(ctx.webRtcTransport2!.dump()).resolves.toMatchObject({
		id: ctx.webRtcTransport2!.id,
		producerIds: [videoProducer.id],
		consumerIds: [],
	});
}, 2000);

test('webRtcTransport1.produce() without header extensions and rtcp succeeds', async () => {
	const onObserverNewProducer = jest.fn();

	ctx.webRtcTransport1!.observer.once('newproducer', onObserverNewProducer);

	const audioProducer = await ctx.webRtcTransport1!.produce({
		kind: 'audio',
		rtpParameters: {
			mid: 'AUDIO2',
			codecs: [
				{
					mimeType: 'audio/opus',
					payloadType: 0,
					clockRate: 48000,
					channels: 2,
					parameters: {
						useinbandfec: 1,
						usedtx: 1,
						foo: 222.222,
						bar: '333',
					},
				},
			],
		},
		appData: { foo: 1, bar: '2' },
	});

	expect(onObserverNewProducer).toHaveBeenCalledTimes(1);
	expect(onObserverNewProducer).toHaveBeenCalledWith(audioProducer!);
	expect(typeof audioProducer!.id).toBe('string');
	expect(audioProducer!.closed).toBe(false);
	expect(audioProducer!.kind).toBe('audio');
	expect(typeof audioProducer!.rtpParameters).toBe('object');
	expect(audioProducer!.type).toBe('simple');
	// Private API.
	expect(typeof audioProducer!.consumableRtpParameters).toBe('object');
	expect(audioProducer!.paused).toBe(false);
	expect(audioProducer!.score).toEqual([]);
	expect(audioProducer!.appData).toEqual({ foo: 1, bar: '2' });

	audioProducer.close();
}, 2000);

test('webRtcTransport1.produce() with wrong arguments rejects with TypeError', async () => {
	await expect(
		ctx.webRtcTransport1!.produce({
			// @ts-expect-error --- Testing purposes.
			kind: 'chicken',
			// @ts-expect-error --- Testing purposes.
			rtpParameters: {},
		})
	).rejects.toThrow(TypeError);

	await expect(
		ctx.webRtcTransport1!.produce({
			kind: 'audio',
			// @ts-expect-error --- Testing purposes.
			rtpParameters: {},
		})
	).rejects.toThrow(TypeError);

	// Invalid ssrc.
	await expect(
		ctx.webRtcTransport1!.produce({
			kind: 'audio',
			rtpParameters: {
				codecs: [],
				headerExtensions: [],
				// @ts-expect-error --- Testing purposes.
				encodings: [{ ssrc: '1111' }],
				rtcp: { cname: 'qwerty' },
			},
		})
	).rejects.toThrow(TypeError);

	// Missing or empty rtpParameters.encodings.
	await expect(
		ctx.webRtcTransport1!.produce({
			kind: 'video',
			rtpParameters: {
				codecs: [
					{
						mimeType: 'video/h264',
						payloadType: 112,
						clockRate: 90000,
						parameters: {
							'packetization-mode': 1,
							'profile-level-id': '4d0032',
						},
					},
					{
						mimeType: 'video/rtx',
						payloadType: 113,
						clockRate: 90000,
						parameters: { apt: 112 },
					},
				],
				headerExtensions: [],
				encodings: [],
				rtcp: { cname: 'qwerty' },
			},
		})
	).rejects.toThrow(TypeError);

	// Wrong apt in RTX codec.
	await expect(
		ctx.webRtcTransport1!.produce({
			kind: 'audio',
			rtpParameters: {
				codecs: [
					{
						mimeType: 'video/h264',
						payloadType: 112,
						clockRate: 90000,
						parameters: {
							'packetization-mode': 1,
							'profile-level-id': '4d0032',
						},
					},
					{
						mimeType: 'video/rtx',
						payloadType: 113,
						clockRate: 90000,
						parameters: { apt: 111 },
					},
				],
				headerExtensions: [],
				encodings: [{ ssrc: 6666, rtx: { ssrc: 6667 } }],
				rtcp: {
					cname: 'video-1',
				},
			},
		})
	).rejects.toThrow(TypeError);
}, 2000);

test('webRtcTransport1.produce() with unsupported codecs rejects with UnsupportedError', async () => {
	await expect(
		ctx.webRtcTransport1!.produce({
			kind: 'audio',
			rtpParameters: {
				codecs: [
					{
						mimeType: 'audio/ISAC',
						payloadType: 108,
						clockRate: 32000,
					},
				],
				headerExtensions: [],
				encodings: [{ ssrc: 1111 }],
				rtcp: { cname: 'audio' },
			},
		})
	).rejects.toThrow(UnsupportedError);

	// Invalid H264 profile-level-id.
	await expect(
		ctx.webRtcTransport1!.produce({
			kind: 'video',
			rtpParameters: {
				codecs: [
					{
						mimeType: 'video/h264',
						payloadType: 112,
						clockRate: 90000,
						parameters: {
							'packetization-mode': 1,
							'profile-level-id': 'CHICKEN',
						},
					},
					{
						mimeType: 'video/rtx',
						payloadType: 113,
						clockRate: 90000,
						parameters: { apt: 112 },
					},
				],
				headerExtensions: [],
				encodings: [{ ssrc: 6666, rtx: { ssrc: 6667 } }],
			},
		})
	).rejects.toThrow(UnsupportedError);
}, 2000);

test('transport.produce() with already used MID or SSRC rejects with Error', async () => {
	const audioProducerOptions: mediasoup.types.ProducerOptions = {
		kind: 'audio',
		rtpParameters: {
			mid: 'AUDIO',
			codecs: [
				{
					mimeType: 'audio/opus',
					payloadType: 0,
					clockRate: 48000,
					channels: 2,
				},
			],
			encodings: [{ ssrc: 33333333 }],
		},
	};

	const videoProducerOptions: mediasoup.types.ProducerOptions = {
		kind: 'video',
		rtpParameters: {
			mid: 'VIDEO2',
			codecs: [
				{
					mimeType: 'video/h264',
					payloadType: 112,
					clockRate: 90000,
					parameters: {
						'packetization-mode': 1,
						'profile-level-id': '4d0032',
					},
				},
			],
			headerExtensions: [
				{
					uri: 'urn:ietf:params:rtp-hdrext:sdes:mid',
					id: 10,
				},
			],
			encodings: [{ ssrc: 22222222 }],
			rtcp: {
				cname: 'video-1',
			},
		},
	};

	await ctx.webRtcTransport1!.produce(audioProducerOptions);

	await expect(
		ctx.webRtcTransport1!.produce(audioProducerOptions)
	).rejects.toThrow(Error);

	await ctx.webRtcTransport2!.produce(videoProducerOptions);

	await expect(
		ctx.webRtcTransport2!.produce(videoProducerOptions)
	).rejects.toThrow(Error);
}, 2000);

test('transport.produce() with no MID and with single encoding without RID or SSRC rejects with Error', async () => {
	await expect(
		ctx.webRtcTransport1!.produce({
			kind: 'audio',
			rtpParameters: {
				codecs: [
					{
						mimeType: 'audio/opus',
						payloadType: 111,
						clockRate: 48000,
						channels: 2,
					},
				],
				encodings: [{}],
			},
		})
	).rejects.toThrow(Error);
}, 2000);

test('producer.dump() succeeds', async () => {
	const audioProducer = await ctx.webRtcTransport1!.produce(
		ctx.audioProducerOptions
	);

	const dump1 = await audioProducer.dump();

	expect(dump1.id).toBe(audioProducer.id);
	expect(dump1.kind).toBe(audioProducer.kind);
	expect(typeof dump1.rtpParameters).toBe('object');
	expect(Array.isArray(dump1.rtpParameters.codecs)).toBe(true);
	expect(dump1.rtpParameters.codecs.length).toBe(1);
	expect(dump1.rtpParameters.codecs[0].mimeType).toBe('audio/opus');
	expect(dump1.rtpParameters.codecs[0].payloadType).toBe(0);
	expect(dump1.rtpParameters.codecs[0].clockRate).toBe(48000);
	expect(dump1.rtpParameters.codecs[0].channels).toBe(2);
	expect(dump1.rtpParameters.codecs[0].parameters).toEqual({
		useinbandfec: 1,
		usedtx: 1,
		foo: 222.222,
		bar: '333',
	});
	expect(dump1.rtpParameters.codecs[0].rtcpFeedback).toEqual([]);
	expect(Array.isArray(dump1.rtpParameters.headerExtensions)).toBe(true);
	expect(dump1.rtpParameters.headerExtensions!.length).toBe(2);
	expect(dump1.rtpParameters.headerExtensions).toEqual([
		{
			uri: 'urn:ietf:params:rtp-hdrext:sdes:mid',
			id: 10,
			parameters: {},
			encrypt: false,
		},
		{
			uri: 'urn:ietf:params:rtp-hdrext:ssrc-audio-level',
			id: 12,
			parameters: {},
			encrypt: false,
		},
	]);
	expect(Array.isArray(dump1.rtpParameters.encodings)).toBe(true);
	expect(dump1.rtpParameters.encodings!.length).toBe(1);
	expect(dump1.rtpParameters.encodings![0]).toEqual(
		expect.objectContaining({
			codecPayloadType: 0,
		})
	);
	expect(dump1.type).toBe('simple');

	const videoProducer = await ctx.webRtcTransport2!.produce(
		ctx.videoProducerOptions
	);

	const dump2 = await videoProducer.dump();

	expect(dump2.id).toBe(videoProducer.id);
	expect(dump2.kind).toBe(videoProducer.kind);
	expect(typeof dump2.rtpParameters).toBe('object');
	expect(Array.isArray(dump2.rtpParameters.codecs)).toBe(true);
	expect(dump2.rtpParameters.codecs.length).toBe(2);
	expect(dump2.rtpParameters.codecs[0].mimeType).toBe('video/H264');
	expect(dump2.rtpParameters.codecs[0].payloadType).toBe(112);
	expect(dump2.rtpParameters.codecs[0].clockRate).toBe(90000);
	expect(dump2.rtpParameters.codecs[0].channels).toBeUndefined();
	expect(dump2.rtpParameters.codecs[0].parameters).toEqual({
		'packetization-mode': 1,
		'profile-level-id': '4d0032',
	});
	expect(dump2.rtpParameters.codecs[0].rtcpFeedback).toEqual([
		{ type: 'nack' },
		{ type: 'nack', parameter: 'pli' },
		{ type: 'goog-remb' },
	]);
	expect(dump2.rtpParameters.codecs[1].mimeType).toBe('video/rtx');
	expect(dump2.rtpParameters.codecs[1].payloadType).toBe(113);
	expect(dump2.rtpParameters.codecs[1].clockRate).toBe(90000);
	expect(dump2.rtpParameters.codecs[1].channels).toBeUndefined();
	expect(dump2.rtpParameters.codecs[1].parameters).toEqual({ apt: 112 });
	expect(dump2.rtpParameters.codecs[1].rtcpFeedback).toEqual([]);
	expect(Array.isArray(dump2.rtpParameters.headerExtensions)).toBe(true);
	expect(dump2.rtpParameters.headerExtensions!.length).toBe(2);
	expect(dump2.rtpParameters.headerExtensions).toEqual([
		{
			uri: 'urn:ietf:params:rtp-hdrext:sdes:mid',
			id: 10,
			parameters: {},
			encrypt: false,
		},
		{
			uri: 'urn:3gpp:video-orientation',
			id: 13,
			parameters: {},
			encrypt: false,
		},
	]);
	expect(Array.isArray(dump2.rtpParameters.encodings)).toBe(true);
	expect(dump2.rtpParameters.encodings!.length).toBe(4);
	expect(dump2.rtpParameters.encodings).toMatchObject([
		{
			codecPayloadType: 112,
			ssrc: 22222222,
			rtx: { ssrc: 22222223 },
			scalabilityMode: 'L1T3',
		},
		{ codecPayloadType: 112, ssrc: 22222224, rtx: { ssrc: 22222225 } },
		{ codecPayloadType: 112, ssrc: 22222226, rtx: { ssrc: 22222227 } },
		{ codecPayloadType: 112, ssrc: 22222228, rtx: { ssrc: 22222229 } },
	]);
	expect(dump2.type).toBe('simulcast');
}, 2000);

test('producer.getStats() succeeds', async () => {
	const audioProducer = await ctx.webRtcTransport1!.produce(
		ctx.audioProducerOptions
	);

	const videoProducer = await ctx.webRtcTransport2!.produce(
		ctx.videoProducerOptions
	);

	await expect(audioProducer.getStats()).resolves.toEqual([]);

	await expect(videoProducer.getStats()).resolves.toEqual([]);
}, 2000);

test('producer.pause() and resume() succeed', async () => {
	const audioProducer = await ctx.webRtcTransport1!.produce(
		ctx.audioProducerOptions
	);

	const onObserverPause = jest.fn();
	const onObserverResume = jest.fn();

	audioProducer.observer.on('pause', onObserverPause);
	audioProducer.observer.on('resume', onObserverResume);

	await audioProducer.pause();
	expect(audioProducer.paused).toBe(true);

	await expect(audioProducer.dump()).resolves.toMatchObject({ paused: true });

	await audioProducer.resume();
	expect(audioProducer.paused).toBe(false);

	await expect(audioProducer.dump()).resolves.toMatchObject({ paused: false });

	// Even if we don't await for pause()/resume() completion, the observer must
	// fire 'pause' and 'resume' events if state was the opposite.
	void audioProducer.pause();
	void audioProducer.resume();
	void audioProducer.pause();
	void audioProducer.pause();
	void audioProducer.pause();
	await audioProducer.resume();

	expect(onObserverPause).toHaveBeenCalledTimes(3);
	expect(onObserverResume).toHaveBeenCalledTimes(3);
}, 2000);

test('producer.pause() and resume() emit events', async () => {
	const audioProducer = await ctx.webRtcTransport1!.produce(
		ctx.audioProducerOptions
	);

	const promises = [];
	const events: string[] = [];

	audioProducer.observer.once('resume', () => {
		events.push('resume');
	});

	audioProducer.observer.once('pause', () => {
		events.push('pause');
	});

	promises.push(audioProducer.pause());
	promises.push(audioProducer.resume());

	await Promise.all(promises);

	expect(events).toEqual(['pause', 'resume']);
	expect(audioProducer.paused).toBe(false);
}, 2000);

test('producer.enableTraceEvent() succeed', async () => {
	const audioProducer = await ctx.webRtcTransport1!.produce(
		ctx.audioProducerOptions
	);

	await audioProducer.enableTraceEvent(['rtp', 'pli']);

	const dump1 = await audioProducer.dump();

	expect(dump1.traceEventTypes).toEqual(expect.arrayContaining(['rtp', 'pli']));

	await audioProducer.enableTraceEvent();

	const dump2 = await audioProducer.dump();

	expect(dump2.traceEventTypes).toEqual(expect.arrayContaining([]));

	// @ts-expect-error --- Testing purposes.
	await audioProducer.enableTraceEvent(['nack', 'FOO', 'fir']);

	const dump3 = await audioProducer.dump();

	expect(dump3.traceEventTypes).toEqual(
		expect.arrayContaining(['nack', 'fir'])
	);

	await audioProducer.enableTraceEvent();

	const dump4 = await audioProducer.dump();

	expect(dump4.traceEventTypes).toEqual(expect.arrayContaining([]));
}, 2000);

test('producer.enableTraceEvent() with wrong arguments rejects with TypeError', async () => {
	const audioProducer = await ctx.webRtcTransport1!.produce(
		ctx.audioProducerOptions
	);

	// @ts-expect-error --- Testing purposes.
	await expect(audioProducer.enableTraceEvent(123)).rejects.toThrow(TypeError);

	// @ts-expect-error --- Testing purposes.
	await expect(audioProducer.enableTraceEvent('rtp')).rejects.toThrow(
		TypeError
	);

	await expect(
		// @ts-expect-error --- Testing purposes.
		audioProducer.enableTraceEvent(['fir', 123.123])
	).rejects.toThrow(TypeError);
}, 2000);

test('Producer emits "score"', async () => {
	const videoProducer = await ctx.webRtcTransport2!.produce(
		ctx.videoProducerOptions
	);

	// API not exposed in the interface.
	const channel = (videoProducer as Producer).channelForTesting;
	const onScore = jest.fn();

	videoProducer.on('score', onScore);

	// Simulate a 'score' notification coming through the channel.
	const builder = new flatbuffers.Builder();
	const producerScoreNotification = new FbsProducer.ScoreNotificationT([
		new FbsProducer.ScoreT(
			/* encodingIdx */ 0,
			/* ssrc */ 11,
			/* rid */ undefined,
			/* score */ 10
		),
		new FbsProducer.ScoreT(
			/* encodingIdx */ 1,
			/* ssrc */ 22,
			/* rid */ undefined,
			/* score */ 9
		),
	]);
	const notificationOffset = Notification.createNotification(
		builder,
		builder.createString(videoProducer.id),
		Event.PRODUCER_SCORE,
		NotificationBody.Producer_ScoreNotification,
		producerScoreNotification.pack(builder)
	);

	builder.finish(notificationOffset);

	const notification = Notification.getRootAsNotification(
		new flatbuffers.ByteBuffer(builder.asUint8Array())
	);

	channel.emit(videoProducer.id, Event.PRODUCER_SCORE, notification);
	channel.emit(videoProducer.id, Event.PRODUCER_SCORE, notification);
	channel.emit(videoProducer.id, Event.PRODUCER_SCORE, notification);

	expect(onScore).toHaveBeenCalledTimes(3);
	expect(videoProducer.score).toEqual([
		{ ssrc: 11, rid: undefined, score: 10, encodingIdx: 0 },
		{ ssrc: 22, rid: undefined, score: 9, encodingIdx: 1 },
	]);
}, 2000);

test('producer.close() succeeds', async () => {
	const audioProducer = await ctx.webRtcTransport1!.produce(
		ctx.audioProducerOptions
	);

	const onObserverClose = jest.fn();

	audioProducer.observer.once('close', onObserverClose);
	audioProducer.close();

	expect(onObserverClose).toHaveBeenCalledTimes(1);
	expect(audioProducer.closed).toBe(true);

	await expect(ctx.router!.dump()).resolves.toMatchObject({
		mapProducerIdConsumerIds: [],
		mapConsumerIdProducerId: [],
	});

	await expect(ctx.webRtcTransport1!.dump()).resolves.toMatchObject({
		id: ctx.webRtcTransport1!.id,
		producerIds: [],
		consumerIds: [],
	});
}, 2000);

test('Producer methods reject if closed', async () => {
	const audioProducer = await ctx.webRtcTransport1!.produce(
		ctx.audioProducerOptions
	);

	audioProducer.close();

	await expect(audioProducer.dump()).rejects.toThrow(Error);
	await expect(audioProducer.getStats()).rejects.toThrow(Error);
	await expect(audioProducer.pause()).rejects.toThrow(Error);
	await expect(audioProducer.resume()).rejects.toThrow(Error);
}, 2000);

test('Producer emits "transportclose" if Transport is closed', async () => {
	const videoProducer = await ctx.webRtcTransport2!.produce(
		ctx.videoProducerOptions
	);

	const onObserverClose = jest.fn();

	videoProducer.observer.once('close', onObserverClose);

	const promise = enhancedOnce<ProducerEvents>(videoProducer, 'transportclose');

	ctx.webRtcTransport2!.close();
	await promise;

	expect(onObserverClose).toHaveBeenCalledTimes(1);
	expect(videoProducer.closed).toBe(true);
}, 2000);
