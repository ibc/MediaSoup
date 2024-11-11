import { pickPort } from 'pick-port';
import * as mediasoup from '../';
import { enhancedOnce } from '../enhancedEvents';
import type {
	WorkerEvents,
	ConsumerEvents,
	ProducerObserverEvents,
	DataConsumerEvents,
} from '../types';
import * as utils from '../utils';

type TestContext = {
	mediaCodecs: mediasoup.types.RtpCodecCapability[];
	audioProducerOptions: mediasoup.types.ProducerOptions;
	videoProducerOptions: mediasoup.types.ProducerOptions;
	dataProducerOptions: mediasoup.types.DataProducerOptions;
	consumerDeviceCapabilities: mediasoup.types.RtpCapabilities;
	worker1?: mediasoup.types.Worker;
	worker2?: mediasoup.types.Worker;
	router1?: mediasoup.types.Router;
	router2?: mediasoup.types.Router;
	webRtcTransport1?: mediasoup.types.WebRtcTransport;
	webRtcTransport2?: mediasoup.types.WebRtcTransport;
	audioProducer?: mediasoup.types.Producer;
	videoProducer?: mediasoup.types.Producer;
	videoConsumer?: mediasoup.types.Consumer;
	dataProducer?: mediasoup.types.DataProducer;
	dataConsumer?: mediasoup.types.DataConsumer;
};

const ctx: TestContext = {
	mediaCodecs: utils.deepFreeze<mediasoup.types.RtpCodecCapability[]>([
		{
			kind: 'audio',
			mimeType: 'audio/opus',
			clockRate: 48000,
			channels: 2,
		},
		{
			kind: 'video',
			mimeType: 'video/VP8',
			clockRate: 90000,
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
						foo: 'bar1',
					},
				},
			],
			headerExtensions: [
				{
					uri: 'urn:ietf:params:rtp-hdrext:sdes:mid',
					id: 10,
				},
			],
			encodings: [{ ssrc: 11111111 }],
			rtcp: {
				cname: 'FOOBAR',
			},
		},
		appData: { foo: 'bar1' },
	}),
	videoProducerOptions: utils.deepFreeze<mediasoup.types.ProducerOptions>({
		kind: 'video',
		rtpParameters: {
			mid: 'VIDEO',
			codecs: [
				{
					mimeType: 'video/VP8',
					payloadType: 112,
					clockRate: 90000,
					rtcpFeedback: [
						{ type: 'nack' },
						{ type: 'nack', parameter: 'pli' },
						{ type: 'goog-remb' },
						{ type: 'lalala' },
					],
				},
			],
			headerExtensions: [
				{
					uri: 'urn:ietf:params:rtp-hdrext:sdes:mid',
					id: 10,
				},
				{
					uri: 'http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time',
					id: 11,
				},
				{
					uri: 'urn:3gpp:video-orientation',
					id: 13,
				},
			],
			encodings: [{ ssrc: 22222222 }, { ssrc: 22222223 }, { ssrc: 22222224 }],
			rtcp: {
				cname: 'FOOBAR',
			},
		},
		appData: { foo: 'bar2' },
	}),
	dataProducerOptions: utils.deepFreeze<mediasoup.types.DataProducerOptions>({
		sctpStreamParameters: {
			streamId: 666,
			ordered: false,
			maxPacketLifeTime: 5000,
		},
		label: 'foo',
		protocol: 'bar',
	}),
	consumerDeviceCapabilities: utils.deepFreeze<mediasoup.types.RtpCapabilities>(
		{
			codecs: [
				{
					kind: 'audio',
					mimeType: 'audio/opus',
					preferredPayloadType: 100,
					clockRate: 48000,
					channels: 2,
				},
				{
					kind: 'video',
					mimeType: 'video/VP8',
					preferredPayloadType: 101,
					clockRate: 90000,
					rtcpFeedback: [
						{ type: 'nack' },
						{ type: 'ccm', parameter: 'fir' },
						{ type: 'transport-cc' },
					],
				},
				{
					kind: 'video',
					mimeType: 'video/rtx',
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
					kind: 'video',
					uri: 'http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time',
					preferredId: 4,
					preferredEncrypt: false,
					direction: 'sendrecv',
				},
				{
					kind: 'video',
					uri: 'http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01',
					preferredId: 5,
					preferredEncrypt: false,
				},
				{
					kind: 'audio',
					uri: 'urn:ietf:params:rtp-hdrext:ssrc-audio-level',
					preferredId: 10,
					preferredEncrypt: false,
				},
			],
		}
	),
};

