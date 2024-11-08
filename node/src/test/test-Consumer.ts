import * as flatbuffers from 'flatbuffers';
import * as mediasoup from '../';
import { enhancedOnce } from '../enhancedEvents';
import { WorkerEvents, ConsumerEvents } from '../types';
import { Consumer } from '../Consumer';
import { UnsupportedError } from '../errors';
import * as utils from '../utils';
import {
	Notification,
	Body as NotificationBody,
	Event,
} from '../fbs/notification';
import * as FbsConsumer from '../fbs/consumer';

type TestContext = {
	mediaCodecs: mediasoup.types.RtpCodecCapability[];
	audioProducerOptions: mediasoup.types.ProducerOptions;
	videoProducerOptions: mediasoup.types.ProducerOptions;
	consumerDeviceCapabilities: mediasoup.types.RtpCapabilities;
	worker?: mediasoup.types.WorkerInterface;
	router?: mediasoup.types.RouterInterface;
	webRtcTransport1?: mediasoup.types.WebRtcTransportInterface;
	webRtcTransport2?: mediasoup.types.WebRtcTransportInterface;
	audioProducer?: mediasoup.types.ProducerInterface;
	videoProducer?: mediasoup.types.ProducerInterface;
};

const ctx: TestContext = {
	mediaCodecs: utils.deepFreeze<mediasoup.types.RtpCodecCapability[]>([
		{
			kind: 'audio',
			mimeType: 'audio/opus',
			clockRate: 48000,
			channels: 2,
			parameters: {
				foo: 'bar',
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
		},
	]),
	audioProducerOptions: utils.deepFreeze<mediasoup.types.ProducerOptions>({
		kind: 'audio',
		rtpParameters: {
			mid: 'AUDIO',
			codecs: [
				{
					mimeType: 'audio/opus',
					payloadType: 111,
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
			encodings: [{ ssrc: 11111111 }],
			rtcp: {
				cname: 'FOOBAR',
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
						{ type: 'nack', parameter: '' },
						{ type: 'nack', parameter: 'pli' },
						{ type: 'goog-remb', parameter: '' },
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
				{ ssrc: 22222222, scalabilityMode: 'L1T5', rtx: { ssrc: 22222223 } },
				{ ssrc: 22222224, scalabilityMode: 'L1T5', rtx: { ssrc: 22222225 } },
				{ ssrc: 22222226, scalabilityMode: 'L1T5', rtx: { ssrc: 22222227 } },
				{ ssrc: 22222228, scalabilityMode: 'L1T5', rtx: { ssrc: 22222229 } },
			],
			rtcp: {
				cname: 'FOOBAR',
			},
		},
		appData: { foo: 1, bar: '2' },
	}),
	consumerDeviceCapabilities: utils.deepFreeze<mediasoup.types.RtpCapabilities>(
		{
			codecs: [
				{
					mimeType: 'audio/opus',
					kind: 'audio',
					preferredPayloadType: 100,
					clockRate: 48000,
					channels: 2,
					rtcpFeedback: [{ type: 'nack', parameter: '' }],
				},
				{
					mimeType: 'video/H264',
					kind: 'video',
					preferredPayloadType: 101,
					clockRate: 90000,
					parameters: {
						'level-asymmetry-allowed': 1,
						'packetization-mode': 1,
						'profile-level-id': '4d0032',
					},
					rtcpFeedback: [
						{ type: 'nack', parameter: '' },
						{ type: 'nack', parameter: 'pli' },
						{ type: 'ccm', parameter: 'fir' },
						{ type: 'goog-remb', parameter: '' },
					],
				},
				{
					mimeType: 'video/rtx',
					kind: 'video',
					preferredPayloadType: 102,
					clockRate: 90000,
					parameters: {
						apt: 101,
					},
					rtcpFeedback: [],
				},
			],
			headerExtensions: [
				{
					kind: 'audio',
					uri: 'urn:ietf:params:rtp-hdrext:sdes:mid',
					preferredId: 1,
					preferredEncrypt: false,
				},
				{
					kind: 'video',
					uri: 'urn:ietf:params:rtp-hdrext:sdes:mid',
					preferredId: 1,
					preferredEncrypt: false,
				},
				{
					kind: 'video',
					uri: 'urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id',
					preferredId: 2,
					preferredEncrypt: false,
				},
				{
					kind: 'audio',
					uri: 'http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time',
					preferredId: 4,
					preferredEncrypt: false,
				},
				{
					kind: 'video',
					uri: 'http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time',
					preferredId: 4,
					preferredEncrypt: false,
				},
				{
					kind: 'audio',
					uri: 'urn:ietf:params:rtp-hdrext:ssrc-audio-level',
					preferredId: 10,
					preferredEncrypt: false,
				},
				{
					kind: 'video',
					uri: 'urn:3gpp:video-orientation',
					preferredId: 11,
					preferredEncrypt: false,
				},
				{
					kind: 'video',
					uri: 'urn:ietf:params:rtp-hdrext:toffset',
					preferredId: 12,
					preferredEncrypt: false,
				},
			],
		}
	),
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
	ctx.audioProducer = await ctx.webRtcTransport1.produce(
		ctx.audioProducerOptions
	);
	ctx.videoProducer = await ctx.webRtcTransport1.produce(
		ctx.videoProducerOptions
	);
});

afterEach(async () => {
	ctx.worker?.close();

	if (ctx.worker?.subprocessClosed === false) {
		await enhancedOnce<WorkerEvents>(ctx.worker, 'subprocessclose');
	}
});

test('transport.consume() succeeds', async () => {
	const onObserverNewConsumer1 = jest.fn();

	ctx.webRtcTransport2!.observer.once('newconsumer', onObserverNewConsumer1);

	expect(
		ctx.router!.canConsume({
			producerId: ctx.audioProducer!.id,
			rtpCapabilities: ctx.consumerDeviceCapabilities,
		})
	).toBe(true);

	const audioConsumer = await ctx.webRtcTransport2!.consume({
		producerId: ctx.audioProducer!.id,
		rtpCapabilities: ctx.consumerDeviceCapabilities,
		appData: { baz: 'LOL' },
	});

	expect(onObserverNewConsumer1).toHaveBeenCalledTimes(1);
	expect(onObserverNewConsumer1).toHaveBeenCalledWith(audioConsumer);
	expect(typeof audioConsumer.id).toBe('string');
	expect(audioConsumer.producerId).toBe(ctx.audioProducer!.id);
	expect(audioConsumer.closed).toBe(false);
	expect(audioConsumer.kind).toBe('audio');
	expect(typeof audioConsumer.rtpParameters).toBe('object');
	expect(audioConsumer.rtpParameters.mid).toBe('0');
	expect(audioConsumer.rtpParameters.codecs.length).toBe(1);
	expect(audioConsumer.rtpParameters.codecs[0]).toEqual({
		mimeType: 'audio/opus',
		payloadType: 100,
		clockRate: 48000,
		channels: 2,
		parameters: {
			useinbandfec: 1,
			usedtx: 1,
			foo: 222.222,
			bar: '333',
		},
		rtcpFeedback: [],
	});
	expect(audioConsumer.type).toBe('simple');
	expect(audioConsumer.paused).toBe(false);
	expect(audioConsumer.producerPaused).toBe(false);
	expect(audioConsumer.priority).toBe(1);
	expect(audioConsumer.score).toEqual({
		score: 10,
		producerScore: 0,
		producerScores: [0],
	});
	expect(audioConsumer.preferredLayers).toBeUndefined();
	expect(audioConsumer.currentLayers).toBeUndefined();
	expect(audioConsumer.appData).toEqual({ baz: 'LOL' });

	const dump1 = await ctx.router!.dump();

	expect(dump1.mapProducerIdConsumerIds).toEqual(
		expect.arrayContaining([
			{ key: ctx.audioProducer!.id, values: [audioConsumer.id] },
		])
	);

	expect(dump1.mapConsumerIdProducerId).toEqual(
		expect.arrayContaining([
			{ key: audioConsumer.id, value: ctx.audioProducer!.id },
		])
	);

	await expect(ctx.webRtcTransport2!.dump()).resolves.toMatchObject({
		id: ctx.webRtcTransport2!.id,
		producerIds: [],
		consumerIds: [audioConsumer.id],
	});

	const onObserverNewConsumer2 = jest.fn();

	ctx.webRtcTransport2!.observer.once('newconsumer', onObserverNewConsumer2);

	expect(
		ctx.router!.canConsume({
			producerId: ctx.videoProducer!.id,
			rtpCapabilities: ctx.consumerDeviceCapabilities,
		})
	).toBe(true);

	// Pause videoProducer.
	await ctx.videoProducer!.pause();

	const videoConsumer = await ctx.webRtcTransport2!.consume({
		producerId: ctx.videoProducer!.id,
		rtpCapabilities: ctx.consumerDeviceCapabilities,
		paused: true,
		preferredLayers: { spatialLayer: 12, temporalLayer: 0 },
		appData: { baz: 'LOL' },
	});

	expect(onObserverNewConsumer2).toHaveBeenCalledTimes(1);
	expect(onObserverNewConsumer2).toHaveBeenCalledWith(videoConsumer);
	expect(typeof videoConsumer.id).toBe('string');
	expect(videoConsumer.producerId).toBe(ctx.videoProducer!.id);
	expect(videoConsumer.closed).toBe(false);
	expect(videoConsumer.kind).toBe('video');
	expect(typeof videoConsumer.rtpParameters).toBe('object');
	expect(videoConsumer.rtpParameters.mid).toBe('1');
	expect(videoConsumer.rtpParameters.codecs.length).toBe(2);
	expect(videoConsumer.rtpParameters.codecs[0]).toEqual({
		mimeType: 'video/H264',
		payloadType: 103,
		clockRate: 90000,
		parameters: {
			'packetization-mode': 1,
			'profile-level-id': '4d0032',
		},
		rtcpFeedback: [
			{ type: 'nack', parameter: '' },
			{ type: 'nack', parameter: 'pli' },
			{ type: 'ccm', parameter: 'fir' },
			{ type: 'goog-remb', parameter: '' },
		],
	});
	expect(videoConsumer.rtpParameters.codecs[1]).toEqual({
		mimeType: 'video/rtx',
		payloadType: 104,
		clockRate: 90000,
		parameters: { apt: 103 },
		rtcpFeedback: [],
	});
	expect(videoConsumer.type).toBe('simulcast');
	expect(videoConsumer.paused).toBe(true);
	expect(videoConsumer.producerPaused).toBe(true);
	expect(videoConsumer.priority).toBe(1);
	expect(videoConsumer.score).toEqual({
		score: 10,
		producerScore: 0,
		producerScores: [0, 0, 0, 0],
	});
	expect(videoConsumer.preferredLayers).toEqual({
		spatialLayer: 3,
		temporalLayer: 0,
	});
	expect(videoConsumer.currentLayers).toBeUndefined();
	expect(videoConsumer.appData).toEqual({ baz: 'LOL' });

	const onObserverNewConsumer3 = jest.fn();

	ctx.webRtcTransport2!.observer.once('newconsumer', onObserverNewConsumer3);

	expect(
		ctx.router!.canConsume({
			producerId: ctx.videoProducer!.id,
			rtpCapabilities: ctx.consumerDeviceCapabilities,
		})
	).toBe(true);

	const videoPipeConsumer = await ctx.webRtcTransport2!.consume({
		producerId: ctx.videoProducer!.id,
		rtpCapabilities: ctx.consumerDeviceCapabilities,
		pipe: true,
	});

	expect(onObserverNewConsumer3).toHaveBeenCalledTimes(1);
	expect(onObserverNewConsumer3).toHaveBeenCalledWith(videoPipeConsumer);
	expect(typeof videoPipeConsumer.id).toBe('string');
	expect(videoPipeConsumer.producerId).toBe(ctx.videoProducer!.id);
	expect(videoPipeConsumer.closed).toBe(false);
	expect(videoPipeConsumer.kind).toBe('video');
	expect(typeof videoPipeConsumer.rtpParameters).toBe('object');
	expect(videoPipeConsumer.rtpParameters.mid).toBeUndefined();
	expect(videoPipeConsumer.rtpParameters.codecs.length).toBe(2);
	expect(videoPipeConsumer.rtpParameters.codecs[0]).toEqual({
		mimeType: 'video/H264',
		payloadType: 103,
		clockRate: 90000,
		parameters: {
			'packetization-mode': 1,
			'profile-level-id': '4d0032',
		},
		rtcpFeedback: [
			{ type: 'nack', parameter: '' },
			{ type: 'nack', parameter: 'pli' },
			{ type: 'ccm', parameter: 'fir' },
			{ type: 'goog-remb', parameter: '' },
		],
	});
	expect(videoPipeConsumer.rtpParameters.codecs[1]).toEqual({
		mimeType: 'video/rtx',
		payloadType: 104,
		clockRate: 90000,
		parameters: { apt: 103 },
		rtcpFeedback: [],
	});
	expect(videoPipeConsumer.type).toBe('pipe');
	expect(videoPipeConsumer.paused).toBe(false);
	expect(videoPipeConsumer.producerPaused).toBe(true);
	expect(videoPipeConsumer.priority).toBe(1);
	expect(videoPipeConsumer.score).toEqual({
		score: 10,
		producerScore: 10,
		producerScores: [0, 0, 0, 0],
	});
	expect(videoPipeConsumer.preferredLayers).toBeUndefined();
	expect(videoPipeConsumer.currentLayers).toBeUndefined();
	expect(videoPipeConsumer.appData).toEqual({});

	const dump2 = await ctx.router!.dump();

	expect(Array.isArray(dump2.mapProducerIdConsumerIds)).toBe(true);

	expect(dump2.mapProducerIdConsumerIds).toEqual(
		expect.arrayContaining([
			{
				key: ctx.audioProducer!.id,
				values: [audioConsumer.id],
			},
			{
				key: ctx.videoProducer!.id,
				values: expect.arrayContaining([
					videoConsumer.id,
					videoPipeConsumer.id,
				]),
			},
		])
	);
	expect(dump2.mapConsumerIdProducerId).toEqual(
		expect.arrayContaining([
			{ key: audioConsumer.id, value: ctx.audioProducer!.id },
			{ key: videoConsumer.id, value: ctx.videoProducer!.id },
			{ key: videoPipeConsumer.id, value: ctx.videoProducer!.id },
		])
	);

	await expect(ctx.webRtcTransport2!.dump()).resolves.toMatchObject({
		id: ctx.webRtcTransport2!.id,
		producerIds: [],
		consumerIds: expect.arrayContaining([
			audioConsumer.id,
			videoConsumer.id,
			videoPipeConsumer.id,
		]),
	});
}, 2000);

test('transport.consume() with enableRtx succeeds', async () => {
	const audioConsumer2 = await ctx.webRtcTransport2!.consume({
		producerId: ctx.audioProducer!.id,
		rtpCapabilities: ctx.consumerDeviceCapabilities,
		enableRtx: true,
	});

	expect(audioConsumer2.kind).toBe('audio');
	expect(audioConsumer2.rtpParameters.codecs.length).toBe(1);
	expect(audioConsumer2.rtpParameters.codecs[0]).toEqual({
		mimeType: 'audio/opus',
		payloadType: 100,
		clockRate: 48000,
		channels: 2,
		parameters: {
			useinbandfec: 1,
			usedtx: 1,
			foo: 222.222,
			bar: '333',
		},
		rtcpFeedback: [{ type: 'nack', parameter: '' }],
	});
}, 2000);

test('transport.consume() can be created with user provided mid', async () => {
	const audioConsumer1 = await ctx.webRtcTransport2!.consume({
		producerId: ctx.audioProducer!.id,
		rtpCapabilities: ctx.consumerDeviceCapabilities,
	});

	expect(audioConsumer1.rtpParameters.mid).toEqual(
		expect.stringMatching(/^[0-9]+/)
	);

	const audioConsumer2 = await ctx.webRtcTransport2!.consume({
		producerId: ctx.audioProducer!.id,
		mid: 'custom-mid',
		rtpCapabilities: ctx.consumerDeviceCapabilities,
	});

	expect(audioConsumer2.rtpParameters.mid).toBe('custom-mid');

	const audioConsumer3 = await ctx.webRtcTransport2!.consume({
		producerId: ctx.audioProducer!.id,
		rtpCapabilities: ctx.consumerDeviceCapabilities,
	});

	expect(audioConsumer3.rtpParameters.mid).toEqual(
		expect.stringMatching(/^[0-9]+/)
	);
	expect(Number(audioConsumer1.rtpParameters.mid) + 1).toBe(
		Number(audioConsumer3.rtpParameters.mid)
	);
}, 2000);

test('transport.consume() with incompatible rtpCapabilities rejects with UnsupportedError', async () => {
	let invalidDeviceCapabilities: mediasoup.types.RtpCapabilities;

	invalidDeviceCapabilities = {
		codecs: [
			{
				kind: 'audio',
				mimeType: 'audio/ISAC',
				preferredPayloadType: 100,
				clockRate: 32000,
				channels: 1,
			},
		],
		headerExtensions: [],
	};

	expect(
		ctx.router!.canConsume({
			producerId: ctx.audioProducer!.id,
			rtpCapabilities: invalidDeviceCapabilities,
		})
	).toBe(false);

	await expect(
		ctx.webRtcTransport2!.consume({
			producerId: ctx.audioProducer!.id,
			rtpCapabilities: invalidDeviceCapabilities,
		})
	).rejects.toThrow(UnsupportedError);

	invalidDeviceCapabilities = {
		codecs: [],
		headerExtensions: [],
	};

	expect(
		ctx.router!.canConsume({
			producerId: ctx.audioProducer!.id,
			rtpCapabilities: invalidDeviceCapabilities,
		})
	).toBe(false);

	await expect(
		ctx.webRtcTransport2!.consume({
			producerId: ctx.audioProducer!.id,
			rtpCapabilities: invalidDeviceCapabilities,
		})
	).rejects.toThrow(UnsupportedError);
}, 2000);

test('consumer.dump() succeeds', async () => {
	const audioConsumer = await ctx.webRtcTransport2!.consume({
		producerId: ctx.audioProducer!.id,
		rtpCapabilities: ctx.consumerDeviceCapabilities,
	});
	const dump1 = await audioConsumer.dump();

	expect(dump1.id).toBe(audioConsumer.id);
	expect(dump1.producerId).toBe(audioConsumer.producerId);
	expect(dump1.kind).toBe(audioConsumer.kind);
	expect(typeof dump1.rtpParameters).toBe('object');
	expect(Array.isArray(dump1.rtpParameters.codecs)).toBe(true);
	expect(dump1.rtpParameters.codecs.length).toBe(1);
	expect(dump1.rtpParameters.codecs[0].mimeType).toBe('audio/opus');
	expect(dump1.rtpParameters.codecs[0].payloadType).toBe(100);
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
	expect(dump1.rtpParameters.headerExtensions!.length).toBe(3);
	expect(dump1.rtpParameters.headerExtensions).toEqual([
		{
			uri: 'urn:ietf:params:rtp-hdrext:sdes:mid',
			id: 1,
			encrypt: false,
			parameters: {},
		},
		{
			uri: 'http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time',
			id: 4,
			parameters: {},
			encrypt: false,
		},
		{
			uri: 'urn:ietf:params:rtp-hdrext:ssrc-audio-level',
			id: 10,
			parameters: {},
			encrypt: false,
		},
	]);
	expect(Array.isArray(dump1.rtpParameters.encodings)).toBe(true);
	expect(dump1.rtpParameters.encodings!.length).toBe(1);
	expect(dump1.rtpParameters.encodings).toEqual([
		expect.objectContaining({
			codecPayloadType: 100,
			ssrc: audioConsumer.rtpParameters.encodings?.[0].ssrc,
		}),
	]);
	expect(dump1.type).toBe('simple');
	expect(Array.isArray(dump1.consumableRtpEncodings)).toBe(true);
	expect(dump1.consumableRtpEncodings!.length).toBe(1);
	expect(dump1.consumableRtpEncodings).toEqual([
		expect.objectContaining({
			ssrc: ctx.audioProducer!.consumableRtpParameters.encodings?.[0].ssrc,
		}),
	]);
	expect(dump1.supportedCodecPayloadTypes).toEqual([100]);
	expect(dump1.paused).toBe(false);
	expect(dump1.producerPaused).toBe(false);
	expect(dump1.priority).toBe(1);

	const videoConsumer = await ctx.webRtcTransport2!.consume({
		producerId: ctx.videoProducer!.id,
		rtpCapabilities: ctx.consumerDeviceCapabilities,
		paused: true,
	});
	const dump2 = await videoConsumer.dump();

	expect(dump2.id).toBe(videoConsumer.id);
	expect(dump2.producerId).toBe(videoConsumer.producerId);
	expect(dump2.kind).toBe(videoConsumer.kind);
	expect(typeof dump2.rtpParameters).toBe('object');
	expect(Array.isArray(dump2.rtpParameters.codecs)).toBe(true);
	expect(dump2.rtpParameters.codecs.length).toBe(2);
	expect(dump2.rtpParameters.codecs[0].mimeType).toBe('video/H264');
	expect(dump2.rtpParameters.codecs[0].payloadType).toBe(103);
	expect(dump2.rtpParameters.codecs[0].clockRate).toBe(90000);
	expect(dump2.rtpParameters.codecs[0].channels).toBeUndefined();
	expect(dump2.rtpParameters.codecs[0].parameters).toEqual({
		'packetization-mode': 1,
		'profile-level-id': '4d0032',
	});
	expect(dump2.rtpParameters.codecs[0].rtcpFeedback).toEqual([
		{ type: 'nack' },
		{ type: 'nack', parameter: 'pli' },
		{ type: 'ccm', parameter: 'fir' },
		{ type: 'goog-remb' },
	]);
	expect(Array.isArray(dump2.rtpParameters.headerExtensions)).toBe(true);
	expect(dump2.rtpParameters.headerExtensions!.length).toBe(4);
	expect(dump2.rtpParameters.headerExtensions).toEqual([
		{
			uri: 'urn:ietf:params:rtp-hdrext:sdes:mid',
			id: 1,
			encrypt: false,
			parameters: {},
		},
		{
			uri: 'http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time',
			id: 4,
			parameters: {},
			encrypt: false,
		},
		{
			uri: 'urn:3gpp:video-orientation',
			id: 11,
			parameters: {},
			encrypt: false,
		},
		{
			uri: 'urn:ietf:params:rtp-hdrext:toffset',
			id: 12,
			parameters: {},
			encrypt: false,
		},
	]);
	expect(Array.isArray(dump2.rtpParameters.encodings)).toBe(true);
	expect(dump2.rtpParameters.encodings!.length).toBe(1);
	expect(dump2.rtpParameters.encodings).toMatchObject([
		{
			codecPayloadType: 103,
			ssrc: videoConsumer.rtpParameters.encodings?.[0].ssrc,
			rtx: {
				ssrc: videoConsumer.rtpParameters.encodings?.[0].rtx?.ssrc,
			},
			scalabilityMode: 'L4T5',
		},
	]);
	expect(Array.isArray(dump2.consumableRtpEncodings)).toBe(true);
	expect(dump2.consumableRtpEncodings!.length).toBe(4);
	expect(dump2.consumableRtpEncodings![0]).toEqual(
		expect.objectContaining({
			ssrc: ctx.videoProducer!.consumableRtpParameters.encodings?.[0].ssrc,
			scalabilityMode: 'L1T5',
		})
	);
	expect(dump2.consumableRtpEncodings![1]).toEqual(
		expect.objectContaining({
			ssrc: ctx.videoProducer!.consumableRtpParameters.encodings?.[1].ssrc,
			scalabilityMode: 'L1T5',
		})
	);
	expect(dump2.consumableRtpEncodings![2]).toEqual(
		expect.objectContaining({
			ssrc: ctx.videoProducer!.consumableRtpParameters.encodings?.[2].ssrc,
			scalabilityMode: 'L1T5',
		})
	);
	expect(dump2.consumableRtpEncodings![3]).toEqual(
		expect.objectContaining({
			ssrc: ctx.videoProducer!.consumableRtpParameters.encodings?.[3].ssrc,
			scalabilityMode: 'L1T5',
		})
	);
	expect(dump2.supportedCodecPayloadTypes).toEqual([103]);
	expect(dump2.paused).toBe(true);
	expect(dump2.producerPaused).toBe(false);
	expect(dump2.priority).toBe(1);
}, 2000);

test('consumer.getStats() succeeds', async () => {
	const audioConsumer = await ctx.webRtcTransport2!.consume({
		producerId: ctx.audioProducer!.id,
		rtpCapabilities: ctx.consumerDeviceCapabilities,
	});

	await expect(audioConsumer.getStats()).resolves.toEqual([
		expect.objectContaining({
			type: 'outbound-rtp',
			kind: 'audio',
			mimeType: 'audio/opus',
			ssrc: audioConsumer.rtpParameters.encodings?.[0].ssrc,
		}),
	]);

	const videoConsumer = await ctx.webRtcTransport2!.consume({
		producerId: ctx.videoProducer!.id,
		rtpCapabilities: ctx.consumerDeviceCapabilities,
	});

	await expect(videoConsumer.getStats()).resolves.toEqual([
		expect.objectContaining({
			type: 'outbound-rtp',
			kind: 'video',
			mimeType: 'video/H264',
			ssrc: videoConsumer.rtpParameters.encodings?.[0].ssrc,
		}),
	]);
}, 2000);

test('consumer.pause() and resume() succeed', async () => {
	const audioConsumer = await ctx.webRtcTransport2!.consume({
		producerId: ctx.audioProducer!.id,
		rtpCapabilities: ctx.consumerDeviceCapabilities,
	});
	const onObserverPause = jest.fn();
	const onObserverResume = jest.fn();

	audioConsumer.observer.on('pause', onObserverPause);
	audioConsumer.observer.on('resume', onObserverResume);

	await audioConsumer.pause();
	expect(audioConsumer.paused).toBe(true);

	await expect(audioConsumer.dump()).resolves.toMatchObject({ paused: true });

	await audioConsumer.resume();
	expect(audioConsumer.paused).toBe(false);

	await expect(audioConsumer.dump()).resolves.toMatchObject({ paused: false });

	// Even if we don't await for pause()/resume() completion, the observer must
	// fire 'pause' and 'resume' events if state was the opposite.
	void audioConsumer.pause();
	void audioConsumer.resume();
	void audioConsumer.pause();
	void audioConsumer.pause();
	void audioConsumer.pause();
	await audioConsumer.resume();

	expect(onObserverPause).toHaveBeenCalledTimes(3);
	expect(onObserverResume).toHaveBeenCalledTimes(3);
}, 2000);

test('producer.pause() and resume() emit events', async () => {
	const audioConsumer = await ctx.webRtcTransport2!.consume({
		producerId: ctx.audioProducer!.id,
		rtpCapabilities: ctx.consumerDeviceCapabilities,
	});
	const promises = [];
	const events: string[] = [];

	audioConsumer.observer.once('resume', () => {
		events.push('resume');
	});

	audioConsumer.observer.once('pause', () => {
		events.push('pause');
	});

	promises.push(ctx.audioProducer!.pause());
	promises.push(ctx.audioProducer!.resume());

	await Promise.all(promises);

	// Must also wait a bit for the corresponding events in the consumer.
	await new Promise(resolve => setTimeout(resolve, 100));

	expect(events).toEqual(['pause', 'resume']);
	expect(audioConsumer.paused).toBe(false);
}, 2000);

test('consumer.setPreferredLayers() succeed', async () => {
	const audioConsumer = await ctx.webRtcTransport2!.consume({
		producerId: ctx.audioProducer!.id,
		rtpCapabilities: ctx.consumerDeviceCapabilities,
	});
	const videoConsumer = await ctx.webRtcTransport2!.consume({
		producerId: ctx.videoProducer!.id,
		rtpCapabilities: ctx.consumerDeviceCapabilities,
	});

	await audioConsumer.setPreferredLayers({ spatialLayer: 1, temporalLayer: 1 });

	expect(audioConsumer.preferredLayers).toBeUndefined();

	await videoConsumer.setPreferredLayers({ spatialLayer: 2, temporalLayer: 3 });

	expect(videoConsumer.preferredLayers).toEqual({
		spatialLayer: 2,
		temporalLayer: 3,
	});

	await videoConsumer.setPreferredLayers({ spatialLayer: 3 });

	expect(videoConsumer.preferredLayers).toEqual({
		spatialLayer: 3,
		temporalLayer: 4,
	});

	await videoConsumer.setPreferredLayers({ spatialLayer: 3, temporalLayer: 0 });

	expect(videoConsumer.preferredLayers).toEqual({
		spatialLayer: 3,
		temporalLayer: 0,
	});

	await videoConsumer.setPreferredLayers({
		spatialLayer: 66,
		temporalLayer: 66,
	});

	expect(videoConsumer.preferredLayers).toEqual({
		spatialLayer: 3,
		temporalLayer: 4,
	});
}, 2000);

test('consumer.setPreferredLayers() with wrong arguments rejects with TypeError', async () => {
	const videoConsumer = await ctx.webRtcTransport2!.consume({
		producerId: ctx.videoProducer!.id,
		rtpCapabilities: ctx.consumerDeviceCapabilities,
	});

	// @ts-expect-error --- Testing purposes.
	await expect(videoConsumer.setPreferredLayers({})).rejects.toThrow(TypeError);

	await expect(
		// @ts-expect-error --- Testing purposes.
		videoConsumer.setPreferredLayers({ foo: '123' })
	).rejects.toThrow(TypeError);

	// @ts-expect-error --- Testing purposes.
	await expect(videoConsumer.setPreferredLayers('foo')).rejects.toThrow(
		TypeError
	);

	// Missing spatialLayer.
	await expect(
		// @ts-expect-error --- Testing purposes.
		videoConsumer.setPreferredLayers({ temporalLayer: 2 })
	).rejects.toThrow(TypeError);
}, 2000);

test('consumer.setPriority() succeed', async () => {
	const videoConsumer = await ctx.webRtcTransport2!.consume({
		producerId: ctx.videoProducer!.id,
		rtpCapabilities: ctx.consumerDeviceCapabilities,
	});

	await videoConsumer.setPriority(2);
	expect(videoConsumer.priority).toBe(2);
}, 2000);

test('consumer.setPriority() with wrong arguments rejects with TypeError', async () => {
	const videoConsumer = await ctx.webRtcTransport2!.consume({
		producerId: ctx.videoProducer!.id,
		rtpCapabilities: ctx.consumerDeviceCapabilities,
	});

	// @ts-expect-error --- Testing purposes.
	await expect(videoConsumer.setPriority()).rejects.toThrow(TypeError);

	await expect(videoConsumer.setPriority(0)).rejects.toThrow(TypeError);

	// @ts-expect-error --- Testing purposes.
	await expect(videoConsumer.setPriority('foo')).rejects.toThrow(TypeError);
}, 2000);

test('consumer.unsetPriority() succeed', async () => {
	const videoConsumer = await ctx.webRtcTransport2!.consume({
		producerId: ctx.videoProducer!.id,
		rtpCapabilities: ctx.consumerDeviceCapabilities,
	});

	await videoConsumer.unsetPriority();
	expect(videoConsumer.priority).toBe(1);
}, 2000);

test('consumer.enableTraceEvent() succeed', async () => {
	const audioConsumer = await ctx.webRtcTransport2!.consume({
		producerId: ctx.audioProducer!.id,
		rtpCapabilities: ctx.consumerDeviceCapabilities,
	});

	await audioConsumer.enableTraceEvent(['rtp', 'pli']);
	const dump1 = await audioConsumer.dump();

	expect(dump1.traceEventTypes).toEqual(expect.arrayContaining(['rtp', 'pli']));

	await audioConsumer.enableTraceEvent();

	const dump2 = await audioConsumer.dump();

	expect(dump2.traceEventTypes).toEqual(expect.arrayContaining([]));

	// @ts-expect-error --- Testing purposes.
	await audioConsumer.enableTraceEvent(['nack', 'FOO', 'fir']);

	const dump3 = await audioConsumer.dump();

	expect(dump3.traceEventTypes).toEqual(
		expect.arrayContaining(['nack', 'fir'])
	);

	await audioConsumer.enableTraceEvent();

	const dump4 = await audioConsumer.dump();

	expect(dump4.traceEventTypes).toEqual(expect.arrayContaining([]));
}, 2000);

test('consumer.enableTraceEvent() with wrong arguments rejects with TypeError', async () => {
	const audioConsumer = await ctx.webRtcTransport2!.consume({
		producerId: ctx.audioProducer!.id,
		rtpCapabilities: ctx.consumerDeviceCapabilities,
	});

	// @ts-expect-error --- Testing purposes.
	await expect(audioConsumer.enableTraceEvent(123)).rejects.toThrow(TypeError);

	// @ts-expect-error --- Testing purposes.
	await expect(audioConsumer.enableTraceEvent('rtp')).rejects.toThrow(
		TypeError
	);

	await expect(
		// @ts-expect-error --- Testing purposes.
		audioConsumer.enableTraceEvent(['fir', 123.123])
	).rejects.toThrow(TypeError);
}, 2000);

test('Consumer emits "producerpause" and "producerresume"', async () => {
	const audioConsumer = await ctx.webRtcTransport2!.consume({
		producerId: ctx.audioProducer!.id,
		rtpCapabilities: ctx.consumerDeviceCapabilities,
	});

	await Promise.all([
		enhancedOnce<ConsumerEvents>(audioConsumer, 'producerpause'),

		// Let's await for pause() to resolve to avoid aborted channel requests
		// due to worker closure.
		ctx.audioProducer!.pause(),
	]);

	expect(audioConsumer.paused).toBe(false);
	expect(audioConsumer.producerPaused).toBe(true);

	await Promise.all([
		enhancedOnce<ConsumerEvents>(audioConsumer, 'producerresume'),

		// Let's await for resume() to resolve to avoid aborted channel requests
		// due to worker closure.
		ctx.audioProducer!.resume(),
	]);

	expect(audioConsumer.paused).toBe(false);
	expect(audioConsumer.producerPaused).toBe(false);
}, 2000);

test('Consumer emits "score"', async () => {
	const audioConsumer = await ctx.webRtcTransport2!.consume({
		producerId: ctx.audioProducer!.id,
		rtpCapabilities: ctx.consumerDeviceCapabilities,
	});

	// API not exposed in the interface.
	const channel = (audioConsumer as Consumer).channelForTesting;
	const onScore = jest.fn();

	audioConsumer.on('score', onScore);

	// Simulate a 'score' notification coming through the channel.
	const builder = new flatbuffers.Builder();
	const consumerScore = new FbsConsumer.ConsumerScoreT(9, 10, [8]);
	const consumerScoreNotification = new FbsConsumer.ScoreNotificationT(
		consumerScore
	);
	const notificationOffset = Notification.createNotification(
		builder,
		builder.createString(audioConsumer.id),
		Event.CONSUMER_SCORE,
		NotificationBody.Consumer_ScoreNotification,
		consumerScoreNotification.pack(builder)
	);

	builder.finish(notificationOffset);

	const notification = Notification.getRootAsNotification(
		new flatbuffers.ByteBuffer(builder.asUint8Array())
	);

	channel.emit(audioConsumer.id, Event.CONSUMER_SCORE, notification);
	channel.emit(audioConsumer.id, Event.CONSUMER_SCORE, notification);
	channel.emit(audioConsumer.id, Event.CONSUMER_SCORE, notification);

	expect(onScore).toHaveBeenCalledTimes(3);
	expect(audioConsumer.score).toEqual({
		score: 9,
		producerScore: 10,
		producerScores: [8],
	});
}, 2000);

test('consumer.close() succeeds', async () => {
	const audioConsumer = await ctx.webRtcTransport2!.consume({
		producerId: ctx.audioProducer!.id,
		rtpCapabilities: ctx.consumerDeviceCapabilities,
	});
	const videoConsumer = await ctx.webRtcTransport2!.consume({
		producerId: ctx.videoProducer!.id,
		rtpCapabilities: ctx.consumerDeviceCapabilities,
	});
	const onObserverClose = jest.fn();

	audioConsumer.observer.once('close', onObserverClose);
	audioConsumer.close();

	expect(onObserverClose).toHaveBeenCalledTimes(1);
	expect(audioConsumer.closed).toBe(true);

	const routerDump = await ctx.router!.dump();

	expect(routerDump.mapProducerIdConsumerIds).toEqual(
		expect.arrayContaining([
			{ key: ctx.audioProducer!.id, values: [] },
			{ key: ctx.videoProducer!.id, values: [videoConsumer.id] },
		])
	);
	expect(routerDump.mapConsumerIdProducerId).toEqual([
		{ key: videoConsumer!.id, value: ctx.videoProducer!.id },
	]);

	const transportDump = await ctx.webRtcTransport2!.dump();

	expect(transportDump).toMatchObject({
		id: ctx.webRtcTransport2!.id,
		producerIds: [],
		consumerIds: [videoConsumer.id],
	});
}, 2000);

test('Consumer methods reject if closed', async () => {
	const audioConsumer = await ctx.webRtcTransport2!.consume({
		producerId: ctx.audioProducer!.id,
		rtpCapabilities: ctx.consumerDeviceCapabilities,
	});

	audioConsumer.close();

	await expect(audioConsumer.dump()).rejects.toThrow(Error);

	await expect(audioConsumer.getStats()).rejects.toThrow(Error);

	await expect(audioConsumer.pause()).rejects.toThrow(Error);

	await expect(audioConsumer.resume()).rejects.toThrow(Error);

	// @ts-expect-error --- Testing purposes.
	await expect(audioConsumer.setPreferredLayers({})).rejects.toThrow(Error);

	await expect(audioConsumer.setPriority(2)).rejects.toThrow(Error);

	await expect(audioConsumer.requestKeyFrame()).rejects.toThrow(Error);
}, 2000);

test('Consumer emits "producerclose" if Producer is closed', async () => {
	const audioConsumer = await ctx.webRtcTransport2!.consume({
		producerId: ctx.audioProducer!.id,
		rtpCapabilities: ctx.consumerDeviceCapabilities,
	});
	const onObserverClose = jest.fn();

	audioConsumer.observer.once('close', onObserverClose);

	const promise = enhancedOnce<ConsumerEvents>(audioConsumer, 'producerclose');

	ctx.audioProducer!.close();
	await promise;

	expect(onObserverClose).toHaveBeenCalledTimes(1);
	expect(audioConsumer.closed).toBe(true);
}, 2000);

test('Consumer emits "transportclose" if Transport is closed', async () => {
	const videoConsumer = await ctx.webRtcTransport2!.consume({
		producerId: ctx.videoProducer!.id,
		rtpCapabilities: ctx.consumerDeviceCapabilities,
	});

	const onObserverClose = jest.fn();

	videoConsumer.observer.once('close', onObserverClose);

	const promise = enhancedOnce<ConsumerEvents>(videoConsumer, 'transportclose');

	ctx.webRtcTransport2!.close();
	await promise;

	expect(onObserverClose).toHaveBeenCalledTimes(1);
	expect(videoConsumer.closed).toBe(true);

	await expect(ctx.router!.dump()).resolves.toMatchObject({
		mapProducerIdConsumerIds: expect.arrayContaining([
			{ key: ctx.audioProducer!.id, values: [] },
			{ key: ctx.videoProducer!.id, values: [] },
		]),
		mapConsumerIdProducerId: [],
	});
}, 2000);
