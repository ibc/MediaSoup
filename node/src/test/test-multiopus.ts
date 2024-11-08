import * as mediasoup from '../';
import { enhancedOnce } from '../enhancedEvents';
import { WorkerEvents } from '../types';
import { UnsupportedError } from '../errors';
import * as utils from '../utils';

type TestContext = {
	mediaCodecs: mediasoup.types.RtpCodecCapability[];
	audioProducerOptions: mediasoup.types.ProducerOptions;
	consumerDeviceCapabilities: mediasoup.types.RtpCapabilities;
	worker?: mediasoup.types.Worker;
	router?: mediasoup.types.Router;
	webRtcTransport?: mediasoup.types.WebRtcTransport;
};

const ctx: TestContext = {
	mediaCodecs: utils.deepFreeze<mediasoup.types.RtpCodecCapability[]>([
		{
			kind: 'audio',
			mimeType: 'audio/multiopus',
			preferredPayloadType: 100,
			clockRate: 48000,
			channels: 6,
			parameters: {
				useinbandfec: 1,
				channel_mapping: '0,4,1,2,3,5',
				num_streams: 4,
				coupled_streams: 2,
			},
		},
	]),
	audioProducerOptions: utils.deepFreeze<mediasoup.types.ProducerOptions>({
		kind: 'audio',
		rtpParameters: {
			mid: 'AUDIO',
			codecs: [
				{
					mimeType: 'audio/multiopus',
					payloadType: 0,
					clockRate: 48000,
					channels: 6,
					parameters: {
						useinbandfec: 1,
						channel_mapping: '0,4,1,2,3,5',
						num_streams: 4,
						coupled_streams: 2,
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
		},
	}),
	consumerDeviceCapabilities: utils.deepFreeze<mediasoup.types.RtpCapabilities>(
		{
			codecs: [
				{
					mimeType: 'audio/multiopus',
					kind: 'audio',
					preferredPayloadType: 100,
					clockRate: 48000,
					channels: 6,
					parameters: {
						channel_mapping: '0,4,1,2,3,5',
						num_streams: 4,
						coupled_streams: 2,
					},
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
					kind: 'audio',
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
			],
		}
	),
};

beforeEach(async () => {
	ctx.worker = await mediasoup.createWorker();
	ctx.router = await ctx.worker.createRouter({ mediaCodecs: ctx.mediaCodecs });
	ctx.webRtcTransport = await ctx.router.createWebRtcTransport({
		listenInfos: [{ protocol: 'udp', ip: '127.0.0.1' }],
	});
});

afterEach(async () => {
	ctx.worker?.close();

	if (ctx.worker?.subprocessClosed === false) {
		await enhancedOnce<WorkerEvents>(ctx.worker, 'subprocessclose');
	}
});

test('produce() and consume() succeed', async () => {
	const audioProducer = await ctx.webRtcTransport!.produce(
		ctx.audioProducerOptions
	);

	expect(audioProducer.rtpParameters.codecs).toEqual([
		{
			mimeType: 'audio/multiopus',
			payloadType: 0,
			clockRate: 48000,
			channels: 6,
			parameters: {
				useinbandfec: 1,
				channel_mapping: '0,4,1,2,3,5',
				num_streams: 4,
				coupled_streams: 2,
			},
			rtcpFeedback: [],
		},
	]);

	expect(
		ctx.router!.canConsume({
			producerId: audioProducer.id,
			rtpCapabilities: ctx.consumerDeviceCapabilities,
		})
	).toBe(true);

	const audioConsumer = await ctx.webRtcTransport!.consume({
		producerId: audioProducer.id,
		rtpCapabilities: ctx.consumerDeviceCapabilities,
	});

	expect(audioConsumer.rtpParameters.codecs).toEqual([
		{
			mimeType: 'audio/multiopus',
			payloadType: 100,
			clockRate: 48000,
			channels: 6,
			parameters: {
				useinbandfec: 1,
				channel_mapping: '0,4,1,2,3,5',
				num_streams: 4,
				coupled_streams: 2,
			},
			rtcpFeedback: [],
		},
	]);
}, 2000);

test('fails to produce wrong parameters', async () => {
	await expect(
		ctx.webRtcTransport!.produce({
			kind: 'audio',
			rtpParameters: {
				mid: 'AUDIO',
				codecs: [
					{
						mimeType: 'audio/multiopus',
						payloadType: 0,
						clockRate: 48000,
						channels: 6,
						parameters: {
							channel_mapping: '0,4,1,2,3,5',
							num_streams: 2,
							coupled_streams: 2,
						},
					},
				],
			},
		})
	).rejects.toThrow(UnsupportedError);

	await expect(
		ctx.webRtcTransport!.produce({
			kind: 'audio',
			rtpParameters: {
				mid: 'AUDIO',
				codecs: [
					{
						mimeType: 'audio/multiopus',
						payloadType: 0,
						clockRate: 48000,
						channels: 6,
						parameters: {
							channel_mapping: '0,4,1,2,3,5',
							num_streams: 4,
							coupled_streams: 1,
						},
					},
				],
			},
		})
	).rejects.toThrow(UnsupportedError);
}, 2000);

test('fails to consume wrong channels', async () => {
	const audioProducer = await ctx.webRtcTransport!.produce(
		ctx.audioProducerOptions
	);

	const localConsumerDeviceCapabilities: mediasoup.types.RtpCapabilities = {
		codecs: [
			{
				mimeType: 'audio/multiopus',
				kind: 'audio',
				preferredPayloadType: 100,
				clockRate: 48000,
				channels: 8,
				parameters: {
					channel_mapping: '0,4,1,2,3,5',
					num_streams: 4,
					coupled_streams: 2,
				},
			},
		],
	};

	expect(
		!ctx.router!.canConsume({
			producerId: audioProducer.id,
			rtpCapabilities: localConsumerDeviceCapabilities,
		})
	).toBe(true);

	await expect(
		ctx.webRtcTransport!.consume({
			producerId: audioProducer.id,
			rtpCapabilities: localConsumerDeviceCapabilities,
		})
	).rejects.toThrow(Error);
}, 2000);