beforeEach(async () => {
	ctx.worker1 = await mediasoup.createWorker();
	ctx.worker2 = await mediasoup.createWorker();
	ctx.router1 = await ctx.worker1.createRouter({
		mediaCodecs: ctx.mediaCodecs,
	});
	ctx.router2 = await ctx.worker2.createRouter({
		mediaCodecs: ctx.mediaCodecs,
	});
	ctx.webRtcTransport1 = await ctx.router1.createWebRtcTransport({
		listenInfos: [{ protocol: 'udp', ip: '127.0.0.1' }],
		enableSctp: true,
	});
	ctx.webRtcTransport2 = await ctx.router2.createWebRtcTransport({
		listenIps: ['127.0.0.1'],
		enableSctp: true,
	});
	ctx.audioProducer = await ctx.webRtcTransport1.produce(
		ctx.audioProducerOptions
	);
	ctx.videoProducer = await ctx.webRtcTransport1.produce(
		ctx.videoProducerOptions
	);
	ctx.dataProducer = await ctx.webRtcTransport1.produceData(
		ctx.dataProducerOptions
	);
});

afterEach(async () => {
	ctx.worker1?.close();
	ctx.worker2?.close();

	if (ctx.worker1?.subprocessClosed === false) {
		await enhancedOnce<WorkerEvents>(ctx.worker1, 'subprocessclose');
	}

	if (ctx.worker2?.subprocessClosed === false) {
		await enhancedOnce<WorkerEvents>(ctx.worker2, 'subprocessclose');
	}
});

test('router.pipeToRouter() succeeds with audio', async () => {
	const { pipeConsumer, pipeProducer } = (await ctx.router1!.pipeToRouter({
		producerId: ctx.audioProducer!.id,
		router: ctx.router2!,
	})) as {
		pipeConsumer: mediasoup.types.Consumer;
		pipeProducer: mediasoup.types.Producer;
	};

	const dump1 = await ctx.router1!.dump();

	// There should be two Transports in router1:
	// - WebRtcTransport for audioProducer and videoProducer.
	// - PipeTransport between router1 and router2.
	expect(dump1.transportIds.length).toBe(2);

	const dump2 = await ctx.router2!.dump();

	// There should be two Transports in router2:
	// - WebRtcTransport for audioConsumer and videoConsumer.
	// - PipeTransport between router2 and router1.
	expect(dump2.transportIds.length).toBe(2);

	expect(typeof pipeConsumer.id).toBe('string');
	expect(pipeConsumer.closed).toBe(false);
	expect(pipeConsumer.kind).toBe('audio');
	expect(typeof pipeConsumer.rtpParameters).toBe('object');
	expect(pipeConsumer.rtpParameters.mid).toBeUndefined();
	expect(pipeConsumer.rtpParameters.codecs).toEqual([
		{
			mimeType: 'audio/opus',
			clockRate: 48000,
			payloadType: 100,
			channels: 2,
			parameters: {
				useinbandfec: 1,
				foo: 'bar1',
			},
			rtcpFeedback: [],
		},
	]);

	expect(pipeConsumer.rtpParameters.headerExtensions).toEqual([
		{
			uri: 'urn:ietf:params:rtp-hdrext:ssrc-audio-level',
			id: 10,
			encrypt: false,
			parameters: {},
		},
		{
			uri: 'http://www.webrtc.org/experiments/rtp-hdrext/abs-capture-time',
			id: 13,
			encrypt: false,
			parameters: {},
		},
		{
			uri: 'http://www.webrtc.org/experiments/rtp-hdrext/playout-delay',
			id: 14,
			encrypt: false,
			parameters: {},
		},
	]);
	expect(pipeConsumer.type).toBe('pipe');
	expect(pipeConsumer.paused).toBe(false);
	expect(pipeConsumer.producerPaused).toBe(false);
	expect(pipeConsumer.score).toEqual({
		score: 10,
		producerScore: 10,
		producerScores: [],
	});
	expect(pipeConsumer.appData).toEqual({});

	expect(pipeProducer.id).toBe(ctx.audioProducer!.id);
	expect(pipeProducer.closed).toBe(false);
	expect(pipeProducer.kind).toBe('audio');
	expect(typeof pipeProducer.rtpParameters).toBe('object');
	expect(pipeProducer.rtpParameters.mid).toBeUndefined();
	expect(pipeProducer.rtpParameters.codecs).toEqual([
		{
			mimeType: 'audio/opus',
			payloadType: 100,
			clockRate: 48000,
			channels: 2,
			parameters: {
				useinbandfec: 1,
				foo: 'bar1',
			},
			rtcpFeedback: [],
		},
	]);
	expect(pipeProducer.rtpParameters.headerExtensions).toEqual([
		{
			uri: 'urn:ietf:params:rtp-hdrext:ssrc-audio-level',
			id: 10,
			encrypt: false,
			parameters: {},
		},
		{
			uri: 'http://www.webrtc.org/experiments/rtp-hdrext/abs-capture-time',
			id: 13,
			encrypt: false,
			parameters: {},
		},
		{
			uri: 'http://www.webrtc.org/experiments/rtp-hdrext/playout-delay',
			id: 14,
			encrypt: false,
			parameters: {},
		},
	]);
	expect(pipeProducer.paused).toBe(false);
}, 2000);

