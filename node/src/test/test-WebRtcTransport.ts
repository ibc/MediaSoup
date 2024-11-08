import { pickPort } from 'pick-port';
import * as flatbuffers from 'flatbuffers';
import * as mediasoup from '../';
import { enhancedOnce } from '../enhancedEvents';
import { WorkerEvents, WebRtcTransportEvents } from '../types';
import { WebRtcTransportImpl } from '../WebRtcTransport';
import { TransportTuple } from '../TransportTypes';
import { serializeProtocol } from '../Transport';
import * as utils from '../utils';
import {
	Notification,
	Body as NotificationBody,
	Event,
} from '../fbs/notification';
import * as FbsTransport from '../fbs/transport';
import * as FbsWebRtcTransport from '../fbs/web-rtc-transport';

type TestContext = {
	mediaCodecs: mediasoup.types.RtpCodecCapability[];
	worker?: mediasoup.types.WorkerInterface;
	router?: mediasoup.types.RouterInterface;
};

const ctx: TestContext = {
	mediaCodecs: utils.deepFreeze<mediasoup.types.RtpCodecCapability[]>([
		{
			kind: 'audio',
			mimeType: 'audio/opus',
			clockRate: 48000,
			channels: 2,
			parameters: {
				useinbandfec: 1,
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
};

beforeEach(async () => {
	ctx.worker = await mediasoup.createWorker();
	ctx.router = await ctx.worker.createRouter({ mediaCodecs: ctx.mediaCodecs });
});

afterEach(async () => {
	ctx.worker?.close();

	if (ctx.worker?.subprocessClosed === false) {
		await enhancedOnce<WorkerEvents>(ctx.worker, 'subprocessclose');
	}
});

test('router.createWebRtcTransport() succeeds', async () => {
	const onObserverNewTransport = jest.fn();

	ctx.router!.observer.once('newtransport', onObserverNewTransport);

	const webRtcTransport = await ctx.router!.createWebRtcTransport({
		listenInfos: [
			{
				protocol: 'udp',
				ip: '127.0.0.1',
				announcedAddress: '9.9.9.1',
				portRange: { min: 2000, max: 3000 },
			},
			{
				protocol: 'tcp',
				ip: '127.0.0.1',
				announcedAddress: '9.9.9.1',
				portRange: { min: 2000, max: 3000 },
			},
			{
				protocol: 'udp',
				ip: '0.0.0.0',
				announcedAddress: 'foo1.bar.org',
				portRange: { min: 2000, max: 3000 },
			},
			{
				protocol: 'tcp',
				ip: '0.0.0.0',
				announcedAddress: 'foo2.bar.org',
				portRange: { min: 2000, max: 3000 },
			},
			{
				protocol: 'udp',
				ip: '127.0.0.1',
				announcedAddress: undefined,
				portRange: { min: 2000, max: 3000 },
			},
			{
				protocol: 'tcp',
				ip: '127.0.0.1',
				announcedAddress: undefined,
				portRange: { min: 2000, max: 3000 },
			},
		],
		enableTcp: true,
		preferUdp: true,
		enableSctp: true,
		numSctpStreams: { OS: 2048, MIS: 2048 },
		maxSctpMessageSize: 1000000,
		appData: { foo: 'bar' },
	});

	await expect(ctx.router!.dump()).resolves.toMatchObject({
		transportIds: [webRtcTransport.id],
	});

	expect(onObserverNewTransport).toHaveBeenCalledTimes(1);
	expect(onObserverNewTransport).toHaveBeenCalledWith(webRtcTransport);
	expect(typeof webRtcTransport.id).toBe('string');
	expect(webRtcTransport.closed).toBe(false);
	expect(webRtcTransport.appData).toEqual({ foo: 'bar' });
	expect(webRtcTransport.iceRole).toBe('controlled');
	expect(typeof webRtcTransport.iceParameters).toBe('object');
	expect(webRtcTransport.iceParameters.iceLite).toBe(true);
	expect(typeof webRtcTransport.iceParameters.usernameFragment).toBe('string');
	expect(typeof webRtcTransport.iceParameters.password).toBe('string');
	expect(webRtcTransport.sctpParameters).toMatchObject({
		port: 5000,
		OS: 2048,
		MIS: 2048,
		maxMessageSize: 1000000,
	});
	expect(Array.isArray(webRtcTransport.iceCandidates)).toBe(true);
	expect(webRtcTransport.iceCandidates.length).toBe(6);

	const iceCandidates = webRtcTransport.iceCandidates;

	expect(iceCandidates[0].ip).toBe('9.9.9.1');
	expect(iceCandidates[0].protocol).toBe('udp');
	expect(iceCandidates[0].type).toBe('host');
	expect(iceCandidates[0].tcpType).toBeUndefined();
	expect(iceCandidates[1].ip).toBe('9.9.9.1');
	expect(iceCandidates[1].protocol).toBe('tcp');
	expect(iceCandidates[1].type).toBe('host');
	expect(iceCandidates[1].tcpType).toBe('passive');
	expect(iceCandidates[2].ip).toBe('foo1.bar.org');
	expect(iceCandidates[2].protocol).toBe('udp');
	expect(iceCandidates[2].type).toBe('host');
	expect(iceCandidates[2].tcpType).toBeUndefined();
	expect(iceCandidates[3].ip).toBe('foo2.bar.org');
	expect(iceCandidates[3].protocol).toBe('tcp');
	expect(iceCandidates[3].type).toBe('host');
	expect(iceCandidates[3].tcpType).toBe('passive');
	expect(iceCandidates[4].ip).toBe('127.0.0.1');
	expect(iceCandidates[4].protocol).toBe('udp');
	expect(iceCandidates[4].type).toBe('host');
	expect(iceCandidates[4].tcpType).toBeUndefined();
	expect(iceCandidates[5].ip).toBe('127.0.0.1');
	expect(iceCandidates[5].protocol).toBe('tcp');
	expect(iceCandidates[5].type).toBe('host');
	expect(iceCandidates[5].tcpType).toBe('passive');
	expect(iceCandidates[0].priority).toBeGreaterThan(iceCandidates[1].priority);
	expect(iceCandidates[1].priority).toBeGreaterThan(iceCandidates[2].priority);
	expect(iceCandidates[2].priority).toBeGreaterThan(iceCandidates[3].priority);
	expect(iceCandidates[3].priority).toBeGreaterThan(iceCandidates[4].priority);
	expect(iceCandidates[4].priority).toBeGreaterThan(iceCandidates[5].priority);

	expect(webRtcTransport.iceState).toBe('new');
	expect(webRtcTransport.iceSelectedTuple).toBeUndefined();
	expect(typeof webRtcTransport.dtlsParameters).toBe('object');
	expect(Array.isArray(webRtcTransport.dtlsParameters.fingerprints)).toBe(true);
	expect(webRtcTransport.dtlsParameters.role).toBe('auto');
	expect(webRtcTransport.dtlsState).toBe('new');
	expect(webRtcTransport.dtlsRemoteCert).toBeUndefined();
	expect(webRtcTransport.sctpState).toBe('new');

	const dump = await webRtcTransport.dump();

	expect(dump.id).toBe(webRtcTransport.id);
	expect(dump.direct).toBe(false);
	expect(dump.producerIds).toEqual([]);
	expect(dump.consumerIds).toEqual([]);
	expect(dump.iceRole).toBe(webRtcTransport.iceRole);
	expect(dump.iceParameters).toEqual(webRtcTransport.iceParameters);
	expect(dump.iceCandidates).toEqual(webRtcTransport.iceCandidates);
	expect(dump.iceState).toBe(webRtcTransport.iceState);
	expect(dump.iceSelectedTuple).toEqual(webRtcTransport.iceSelectedTuple);
	expect(dump.dtlsParameters).toEqual(webRtcTransport.dtlsParameters);
	expect(dump.dtlsState).toBe(webRtcTransport.dtlsState);
	expect(dump.sctpParameters).toEqual(webRtcTransport.sctpParameters);
	expect(dump.sctpState).toBe(webRtcTransport.sctpState);
	expect(dump.recvRtpHeaderExtensions).toBeDefined();
	expect(typeof dump.rtpListener).toBe('object');

	webRtcTransport.close();

	expect(webRtcTransport.closed).toBe(true);
}, 2000);

test('router.createWebRtcTransport() with deprecated listenIps succeeds', async () => {
	const webRtcTransport = await ctx.router!.createWebRtcTransport({
		listenIps: [{ ip: '127.0.0.1', announcedIp: undefined }],
		enableUdp: true,
		enableTcp: true,
		preferUdp: false,
		initialAvailableOutgoingBitrate: 1000000,
	});

	expect(Array.isArray(webRtcTransport.iceCandidates)).toBe(true);
	expect(webRtcTransport.iceCandidates.length).toBe(2);

	const iceCandidates = webRtcTransport.iceCandidates;

	expect(iceCandidates[0].ip).toBe('127.0.0.1');
	expect(iceCandidates[0].protocol).toBe('udp');
	expect(iceCandidates[0].type).toBe('host');
	expect(iceCandidates[0].tcpType).toBeUndefined();
	expect(iceCandidates[1].ip).toBe('127.0.0.1');
	expect(iceCandidates[1].protocol).toBe('tcp');
	expect(iceCandidates[1].type).toBe('host');
	expect(iceCandidates[1].tcpType).toBe('passive');
	expect(iceCandidates[0].priority).toBeGreaterThan(iceCandidates[1].priority);
}, 2000);

test('router.createWebRtcTransport() with fixed port succeeds', async () => {
	const port = await pickPort({
		type: 'tcp',
		ip: '127.0.0.1',
		reserveTimeout: 0,
	});
	const webRtcTransport = await ctx.router!.createWebRtcTransport({
		listenInfos: [
			// NOTE: udpReusePort flag will be ignored since protocol is TCP.
			{ protocol: 'tcp', ip: '127.0.0.1', port, flags: { udpReusePort: true } },
		],
	});

	expect(webRtcTransport.iceCandidates[0].port).toEqual(port);
}, 2000);

test('router.createWebRtcTransport() with portRange succeeds', async () => {
	const portRange = { min: 11111, max: 11112 };

	const webRtcTransport1 = await ctx.router!.createWebRtcTransport({
		listenInfos: [{ protocol: 'udp', ip: '127.0.0.1', portRange }],
	});

	const iceCandidate1 = webRtcTransport1.iceCandidates[0];

	expect(iceCandidate1.ip).toBe('127.0.0.1');
	expect(
		iceCandidate1.port >= portRange.min && iceCandidate1.port <= portRange.max
	).toBe(true);
	expect(iceCandidate1.protocol).toBe('udp');

	const webRtcTransport2 = await ctx.router!.createWebRtcTransport({
		listenInfos: [{ protocol: 'udp', ip: '127.0.0.1', portRange }],
	});

	const iceCandidate2 = webRtcTransport2.iceCandidates[0];

	expect(iceCandidate2.ip).toBe('127.0.0.1');
	expect(
		iceCandidate1.port >= portRange.min && iceCandidate1.port <= portRange.max
	).toBe(true);
	expect(iceCandidate2.protocol).toBe('udp');

	// No more available ports so it must fail.
	await expect(
		ctx.router!.createWebRtcTransport({
			listenInfos: [{ protocol: 'udp', ip: '127.0.0.1', portRange }],
		})
	).rejects.toThrow(Error);
}, 2000);

test('router.createWebRtcTransport() with wrong arguments rejects with TypeError', async () => {
	// @ts-expect-error --- Testing purposes.
	await expect(ctx.router!.createWebRtcTransport({})).rejects.toThrow(
		TypeError
	);

	await expect(
		ctx.router!.createWebRtcTransport({
			listenInfos: [
				{
					protocol: 'udp',
					ip: '127.0.0.1',
					portRange: { min: 4000, max: 3000 },
				},
			],
		})
	).rejects.toThrow(TypeError);

	await expect(
		// @ts-expect-error --- Testing purposes.
		ctx.router!.createWebRtcTransport({ listenIps: [123] })
	).rejects.toThrow(TypeError);

	await expect(
		// @ts-expect-error --- Testing purposes.
		ctx.router!.createWebRtcTransport({ listenInfos: '127.0.0.1' })
	).rejects.toThrow(TypeError);

	await expect(
		// @ts-expect-error --- Testing purposes.
		ctx.router!.createWebRtcTransport({ listenIps: '127.0.0.1' })
	).rejects.toThrow(TypeError);

	await expect(
		ctx.router!.createWebRtcTransport({
			listenIps: ['127.0.0.1'],
			// @ts-expect-error --- Testing purposes.
			appData: 'NOT-AN-OBJECT',
		})
	).rejects.toThrow(TypeError);

	await expect(
		ctx.router!.createWebRtcTransport({
			listenIps: ['127.0.0.1'],
			enableSctp: true,
			// @ts-expect-error --- Testing purposes.
			numSctpStreams: 'foo',
		})
	).rejects.toThrow(TypeError);
}, 2000);

test('router.createWebRtcTransport() with non bindable IP rejects with Error', async () => {
	await expect(
		ctx.router!.createWebRtcTransport({
			listenInfos: [
				{ protocol: 'udp', ip: '8.8.8.8', portRange: { min: 2000, max: 3000 } },
			],
		})
	).rejects.toThrow(Error);
}, 2000);

test('webRtcTransport.getStats() succeeds', async () => {
	const webRtcTransport = await ctx.router!.createWebRtcTransport({
		listenInfos: [
			{
				protocol: 'udp',
				ip: '127.0.0.1',
				announcedAddress: '9.9.9.1',
				portRange: { min: 2000, max: 3000 },
			},
		],
	});

	const stats = await webRtcTransport.getStats();

	expect(Array.isArray(stats)).toBe(true);
	expect(stats.length).toBe(1);
	expect(stats[0].type).toBe('webrtc-transport');
	expect(stats[0].transportId).toBe(webRtcTransport.id);
	expect(typeof stats[0].timestamp).toBe('number');
	expect(stats[0].iceRole).toBe('controlled');
	expect(stats[0].iceState).toBe('new');
	expect(stats[0].dtlsState).toBe('new');
	expect(stats[0].sctpState).toBeUndefined();
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
	expect(stats[0].iceSelectedTuple).toBeUndefined();
	expect(stats[0].maxIncomingBitrate).toBeUndefined();
}, 2000);

test('webRtcTransport.connect() succeeds', async () => {
	const webRtcTransport = await ctx.router!.createWebRtcTransport({
		listenInfos: [
			{
				protocol: 'udp',
				ip: '127.0.0.1',
				announcedAddress: '9.9.9.1',
				portRange: { min: 2000, max: 3000 },
			},
		],
	});

	const dtlsRemoteParameters: mediasoup.types.DtlsParameters = {
		fingerprints: [
			{
				algorithm: 'sha-256',
				value:
					'82:5A:68:3D:36:C3:0A:DE:AF:E7:32:43:D2:88:83:57:AC:2D:65:E5:80:C4:B6:FB:AF:1A:A0:21:9F:6D:0C:AD',
			},
		],
		role: 'client',
	};

	await expect(
		webRtcTransport.connect({
			dtlsParameters: dtlsRemoteParameters,
		})
	).resolves.toBeUndefined();

	// Must fail if connected.
	await expect(
		webRtcTransport.connect({
			dtlsParameters: dtlsRemoteParameters,
		})
	).rejects.toThrow(Error);

	expect(webRtcTransport.dtlsParameters.role).toBe('server');
}, 2000);

test('webRtcTransport.connect() with wrong arguments rejects with TypeError', async () => {
	const webRtcTransport = await ctx.router!.createWebRtcTransport({
		listenInfos: [
			{
				protocol: 'udp',
				ip: '127.0.0.1',
				announcedAddress: '9.9.9.1',
				portRange: { min: 2000, max: 3000 },
			},
		],
	});

	let dtlsRemoteParameters: mediasoup.types.DtlsParameters;

	// @ts-expect-error --- Testing purposes.
	await expect(webRtcTransport.connect({})).rejects.toThrow(TypeError);

	dtlsRemoteParameters = {
		fingerprints: [
			{
				// @ts-expect-error --- Testing purposes..
				algorithm: 'sha-256000',
				value:
					'82:5A:68:3D:36:C3:0A:DE:AF:E7:32:43:D2:88:83:57:AC:2D:65:E5:80:C4:B6:FB:AF:1A:A0:21:9F:6D:0C:AD',
			},
		],
		role: 'client',
	};

	await expect(
		webRtcTransport.connect({ dtlsParameters: dtlsRemoteParameters })
	).rejects.toThrow(TypeError);

	dtlsRemoteParameters = {
		fingerprints: [
			{
				algorithm: 'sha-256',
				value:
					'82:5A:68:3D:36:C3:0A:DE:AF:E7:32:43:D2:88:83:57:AC:2D:65:E5:80:C4:B6:FB:AF:1A:A0:21:9F:6D:0C:AD',
			},
		],
		// @ts-expect-error --- Testing purposes.
		role: 'chicken',
	};

	await expect(
		webRtcTransport.connect({ dtlsParameters: dtlsRemoteParameters })
	).rejects.toThrow(TypeError);

	dtlsRemoteParameters = {
		fingerprints: [],
		role: 'client',
	};

	await expect(
		webRtcTransport.connect({ dtlsParameters: dtlsRemoteParameters })
	).rejects.toThrow(TypeError);

	await expect(
		webRtcTransport.connect({ dtlsParameters: dtlsRemoteParameters })
	).rejects.toThrow(TypeError);

	expect(webRtcTransport.dtlsParameters.role).toBe('auto');
}, 2000);

test('webRtcTransport.setMaxIncomingBitrate() succeeds', async () => {
	const webRtcTransport = await ctx.router!.createWebRtcTransport({
		listenInfos: [
			{
				protocol: 'udp',
				ip: '127.0.0.1',
				announcedAddress: '9.9.9.1',
				portRange: { min: 2000, max: 3000 },
			},
		],
	});

	await expect(
		webRtcTransport.setMaxIncomingBitrate(1000000)
	).resolves.toBeUndefined();

	// Remove limit.
	await expect(
		webRtcTransport.setMaxIncomingBitrate(0)
	).resolves.toBeUndefined();
}, 2000);

test('webRtcTransport.setMaxOutgoingBitrate() succeeds', async () => {
	const webRtcTransport = await ctx.router!.createWebRtcTransport({
		listenInfos: [
			{ protocol: 'udp', ip: '127.0.0.1', portRange: { min: 2000, max: 3000 } },
		],
	});

	await expect(
		webRtcTransport.setMaxOutgoingBitrate(2000000)
	).resolves.toBeUndefined();

	// Remove limit.
	await expect(
		webRtcTransport.setMaxOutgoingBitrate(0)
	).resolves.toBeUndefined();
}, 2000);

test('webRtcTransport.setMinOutgoingBitrate() succeeds', async () => {
	const webRtcTransport = await ctx.router!.createWebRtcTransport({
		listenInfos: [
			{ protocol: 'udp', ip: '127.0.0.1', portRange: { min: 2000, max: 3000 } },
		],
	});

	await expect(
		webRtcTransport.setMinOutgoingBitrate(100000)
	).resolves.toBeUndefined();

	// Remove limit.
	await expect(
		webRtcTransport.setMinOutgoingBitrate(0)
	).resolves.toBeUndefined();
}, 2000);

test('webRtcTransport.setMaxOutgoingBitrate() fails if value is lower than current min limit', async () => {
	const webRtcTransport = await ctx.router!.createWebRtcTransport({
		listenInfos: [
			{ protocol: 'udp', ip: '127.0.0.1', portRange: { min: 2000, max: 3000 } },
		],
	});

	await expect(
		webRtcTransport.setMinOutgoingBitrate(3000000)
	).resolves.toBeUndefined();

	await expect(webRtcTransport.setMaxOutgoingBitrate(2000000)).rejects.toThrow(
		Error
	);

	// Remove limit.
	await expect(
		webRtcTransport.setMinOutgoingBitrate(0)
	).resolves.toBeUndefined();
}, 2000);

test('webRtcTransport.setMinOutgoingBitrate() fails if value is higher than current max limit', async () => {
	const webRtcTransport = await ctx.router!.createWebRtcTransport({
		listenInfos: [
			{ protocol: 'udp', ip: '127.0.0.1', portRange: { min: 2000, max: 3000 } },
		],
	});

	await expect(
		webRtcTransport.setMaxOutgoingBitrate(2000000)
	).resolves.toBeUndefined();

	await expect(webRtcTransport.setMinOutgoingBitrate(3000000)).rejects.toThrow(
		Error
	);

	// Remove limit.
	await expect(
		webRtcTransport.setMaxOutgoingBitrate(0)
	).resolves.toBeUndefined();
}, 2000);

test('webRtcTransport.restartIce() succeeds', async () => {
	const webRtcTransport = await ctx.router!.createWebRtcTransport({
		listenInfos: [
			{ protocol: 'udp', ip: '127.0.0.1', portRange: { min: 2000, max: 3000 } },
		],
	});

	const previousIceUsernameFragment =
		webRtcTransport.iceParameters.usernameFragment;
	const previousIcePassword = webRtcTransport.iceParameters.password;

	await expect(webRtcTransport.restartIce()).resolves.toMatchObject({
		usernameFragment: expect.any(String),
		password: expect.any(String),
		iceLite: true,
	});

	expect(typeof webRtcTransport.iceParameters.usernameFragment).toBe('string');
	expect(typeof webRtcTransport.iceParameters.password).toBe('string');
	expect(webRtcTransport.iceParameters.usernameFragment).not.toBe(
		previousIceUsernameFragment
	);
	expect(webRtcTransport.iceParameters.password).not.toBe(previousIcePassword);
}, 2000);

test('transport.enableTraceEvent() succeed', async () => {
	const webRtcTransport = await ctx.router!.createWebRtcTransport({
		listenInfos: [
			{ protocol: 'udp', ip: '127.0.0.1', portRange: { min: 2000, max: 3000 } },
		],
	});

	// @ts-expect-error --- Testing purposes.
	await webRtcTransport.enableTraceEvent(['foo', 'probation']);
	await expect(webRtcTransport.dump()).resolves.toMatchObject({
		traceEventTypes: ['probation'],
	});

	await webRtcTransport.enableTraceEvent();
	await expect(webRtcTransport.dump()).resolves.toMatchObject({
		traceEventTypes: [],
	});

	// @ts-expect-error --- Testing purposes.
	await webRtcTransport.enableTraceEvent(['probation', 'FOO', 'bwe', 'BAR']);
	await expect(webRtcTransport.dump()).resolves.toMatchObject({
		traceEventTypes: ['probation', 'bwe'],
	});

	await webRtcTransport.enableTraceEvent();
	await expect(webRtcTransport.dump()).resolves.toMatchObject({
		traceEventTypes: [],
	});
}, 2000);

test('transport.enableTraceEvent() with wrong arguments rejects with TypeError', async () => {
	const webRtcTransport = await ctx.router!.createWebRtcTransport({
		listenInfos: [
			{ protocol: 'udp', ip: '127.0.0.1', portRange: { min: 2000, max: 3000 } },
		],
	});

	// @ts-expect-error --- Testing purposes.
	await expect(webRtcTransport.enableTraceEvent(123)).rejects.toThrow(
		TypeError
	);

	// @ts-expect-error --- Testing purposes.
	await expect(webRtcTransport.enableTraceEvent('probation')).rejects.toThrow(
		TypeError
	);

	await expect(
		// @ts-expect-error --- Testing purposes.
		webRtcTransport.enableTraceEvent(['probation', 123.123])
	).rejects.toThrow(TypeError);
}, 2000);

test('WebRtcTransport events succeed', async () => {
	const webRtcTransport = await ctx.router!.createWebRtcTransport({
		listenInfos: [
			{ protocol: 'udp', ip: '127.0.0.1', portRange: { min: 2000, max: 3000 } },
		],
	});

	// API not exposed in the interface.
	const channel = (webRtcTransport as WebRtcTransportImpl).channelForTesting;
	const onIceStateChange = jest.fn();

	webRtcTransport.on('icestatechange', onIceStateChange);

	// Simulate a 'iceselectedtuplechange' notification coming through the
	// channel.
	const builder = new flatbuffers.Builder();
	const iceStateChangeNotification =
		new FbsWebRtcTransport.IceStateChangeNotificationT(
			FbsWebRtcTransport.IceState.COMPLETED
		);

	let notificationOffset = Notification.createNotification(
		builder,
		builder.createString(webRtcTransport.id),
		Event.WEBRTCTRANSPORT_ICE_STATE_CHANGE,
		NotificationBody.WebRtcTransport_IceStateChangeNotification,
		iceStateChangeNotification.pack(builder)
	);

	builder.finish(notificationOffset);

	let notification = Notification.getRootAsNotification(
		new flatbuffers.ByteBuffer(builder.asUint8Array())
	);

	channel.emit(
		webRtcTransport.id,
		Event.WEBRTCTRANSPORT_ICE_STATE_CHANGE,
		notification
	);

	expect(onIceStateChange).toHaveBeenCalledTimes(1);
	expect(onIceStateChange).toHaveBeenCalledWith('completed');
	expect(webRtcTransport.iceState).toBe('completed');

	builder.clear();

	const onIceSelectedTuple = jest.fn();
	const iceSelectedTuple: TransportTuple = {
		// @deprecated Use localAddress.
		localIp: '1.1.1.1',
		localAddress: '1.1.1.1',
		localPort: 1111,
		remoteIp: '2.2.2.2',
		remotePort: 2222,
		protocol: 'udp',
	};

	webRtcTransport.on('iceselectedtuplechange', onIceSelectedTuple);

	// Simulate a 'icestatechange' notification coming through the channel.
	const iceSelectedTupleChangeNotification =
		new FbsWebRtcTransport.IceSelectedTupleChangeNotificationT(
			new FbsTransport.TupleT(
				iceSelectedTuple.localAddress,
				iceSelectedTuple.localPort,
				iceSelectedTuple.remoteIp,
				iceSelectedTuple.remotePort,
				serializeProtocol(iceSelectedTuple.protocol)
			)
		);

	notificationOffset = Notification.createNotification(
		builder,
		builder.createString(webRtcTransport.id),
		Event.WEBRTCTRANSPORT_ICE_SELECTED_TUPLE_CHANGE,
		NotificationBody.WebRtcTransport_IceSelectedTupleChangeNotification,
		iceSelectedTupleChangeNotification.pack(builder)
	);

	builder.finish(notificationOffset);

	notification = Notification.getRootAsNotification(
		new flatbuffers.ByteBuffer(builder.asUint8Array())
	);

	channel.emit(
		webRtcTransport.id,
		Event.WEBRTCTRANSPORT_ICE_SELECTED_TUPLE_CHANGE,
		notification
	);

	expect(onIceSelectedTuple).toHaveBeenCalledTimes(1);
	expect(onIceSelectedTuple).toHaveBeenCalledWith(iceSelectedTuple);
	expect(webRtcTransport.iceSelectedTuple).toEqual(iceSelectedTuple);

	builder.clear();

	const onDtlsStateChange = jest.fn();

	webRtcTransport.on('dtlsstatechange', onDtlsStateChange);

	// Simulate a 'dtlsstatechange' notification coming through the channel.
	const dtlsStateChangeNotification =
		new FbsWebRtcTransport.DtlsStateChangeNotificationT(
			FbsWebRtcTransport.DtlsState.CONNECTING
		);

	notificationOffset = Notification.createNotification(
		builder,
		builder.createString(webRtcTransport.id),
		Event.WEBRTCTRANSPORT_DTLS_STATE_CHANGE,
		NotificationBody.WebRtcTransport_DtlsStateChangeNotification,
		dtlsStateChangeNotification.pack(builder)
	);

	builder.finish(notificationOffset);

	notification = Notification.getRootAsNotification(
		new flatbuffers.ByteBuffer(builder.asUint8Array())
	);

	channel.emit(
		webRtcTransport.id,
		Event.WEBRTCTRANSPORT_DTLS_STATE_CHANGE,
		notification
	);

	expect(onDtlsStateChange).toHaveBeenCalledTimes(1);
	expect(onDtlsStateChange).toHaveBeenCalledWith('connecting');
	expect(webRtcTransport.dtlsState).toBe('connecting');
}, 2000);

test('WebRtcTransport methods reject if closed', async () => {
	const webRtcTransport = await ctx.router!.createWebRtcTransport({
		listenInfos: [
			{ protocol: 'udp', ip: '127.0.0.1', portRange: { min: 2000, max: 3000 } },
		],
	});

	const onObserverClose = jest.fn();

	webRtcTransport.observer.once('close', onObserverClose);
	webRtcTransport.close();

	expect(onObserverClose).toHaveBeenCalledTimes(1);
	expect(webRtcTransport.closed).toBe(true);
	expect(webRtcTransport.iceState).toBe('closed');
	expect(webRtcTransport.iceSelectedTuple).toBeUndefined();
	expect(webRtcTransport.dtlsState).toBe('closed');
	expect(webRtcTransport.sctpState).toBeUndefined();

	await expect(webRtcTransport.dump()).rejects.toThrow(Error);

	await expect(webRtcTransport.getStats()).rejects.toThrow(Error);

	// @ts-expect-error --- Testing purposes.
	await expect(webRtcTransport.connect({})).rejects.toThrow(Error);

	await expect(webRtcTransport.setMaxIncomingBitrate(200000)).rejects.toThrow(
		Error
	);

	await expect(webRtcTransport.setMaxOutgoingBitrate(200000)).rejects.toThrow(
		Error
	);

	await expect(webRtcTransport.setMinOutgoingBitrate(100000)).rejects.toThrow(
		Error
	);

	await expect(webRtcTransport.restartIce()).rejects.toThrow(Error);
}, 2000);

test('WebRtcTransport emits "routerclose" if Router is closed', async () => {
	const webRtcTransport = await ctx.router!.createWebRtcTransport({
		listenIps: ['127.0.0.1'],
		enableSctp: true,
	});

	const onObserverClose = jest.fn();

	webRtcTransport.observer.once('close', onObserverClose);

	const promise = enhancedOnce<WebRtcTransportEvents>(
		webRtcTransport,
		'routerclose'
	);

	ctx.router!.close();
	await promise;

	expect(onObserverClose).toHaveBeenCalledTimes(1);
	expect(webRtcTransport.closed).toBe(true);
	expect(webRtcTransport.iceState).toBe('closed');
	expect(webRtcTransport.iceSelectedTuple).toBeUndefined();
	expect(webRtcTransport.dtlsState).toBe('closed');
	expect(webRtcTransport.sctpState).toBe('closed');
}, 2000);

test('WebRtcTransport emits "routerclose" if Worker is closed', async () => {
	const webRtcTransport = await ctx.router!.createWebRtcTransport({
		listenInfos: [
			{ protocol: 'udp', ip: '127.0.0.1', portRange: { min: 2000, max: 3000 } },
		],
	});

	const onObserverClose = jest.fn();

	webRtcTransport.observer.once('close', onObserverClose);

	const promise = enhancedOnce<WebRtcTransportEvents>(
		webRtcTransport,
		'routerclose'
	);

	ctx.worker!.close();
	await promise;

	expect(onObserverClose).toHaveBeenCalledTimes(1);
	expect(webRtcTransport.closed).toBe(true);
	expect(webRtcTransport.iceState).toBe('closed');
	expect(webRtcTransport.iceSelectedTuple).toBeUndefined();
	expect(webRtcTransport.dtlsState).toBe('closed');
	expect(webRtcTransport.sctpState).toBeUndefined();
}, 2000);