test('router.pipeToRouter() succeeds with video', async () => {
	await ctx.videoProducer!.pause();

	const { pipeConsumer, pipeProducer } = (await ctx.router1!.pipeToRouter({
		producerId: ctx.videoProducer!.id,
		router: ctx.router2!,
	})) as {
		pipeConsumer: mediasoup.types.Consumer;
		pipeProducer: mediasoup.types.Producer;
	};

	const dump1 = await ctx.router1!.dump();

	// No new PipeTransport should has been created. The existing one is used.
	expect(dump1.transportIds.length).toBe(2);

	const dump2 = await ctx.router2!.dump();

	// No new PipeTransport should has been created. The existing one is used.
	expect(dump2.transportIds.length).toBe(2);

	expect(typeof pipeConsumer.id).toBe('string');
	expect(pipeConsumer.closed).toBe(false);
	expect(pipeConsumer.kind).toBe('video');
	expect(typeof pipeConsumer.rtpParameters).toBe('object');
	expect(pipeConsumer.rtpParameters.mid).toBeUndefined();
	expect(pipeConsumer.rtpParameters.codecs).toEqual([
		{
			mimeType: 'video/VP8',
			payloadType: 101,
			clockRate: 90000,
			parameters: {},
			rtcpFeedback: [
				{ type: 'nack', parameter: 'pli' },
				{ type: 'ccm', parameter: 'fir' },
			],
		},
	]);
	expect(pipeConsumer.rtpParameters.headerExtensions).toEqual([
		// NOTE: Remove this once framemarking draft becomes RFC.
		{
			uri: 'http://tools.ietf.org/html/draft-ietf-avtext-framemarking-07',
			id: 6,
			encrypt: false,
			parameters: {},
		},
		{
			uri: 'urn:ietf:params:rtp-hdrext:framemarking',
			id: 7,
			encrypt: false,
			parameters: {},
		},
		{
			uri: 'urn:3gpp:video-orientation',
			id: 11,
			encrypt: false,
			parameters: {},
		},
		{
			uri: 'urn:ietf:params:rtp-hdrext:toffset',
			id: 12,
			encrypt: false,
			parameters: {},
		},
		{
			uri: 'http://www.webrtc.org/experiments/rtp-hdrext/abs-capture-time',
			id: 13,
			encrypt: false,
			parameters: {},
		},
		{
			uri: 'http://www.webrtc.org/experiments/rtp-hdrext/playout-delay',
			id: 14,
			encrypt: false,
			parameters: {},
		},
	]);

	expect(pipeConsumer.type).toBe('pipe');
	expect(pipeConsumer.paused).toBe(false);
	expect(pipeConsumer.producerPaused).toBe(true);
	expect(pipeConsumer.score).toEqual({
		score: 10,
		producerScore: 10,
		producerScores: [],
	});
	expect(pipeConsumer.appData).toEqual({});

	expect(pipeProducer.id).toBe(ctx.videoProducer!.id);
	expect(pipeProducer.closed).toBe(false);
	expect(pipeProducer.kind).toBe('video');
	expect(typeof pipeProducer.rtpParameters).toBe('object');
	expect(pipeProducer.rtpParameters.mid).toBeUndefined();
	expect(pipeProducer.rtpParameters.codecs).toEqual([
		{
			mimeType: 'video/VP8',
			payloadType: 101,
			clockRate: 90000,
			parameters: {},
			rtcpFeedback: [
				{ type: 'nack', parameter: 'pli' },
				{ type: 'ccm', parameter: 'fir' },
			],
		},
	]);
	expect(pipeProducer.rtpParameters.headerExtensions).toEqual([
		// NOTE: Remove this once framemarking draft becomes RFC.
		{
			uri: 'http://tools.ietf.org/html/draft-ietf-avtext-framemarking-07',
			id: 6,
			encrypt: false,
			parameters: {},
		},
		{
			uri: 'urn:ietf:params:rtp-hdrext:framemarking',
			id: 7,
			encrypt: false,
			parameters: {},
		},
		{
			uri: 'urn:3gpp:video-orientation',
			id: 11,
			encrypt: false,
			parameters: {},
		},
		{
			uri: 'urn:ietf:params:rtp-hdrext:toffset',
			id: 12,
			encrypt: false,
			parameters: {},
		},
		{
			uri: 'http://www.webrtc.org/experiments/rtp-hdrext/abs-capture-time',
			id: 13,
			encrypt: false,
			parameters: {},
		},
		{
			uri: 'http://www.webrtc.org/experiments/rtp-hdrext/playout-delay',
			id: 14,
			encrypt: false,
			parameters: {},
		},
	]);
	expect(pipeProducer.paused).toBe(true);
}, 2000);

test('router.createPipeTransport() with wrong arguments rejects with TypeError', async () => {
	// @ts-expect-error --- Testing purposes.
	await expect(ctx.router1!.createPipeTransport({})).rejects.toThrow(TypeError);

	await expect(
		ctx.router1!.createPipeTransport({
			listenInfo: {
				protocol: 'udp',
				ip: '127.0.0.1',
				portRange: { min: 4000, max: 3000 },
			},
		})
	).rejects.toThrow(TypeError);

	await expect(
		ctx.router1!.createPipeTransport({ listenIp: '123' })
	).rejects.toThrow(TypeError);

	await expect(
		// @ts-expect-error --- Testing purposes.
		ctx.router1!.createPipeTransport({ listenIp: ['127.0.0.1'] })
	).rejects.toThrow(TypeError);

	await expect(
		ctx.router1!.createPipeTransport({
			listenInfo: { protocol: 'tcp', ip: '127.0.0.1' },
		})
	).rejects.toThrow(TypeError);

	await expect(
		ctx.router1!.createPipeTransport({
			listenInfo: { protocol: 'udp', ip: '127.0.0.1' },
			// @ts-expect-error --- Testing purposes.
			appData: 'NOT-AN-OBJECT',
		})
	).rejects.toThrow(TypeError);
}, 2000);

test('router.createPipeTransport() with enableRtx succeeds', async () => {
	const pipeTransport = await ctx.router1!.createPipeTransport({
		listenInfo: {
			protocol: 'udp',
			ip: '127.0.0.1',
			portRange: { min: 2000, max: 3000 },
		},
		enableRtx: true,
	});

	expect(pipeTransport.type).toBe('pipe');

	const pipeConsumer = await pipeTransport.consume({
		producerId: ctx.videoProducer!.id,
	});

	expect(typeof pipeConsumer.id).toBe('string');
	expect(pipeConsumer.closed).toBe(false);
	expect(pipeConsumer.kind).toBe('video');
	expect(typeof pipeConsumer.rtpParameters).toBe('object');
	expect(pipeConsumer.rtpParameters.mid).toBeUndefined();
	expect(pipeConsumer.rtpParameters.codecs).toEqual([
		{
			mimeType: 'video/VP8',
			payloadType: 101,
			clockRate: 90000,
			parameters: {},
			rtcpFeedback: [
				{ type: 'nack', parameter: '' },
				{ type: 'nack', parameter: 'pli' },
				{ type: 'ccm', parameter: 'fir' },
			],
		},
		{
			mimeType: 'video/rtx',
			payloadType: 102,
			clockRate: 90000,
			parameters: { apt: 101 },
			rtcpFeedback: [],
		},
	]);
	expect(pipeConsumer.rtpParameters.headerExtensions).toEqual([
		// NOTE: Remove this once framemarking draft becomes RFC.
		{
			uri: 'http://tools.ietf.org/html/draft-ietf-avtext-framemarking-07',
			id: 6,
			encrypt: false,
			parameters: {},
		},
		{
			uri: 'urn:ietf:params:rtp-hdrext:framemarking',
			id: 7,
			encrypt: false,
			parameters: {},
		},
		{
			uri: 'urn:3gpp:video-orientation',
			id: 11,
			encrypt: false,
			parameters: {},
		},
		{
			uri: 'urn:ietf:params:rtp-hdrext:toffset',
			id: 12,
			encrypt: false,
			parameters: {},
		},
		{
			uri: 'http://www.webrtc.org/experiments/rtp-hdrext/abs-capture-time',
			id: 13,
			encrypt: false,
			parameters: {},
		},
		{
			uri: 'http://www.webrtc.org/experiments/rtp-hdrext/playout-delay',
			id: 14,
			encrypt: false,
			parameters: {},
		},
	]);

	expect(pipeConsumer.type).toBe('pipe');
	expect(pipeConsumer.paused).toBe(false);
	expect(pipeConsumer.producerPaused).toBe(false);
	expect(pipeConsumer.score).toEqual({
		score: 10,
		producerScore: 10,
		producerScores: [],
	});
	expect(pipeConsumer.appData).toEqual({});
}, 2000);

test('pipeTransport.connect() with valid SRTP parameters succeeds', async () => {
	const pipeTransport = await ctx.router1!.createPipeTransport({
		listenIp: '127.0.0.1',
		enableSrtp: true,
	});

	expect(typeof pipeTransport.srtpParameters).toBe('object');
	// The master length of AEAD_AES_256_GCM.
	expect(pipeTransport.srtpParameters?.keyBase64.length).toBe(60);

	// Valid srtpParameters.
	await expect(
		pipeTransport.connect({
			ip: '127.0.0.2',
			port: 9999,
			srtpParameters: {
				cryptoSuite: 'AEAD_AES_256_GCM',
				keyBase64:
					'YTdjcDBvY2JoMGY5YXNlNDc0eDJsdGgwaWRvNnJsamRrdG16aWVpZHphdHo=',
			},
		})
	).resolves.toBeUndefined();
}, 2000);

test('pipeTransport.connect() with srtpParameters fails if enableSrtp is unset', async () => {
	const pipeTransport = await ctx.router1!.createPipeTransport({
		listenInfo: {
			protocol: 'udp',
			ip: '127.0.0.1',
			portRange: { min: 2000, max: 3000 },
		},
		enableRtx: true,
	});

	expect(pipeTransport.srtpParameters).toBeUndefined();

	// No SRTP enabled so passing srtpParameters must fail.
	await expect(
		pipeTransport.connect({
			ip: '127.0.0.2',
			port: 9999,
			srtpParameters: {
				cryptoSuite: 'AEAD_AES_256_GCM',
				keyBase64:
					'YTdjcDBvY2JoMGY5YXNlNDc0eDJsdGgwaWRvNnJsamRrdG16aWVpZHphdHo=',
			},
		})
	).rejects.toThrow(TypeError);

	// No SRTP enabled so passing srtpParameters (even if invalid) must fail.
	await expect(
		pipeTransport.connect({
			ip: '127.0.0.2',
			port: 9999,
			// @ts-expect-error --- Testing purposes.
			srtpParameters: 'invalid',
		})
	).rejects.toThrow(TypeError);
});

test('pipeTransport.connect() with invalid srtpParameters fails', async () => {
	const pipeTransport = await ctx.router1!.createPipeTransport({
		listenIp: '127.0.0.1',
		enableSrtp: true,
	});

	expect(typeof pipeTransport.id).toBe('string');
	expect(typeof pipeTransport.srtpParameters).toBe('object');
	// The master length of AEAD_AES_256_GCM.
	expect(pipeTransport.srtpParameters?.keyBase64.length).toBe(60);

	// Missing srtpParameters.
	await expect(
		pipeTransport.connect({
			ip: '127.0.0.2',
			port: 9999,
		})
	).rejects.toThrow(TypeError);

	// Invalid srtpParameters.
	await expect(
		pipeTransport.connect({
			ip: '127.0.0.2',
			port: 9999,
			// @ts-expect-error --- Testing purposes.
			srtpParameters: 1,
		})
	).rejects.toThrow(TypeError);

	// Missing srtpParameters.cryptoSuite.
	await expect(
		pipeTransport.connect({
			ip: '127.0.0.2',
			port: 9999,
			// @ts-expect-error --- Testing purposes.
			srtpParameters: {
				keyBase64:
					'YTdjcDBvY2JoMGY5YXNlNDc0eDJsdGgwaWRvNnJsamRrdG16aWVpZHphdHo=',
			},
		})
	).rejects.toThrow(TypeError);

	// Missing srtpParameters.keyBase64.
	await expect(
		pipeTransport.connect({
			ip: '127.0.0.2',
			port: 9999,
			// @ts-expect-error --- Testing purposes.
			srtpParameters: {
				cryptoSuite: 'AEAD_AES_256_GCM',
			},
		})
	).rejects.toThrow(TypeError);

	// Invalid srtpParameters.cryptoSuite.
	await expect(
		pipeTransport.connect({
			ip: '127.0.0.2',
			port: 9999,
			srtpParameters: {
				// @ts-expect-error --- Testing purposes.
				cryptoSuite: 'FOO',
				keyBase64:
					'YTdjcDBvY2JoMGY5YXNlNDc0eDJsdGgwaWRvNnJsamRrdG16aWVpZHphdHo=',
			},
		})
	).rejects.toThrow(TypeError);

	// Invalid srtpParameters.cryptoSuite.
	await expect(
		pipeTransport.connect({
			ip: '127.0.0.2',
			port: 9999,
			srtpParameters: {
				// @ts-expect-error --- Testing purposes.
				cryptoSuite: 123,
				keyBase64:
					'YTdjcDBvY2JoMGY5YXNlNDc0eDJsdGgwaWRvNnJsamRrdG16aWVpZHphdHo=',
			},
		})
	).rejects.toThrow(TypeError);

	// Invalid srtpParameters.keyBase64.
	await expect(
		pipeTransport.connect({
			ip: '127.0.0.2',
			port: 9999,
			srtpParameters: {
				cryptoSuite: 'AEAD_AES_256_GCM',
				// @ts-expect-error --- Testing purposes.
				keyBase64: [],
			},
		})
	).rejects.toThrow(TypeError);
}, 2000);

test('router.createPipeTransport() with fixed port succeeds', async () => {
	const port = await pickPort({
		type: 'udp',
		ip: '127.0.0.1',
		reserveTimeout: 0,
	});
	const pipeTransport = await ctx.router1!.createPipeTransport({
		listenInfo: { protocol: 'udp', ip: '127.0.0.1', port },
	});

	expect(pipeTransport.tuple.localPort).toEqual(port);
}, 2000);

test('transport.consume() for a pipe Producer succeeds', async () => {
	await ctx.router1!.pipeToRouter({
		producerId: ctx.videoProducer!.id,
		router: ctx.router2!,
	});

	const videoConsumer = await ctx.webRtcTransport2!.consume({
		producerId: ctx.videoProducer!.id,
		rtpCapabilities: ctx.consumerDeviceCapabilities,
	});

	expect(typeof videoConsumer.id).toBe('string');
	expect(videoConsumer.closed).toBe(false);
	expect(videoConsumer.kind).toBe('video');
	expect(typeof videoConsumer.rtpParameters).toBe('object');
	expect(videoConsumer.rtpParameters.mid).toBe('0');
	expect(videoConsumer.rtpParameters.codecs).toEqual([
		{
			mimeType: 'video/VP8',
			payloadType: 101,
			clockRate: 90000,
			parameters: {},
			rtcpFeedback: [
				{ type: 'nack', parameter: '' },
				{ type: 'ccm', parameter: 'fir' },
				{ type: 'transport-cc', parameter: '' },
			],
		},
		{
			mimeType: 'video/rtx',
			payloadType: 102,
			clockRate: 90000,
			parameters: {
				apt: 101,
			},
			rtcpFeedback: [],
		},
	]);
	expect(videoConsumer.rtpParameters.headerExtensions).toEqual([
		{
			uri: 'http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time',
			id: 4,
			encrypt: false,
			parameters: {},
		},
		{
			uri: 'http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01',
			id: 5,
			encrypt: false,
			parameters: {},
		},
	]);
	expect(videoConsumer.rtpParameters.encodings?.length).toBe(1);
	expect(typeof videoConsumer.rtpParameters.encodings?.[0].ssrc).toBe('number');
	expect(typeof videoConsumer.rtpParameters.encodings?.[0].rtx).toBe('object');
	expect(typeof videoConsumer.rtpParameters.encodings?.[0].rtx?.ssrc).toBe(
		'number'
	);
	expect(videoConsumer.type).toBe('simulcast');
	expect(videoConsumer.paused).toBe(false);
	expect(videoConsumer.producerPaused).toBe(false);
	expect(videoConsumer.score).toEqual({
		score: 10,
		producerScore: 0,
		producerScores: [0, 0, 0],
	});
	expect(videoConsumer.appData).toEqual({});
}, 2000);

test('producer.pause() and producer.resume() are transmitted to pipe Consumer', async () => {
	await ctx.videoProducer!.pause();

	// We need to obtain the pipeProducer to await for its 'puase' and 'resume'
	// events, otherwise we may get errors like this:
	// InvalidStateError: Channel closed, pending request aborted [method:PRODUCER_PAUSE, id:8]
	// See related fixed issue:
	// https://github.com/versatica/mediasoup/issues/1374
	const { pipeProducer: pipeVideoProducer } = await ctx.router1!.pipeToRouter({
		producerId: ctx.videoProducer!.id,
		router: ctx.router2!,
	});

	const videoConsumer = await ctx.webRtcTransport2!.consume({
		producerId: ctx.videoProducer!.id,
		rtpCapabilities: ctx.consumerDeviceCapabilities,
	});

	expect(ctx.videoProducer!.paused).toBe(true);
	expect(videoConsumer.producerPaused).toBe(true);
	expect(videoConsumer.paused).toBe(false);

	// NOTE: Let's use a Promise since otherwise there may be race conditions
	// between events and await lines below.
	const promise1 = enhancedOnce<ConsumerEvents>(
		videoConsumer,
		'producerresume'
	);
	const promise2 = enhancedOnce<ProducerObserverEvents>(
		pipeVideoProducer!.observer,
		'resume'
	);

	await ctx.videoProducer!.resume();
	await Promise.all([promise1, promise2]);

	expect(videoConsumer.producerPaused).toBe(false);
	expect(videoConsumer.paused).toBe(false);
	expect(pipeVideoProducer!.paused).toBe(false);

	const promise3 = enhancedOnce<ConsumerEvents>(videoConsumer, 'producerpause');
	const promise4 = enhancedOnce<ProducerObserverEvents>(
		pipeVideoProducer!.observer,
		'pause'
	);

	await ctx.videoProducer!.pause();
	await Promise.all([promise3, promise4]);

	expect(videoConsumer.producerPaused).toBe(true);
	expect(videoConsumer.paused).toBe(false);
	expect(pipeVideoProducer!.paused).toBe(true);
}, 2000);

test('producer.close() is transmitted to pipe Consumer', async () => {
	await ctx.router1!.pipeToRouter({
		producerId: ctx.videoProducer!.id,
		router: ctx.router2!,
	});

	const videoConsumer = await ctx.webRtcTransport2!.consume({
		producerId: ctx.videoProducer!.id,
		rtpCapabilities: ctx.consumerDeviceCapabilities,
	});

	ctx.videoProducer!.close();

	expect(ctx.videoProducer!.closed).toBe(true);

	if (!videoConsumer.closed) {
		await enhancedOnce<ConsumerEvents>(videoConsumer, 'producerclose');
	}

	expect(videoConsumer.closed).toBe(true);
}, 2000);

test('router.pipeToRouter() fails if both Routers belong to the same Worker', async () => {
	const router1bis = await ctx.worker1!.createRouter({
		mediaCodecs: ctx.mediaCodecs,
	});

	await expect(
		ctx.router1!.pipeToRouter({
			producerId: ctx.videoProducer!.id,
			router: router1bis,
		})
	).rejects.toThrow(Error);
}, 2000);

test('router.pipeToRouter() succeeds with data', async () => {
	const { pipeDataConsumer, pipeDataProducer } =
		(await ctx.router1!.pipeToRouter({
			dataProducerId: ctx.dataProducer!.id,
			router: ctx.router2!,
		})) as {
			pipeDataConsumer: mediasoup.types.DataConsumer;
			pipeDataProducer: mediasoup.types.DataProducer;
		};

	const dump1 = await ctx.router1!.dump();

	// There should be two Transports in router1:
	// - WebRtcTransport for audioProducer, videoProducer and dataProducer.
	// - PipeTransport between router1 and router2.
	expect(dump1.transportIds.length).toBe(2);

	const dump2 = await ctx.router2!.dump();

	// There should be two Transports in router2:
	// - WebRtcTransport for audioConsumer, videoConsumer and dataConsumer.
	// - PipeTransport between router2 and router1.
	expect(dump2.transportIds.length).toBe(2);

	expect(typeof pipeDataConsumer.id).toBe('string');
	expect(pipeDataConsumer.closed).toBe(false);
	expect(pipeDataConsumer.type).toBe('sctp');
	expect(typeof pipeDataConsumer.sctpStreamParameters).toBe('object');
	expect(typeof pipeDataConsumer.sctpStreamParameters?.streamId).toBe('number');
	expect(pipeDataConsumer.sctpStreamParameters?.ordered).toBe(false);
	expect(pipeDataConsumer.sctpStreamParameters?.maxPacketLifeTime).toBe(5000);
	expect(pipeDataConsumer.sctpStreamParameters?.maxRetransmits).toBeUndefined();
	expect(pipeDataConsumer.label).toBe('foo');
	expect(pipeDataConsumer.protocol).toBe('bar');

	expect(pipeDataProducer.id).toBe(ctx.dataProducer!.id);
	expect(pipeDataProducer.closed).toBe(false);
	expect(pipeDataProducer.type).toBe('sctp');
	expect(typeof pipeDataProducer.sctpStreamParameters).toBe('object');
	expect(typeof pipeDataProducer.sctpStreamParameters?.streamId).toBe('number');
	expect(pipeDataProducer.sctpStreamParameters?.ordered).toBe(false);
	expect(pipeDataProducer.sctpStreamParameters?.maxPacketLifeTime).toBe(5000);
	expect(pipeDataProducer.sctpStreamParameters?.maxRetransmits).toBeUndefined();
	expect(pipeDataProducer.label).toBe('foo');
	expect(pipeDataProducer.protocol).toBe('bar');
}, 2000);

test('transport.dataConsume() for a pipe DataProducer succeeds', async () => {
	await ctx.router1!.pipeToRouter({
		dataProducerId: ctx.dataProducer!.id,
		router: ctx.router2!,
	});

	const dataConsumer = await ctx.webRtcTransport2!.consumeData({
		dataProducerId: ctx.dataProducer!.id,
	});

	expect(typeof dataConsumer.id).toBe('string');
	expect(dataConsumer.closed).toBe(false);
	expect(dataConsumer.type).toBe('sctp');
	expect(typeof dataConsumer.sctpStreamParameters).toBe('object');
	expect(typeof dataConsumer.sctpStreamParameters?.streamId).toBe('number');
	expect(dataConsumer.sctpStreamParameters?.ordered).toBe(false);
	expect(dataConsumer.sctpStreamParameters?.maxPacketLifeTime).toBe(5000);
	expect(dataConsumer.sctpStreamParameters?.maxRetransmits).toBeUndefined();
	expect(dataConsumer.label).toBe('foo');
	expect(dataConsumer.protocol).toBe('bar');
}, 2000);

test('dataProducer.close() is transmitted to pipe DataConsumer', async () => {
	await ctx.router1!.pipeToRouter({
		dataProducerId: ctx.dataProducer!.id,
		router: ctx.router2!,
	});

	const dataConsumer = await ctx.webRtcTransport2!.consumeData({
		dataProducerId: ctx.dataProducer!.id,
	});

	ctx.dataProducer!.close();

	expect(ctx.dataProducer!.closed).toBe(true);

	if (!dataConsumer.closed) {
		await enhancedOnce<DataConsumerEvents>(dataConsumer, 'dataproducerclose');
	}

	expect(dataConsumer.closed).toBe(true);
}, 2000);

test('router.pipeToRouter() called twice generates a single PipeTransport pair', async () => {
	const routerA = await ctx.worker1!.createRouter({
		mediaCodecs: ctx.mediaCodecs,
	});
	const routerB = await ctx.worker2!.createRouter({
		mediaCodecs: ctx.mediaCodecs,
	});
	const transportA1 = await routerA.createWebRtcTransport({
		listenIps: ['127.0.0.1'],
	});
	const transportA2 = await routerA.createWebRtcTransport({
		listenIps: ['127.0.0.1'],
	});
	const audioProducerA1 = await transportA1.produce(ctx.audioProducerOptions);
	const audioProducerA2 = await transportA2.produce(ctx.audioProducerOptions);

	await Promise.all([
		routerA.pipeToRouter({
			producerId: audioProducerA1.id,
			router: routerB,
		}),
		routerA.pipeToRouter({
			producerId: audioProducerA2.id,
			router: routerB,
		}),
	]);

	const dump1 = await routerA.dump();

	// There should be 3 Transports in routerA:
	// - WebRtcTransport for audioProducerA1 and audioProducerA2.
	// - PipeTransport between routerA and routerB.
	expect(dump1.transportIds.length).toBe(3);

	const dump2 = await routerB.dump();

	// There should be 1 Transport in routerB:
	// - PipeTransport between routerA and routerB.
	expect(dump2.transportIds.length).toBe(1);
}, 2000);

test('router.pipeToRouter() called in two Routers passing one to each other as argument generates a single PipeTransport pair', async () => {
	const routerA = await ctx.worker1!.createRouter({
		mediaCodecs: ctx.mediaCodecs,
	});
	const routerB = await ctx.worker2!.createRouter({
		mediaCodecs: ctx.mediaCodecs,
	});
	const transportA = await routerA.createWebRtcTransport({
		listenIps: ['127.0.0.1'],
	});
	const transportB = await routerB.createWebRtcTransport({
		listenIps: ['127.0.0.1'],
	});
	const audioProducerA = await transportA.produce(ctx.audioProducerOptions);
	const audioProducerB = await transportB.produce(ctx.audioProducerOptions);
	const pipeTransportsA = new Map();
	const pipeTransportsB = new Map();

	routerA.observer.on('newtransport', transport => {
		if (transport.constructor.name !== 'PipeTransportImpl') {
			return;
		}

		pipeTransportsA.set(transport.id, transport);
		transport.observer.on('close', () => pipeTransportsA.delete(transport.id));
	});

	routerB.observer.on('newtransport', transport => {
		if (transport.constructor.name !== 'PipeTransportImpl') {
			return;
		}

		pipeTransportsB.set(transport.id, transport);
		transport.observer.on('close', () => pipeTransportsB.delete(transport.id));
	});

	await Promise.all([
		routerA.pipeToRouter({
			producerId: audioProducerA.id,
			router: routerB,
		}),
		routerB.pipeToRouter({
			producerId: audioProducerB.id,
			router: routerA,
		}),
	]);

	// There should be a single PipeTransport in each Router and they must be
	// connected.

	expect(pipeTransportsA.size).toBe(1);
	expect(pipeTransportsB.size).toBe(1);

	const pipeTransportA = Array.from(pipeTransportsA.values())[0];
	const pipeTransportB = Array.from(pipeTransportsB.values())[0];

	expect(pipeTransportA.tuple.localPort).toBe(pipeTransportB.tuple.remotePort);
	expect(pipeTransportB.tuple.localPort).toBe(pipeTransportA.tuple.remotePort);

	routerA.close();

	expect(pipeTransportsA.size).toBe(0);
	expect(pipeTransportsB.size).toBe(0);
}, 2000);
