import * as os from 'node:os';
import { pickPort } from 'pick-port';
import * as mediasoup from '../';
import { enhancedOnce } from '../enhancedEvents';
import { WorkerEvents, PlainTransportEvents } from '../types';
import * as utils from '../utils';

const IS_WINDOWS = os.platform() === 'win32';

type TestContext = {
	mediaCodecs: mediasoup.types.RtpCodecCapability[];
	worker?: mediasoup.types.Worker;
	router?: mediasoup.types.Router;
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
			rtcpFeedback: [], // Will be ignored.
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

test('router.createPlainTransport() succeeds', async () => {
	const plainTransport = await ctx.router!.createPlainTransport({
		listenInfo: {
			protocol: 'udp',
			ip: '127.0.0.1',
			portRange: { min: 2000, max: 3000 },
		},
	});

	await expect(ctx.router!.dump()).resolves.toMatchObject({
		transportIds: [plainTransport.id],
	});

	const onObserverNewTransport = jest.fn();

	ctx.router!.observer.once('newtransport', onObserverNewTransport);

	// Create a separate transport here.
	const plainTransport2 = await ctx.router!.createPlainTransport({
		listenInfo: {
			protocol: 'udp',
			ip: '127.0.0.1',
			announcedAddress: '9.9.9.1',
			portRange: { min: 2000, max: 3000 },
		},
		enableSctp: true,
		appData: { foo: 'bar' },
	});

	expect(onObserverNewTransport).toHaveBeenCalledTimes(1);
	expect(onObserverNewTransport).toHaveBeenCalledWith(plainTransport2);
	expect(typeof plainTransport2.id).toBe('string');
	expect(plainTransport2.closed).toBe(false);
	expect(plainTransport2.appData).toEqual({ foo: 'bar' });
	expect(typeof plainTransport2.tuple).toBe('object');
	// @deprecated Use tuple.localAddress instead.
	expect(plainTransport2.tuple.localIp).toBe('9.9.9.1');
	expect(plainTransport2.tuple.localAddress).toBe('9.9.9.1');
	expect(typeof plainTransport2.tuple.localPort).toBe('number');
	expect(plainTransport2.tuple.protocol).toBe('udp');
	expect(plainTransport2.rtcpTuple).toBeUndefined();
	expect(plainTransport2.sctpParameters).toMatchObject({
		port: 5000,
		OS: 1024,
		MIS: 1024,
		maxMessageSize: 262144,
	});
	expect(plainTransport2.sctpState).toBe('new');
	expect(plainTransport2.srtpParameters).toBeUndefined();

	const dump1 = await plainTransport2.dump();

	expect(dump1.id).toBe(plainTransport2.id);
	expect(dump1.direct).toBe(false);
	expect(dump1.producerIds).toEqual([]);
	expect(dump1.consumerIds).toEqual([]);
	expect(dump1.tuple).toEqual(plainTransport2.tuple);
	expect(dump1.rtcpTuple).toEqual(plainTransport2.rtcpTuple);
	expect(dump1.sctpParameters).toEqual(plainTransport2.sctpParameters);
	expect(dump1.sctpState).toBe('new');
	expect(dump1.recvRtpHeaderExtensions).toBeDefined();
	expect(typeof dump1.rtpListener).toBe('object');

	plainTransport2.close();
	expect(plainTransport2.closed).toBe(true);

	const anotherTransport = await ctx.router!.createPlainTransport({
		listenIp: '127.0.0.1',
	});

	expect(typeof anotherTransport).toBe('object');

	const rtpPort = await pickPort({
		type: 'udp',
		ip: '127.0.0.1',
		reserveTimeout: 0,
	});
	const rtcpPort = await pickPort({
		type: 'udp',
		ip: '127.0.0.1',
		reserveTimeout: 0,
	});
	const transport2 = await ctx.router!.createPlainTransport({
		listenInfo: { protocol: 'udp', ip: '127.0.0.1', port: rtpPort },
		rtcpListenInfo: { protocol: 'udp', ip: '127.0.0.1', port: rtcpPort },
	});

	expect(typeof transport2.id).toBe('string');
	expect(transport2.closed).toBe(false);
	expect(transport2.appData).toEqual({});
	expect(typeof transport2.tuple).toBe('object');
	// @deprecated Use tuple.localAddress instead.
	expect(transport2.tuple.localIp).toBe('127.0.0.1');
	expect(transport2.tuple.localAddress).toBe('127.0.0.1');
	expect(transport2.tuple.localPort).toBe(rtpPort);
	expect(transport2.tuple.protocol).toBe('udp');
	expect(typeof transport2.rtcpTuple).toBe('object');
	// @deprecated Use tuple.localAddress instead.
	expect(transport2.rtcpTuple?.localIp).toBe('127.0.0.1');
	expect(transport2.rtcpTuple?.localAddress).toBe('127.0.0.1');
	expect(transport2.rtcpTuple?.localPort).toBe(rtcpPort);
	expect(transport2.rtcpTuple?.protocol).toBe('udp');
	expect(transport2.sctpParameters).toBeUndefined();
	expect(transport2.sctpState).toBeUndefined();

	const dump2 = await transport2.dump();

	expect(dump2.id).toBe(transport2.id);
	expect(dump2.direct).toBe(false);
	expect(dump2.tuple).toEqual(transport2.tuple);
	expect(dump2.rtcpTuple).toEqual(transport2.rtcpTuple);
	expect(dump2.sctpState).toBeUndefined();
}, 2000);

test('router.createPlainTransport() with wrong arguments rejects with TypeError', async () => {
	// @ts-expect-error --- Testing purposes.
	await expect(ctx.router!.createPlainTransport({})).rejects.toThrow(TypeError);

	await expect(
		ctx.router!.createPlainTransport({
			listenInfo: {
				protocol: 'udp',
				ip: '127.0.0.1',
				portRange: { min: 4000, max: 3000 },
			},
		})
	).rejects.toThrow(TypeError);

	await expect(
		ctx.router!.createPlainTransport({ listenIp: '123' })
	).rejects.toThrow(TypeError);

	await expect(
		// @ts-expect-error --- Testing purposes.
		ctx.router!.createPlainTransport({ listenIp: ['127.0.0.1'] })
	).rejects.toThrow(TypeError);

	await expect(
		ctx.router!.createPipeTransport({
			listenInfo: { protocol: 'tcp', ip: '127.0.0.1' },
		})
	).rejects.toThrow(TypeError);

	await expect(
		ctx.router!.createPlainTransport({
			listenInfo: { protocol: 'udp', ip: '127.0.0.1' },
			// @ts-expect-error --- Testing purposes.
			appData: 'NOT-AN-OBJECT',
		})
	).rejects.toThrow(TypeError);
}, 2000);

test('router.createPlainTransport() with enableSrtp succeeds', async () => {
	// Use default cryptoSuite: 'AES_CM_128_HMAC_SHA1_80'.
	const plainTransport = await ctx.router!.createPlainTransport({
		listenIp: '127.0.0.1',
		enableSrtp: true,
	});

	expect(typeof plainTransport.id).toBe('string');
	expect(typeof plainTransport.srtpParameters).toBe('object');
	expect(plainTransport.srtpParameters?.cryptoSuite).toBe(
		'AES_CM_128_HMAC_SHA1_80'
	);
	expect(plainTransport.srtpParameters?.keyBase64.length).toBe(40);

	// Missing srtpParameters.
	await expect(
		plainTransport.connect({
			ip: '127.0.0.2',
			port: 9999,
		})
	).rejects.toThrow(TypeError);

	// Invalid srtpParameters.
	await expect(
		plainTransport.connect({
			ip: '127.0.0.2',
			port: 9999,
			// @ts-expect-error --- Testing purposes.
			srtpParameters: 1,
		})
	).rejects.toThrow(TypeError);

	// Missing srtpParameters.cryptoSuite.
	await expect(
		plainTransport.connect({
			ip: '127.0.0.2',
			port: 9999,
			// @ts-expect-error --- Testing purposes.
			srtpParameters: {
				keyBase64: 'ZnQ3eWJraDg0d3ZoYzM5cXN1Y2pnaHU5NWxrZTVv',
			},
		})
	).rejects.toThrow(TypeError);

	// Missing srtpParameters.keyBase64.
	await expect(
		plainTransport.connect({
			ip: '127.0.0.2',
			port: 9999,
			// @ts-expect-error --- Testing purposes.
			srtpParameters: {
				cryptoSuite: 'AES_CM_128_HMAC_SHA1_80',
			},
		})
	).rejects.toThrow(TypeError);

	// Invalid srtpParameters.cryptoSuite.
	await expect(
		plainTransport.connect({
			ip: '127.0.0.2',
			port: 9999,
			srtpParameters: {
				// @ts-expect-error --- Testing purposes.
				cryptoSuite: 'FOO',
				keyBase64: 'ZnQ3eWJraDg0d3ZoYzM5cXN1Y2pnaHU5NWxrZTVv',
			},
		})
	).rejects.toThrow(TypeError);

	// Invalid srtpParameters.cryptoSuite.
	await expect(
		plainTransport.connect({
			ip: '127.0.0.2',
			port: 9999,
			srtpParameters: {
				// @ts-expect-error --- Testing purposes.
				cryptoSuite: 123,
				keyBase64: 'ZnQ3eWJraDg0d3ZoYzM5cXN1Y2pnaHU5NWxrZTVv',
			},
		})
	).rejects.toThrow(TypeError);

	// Invalid srtpParameters.keyBase64.
	await expect(
		plainTransport.connect({
			ip: '127.0.0.2',
			port: 9999,
			srtpParameters: {
				cryptoSuite: 'AES_CM_128_HMAC_SHA1_80',
				// @ts-expect-error --- Testing purposes.
				keyBase64: [],
			},
		})
	).rejects.toThrow(TypeError);

	// Valid srtpParameters. And let's update the crypto suite.
	await expect(
		plainTransport.connect({
			ip: '127.0.0.2',
			port: 9999,
			srtpParameters: {
				cryptoSuite: 'AEAD_AES_256_GCM',
				keyBase64:
					'YTdjcDBvY2JoMGY5YXNlNDc0eDJsdGgwaWRvNnJsamRrdG16aWVpZHphdHo=',
			},
		})
	).resolves.toBeUndefined();

	expect(plainTransport.srtpParameters?.cryptoSuite).toBe('AEAD_AES_256_GCM');
	expect(plainTransport.srtpParameters?.keyBase64.length).toBe(60);
}, 2000);

test('router.createPlainTransport() with non bindable IP rejects with Error', async () => {
	await expect(
		ctx.router!.createPlainTransport({ listenIp: '8.8.8.8' })
	).rejects.toThrow(Error);
}, 2000);

if (!IS_WINDOWS) {
	test('two transports binding to the same IP:port with udpReusePort flag succeed', async () => {
		const multicastIp = '224.0.0.1';
		const port = await pickPort({
			type: 'udp',
			ip: multicastIp,
			reserveTimeout: 0,
		});

		await expect(
			ctx.router!.createPlainTransport({
				listenInfo: {
					protocol: 'udp',
					ip: multicastIp,
					port: port,
					// NOTE: ipv6Only flag will be ignored since ip is IPv4.
					flags: { udpReusePort: true, ipv6Only: true },
				},
			})
		).resolves.toBeDefined();

		await expect(
			ctx.router!.createPlainTransport({
				listenInfo: {
					protocol: 'udp',
					ip: multicastIp,
					port: port,
					flags: { udpReusePort: true },
				},
			})
		).resolves.toBeDefined();
	}, 2000);

	test('two transports binding to the same IP:port without udpReusePort flag fail', async () => {
		const multicastIp = '224.0.0.1';
		const port = await pickPort({
			type: 'udp',
			ip: multicastIp,
			reserveTimeout: 0,
		});

		await expect(
			ctx.router!.createPlainTransport({
				listenInfo: {
					protocol: 'udp',
					ip: multicastIp,
					port: port,
					flags: { udpReusePort: false },
				},
			})
		).resolves.toBeDefined();

		await expect(
			ctx.router!.createPlainTransport({
				listenInfo: {
					protocol: 'udp',
					ip: multicastIp,
					port: port,
					flags: { udpReusePort: false },
				},
			})
		).rejects.toThrow();
	}, 2000);
}

test('plainTransport.getStats() succeeds', async () => {
	const plainTransport = await ctx.router!.createPlainTransport({
		listenIp: '127.0.0.1',
	});

	const stats = await plainTransport.getStats();

	expect(Array.isArray(stats)).toBe(true);
	expect(stats.length).toBe(1);
	expect(stats[0].type).toBe('plain-rtp-transport');
	expect(stats[0].transportId).toBe(plainTransport.id);
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
	expect(typeof stats[0].tuple).toBe('object');
	// @deprecated Use tuple.localAddress instead.
	expect(stats[0].tuple.localIp).toBe('127.0.0.1');
	expect(stats[0].tuple.localAddress).toBe('127.0.0.1');
	expect(typeof stats[0].tuple.localPort).toBe('number');
	expect(stats[0].tuple.protocol).toBe('udp');
	expect(stats[0].rtcpTuple).toBeUndefined();
}, 2000);

test('plainTransport.connect() succeeds', async () => {
	const plainTransport = await ctx.router!.createPlainTransport({
		listenIp: '127.0.0.1',
		rtcpMux: false,
	});

	await expect(
		plainTransport.connect({ ip: '1.2.3.4', port: 1234, rtcpPort: 1235 })
	).resolves.toBeUndefined();

	// Must fail if connected.
	await expect(
		plainTransport.connect({ ip: '1.2.3.4', port: 1234, rtcpPort: 1235 })
	).rejects.toThrow(Error);

	expect(plainTransport.tuple.remoteIp).toBe('1.2.3.4');
	expect(plainTransport.tuple.remotePort).toBe(1234);
	expect(plainTransport.tuple.protocol).toBe('udp');
	expect(plainTransport.rtcpTuple?.remoteIp).toBe('1.2.3.4');
	expect(plainTransport.rtcpTuple?.remotePort).toBe(1235);
	expect(plainTransport.rtcpTuple?.protocol).toBe('udp');
}, 2000);

test('plainTransport.connect() with wrong arguments rejects with TypeError', async () => {
	const plainTransport = await ctx.router!.createPlainTransport({
		listenIp: '127.0.0.1',
		rtcpMux: false,
	});

	// No SRTP enabled so passing srtpParameters must fail.
	await expect(
		plainTransport.connect({
			ip: '127.0.0.2',
			port: 9998,
			rtcpPort: 9999,
			srtpParameters: {
				cryptoSuite: 'AES_CM_128_HMAC_SHA1_80',
				keyBase64: 'ZnQ3eWJraDg0d3ZoYzM5cXN1Y2pnaHU5NWxrZTVv',
			},
		})
	).rejects.toThrow(TypeError);

	await expect(plainTransport.connect({})).rejects.toThrow(TypeError);

	await expect(plainTransport.connect({ ip: '::::1234' })).rejects.toThrow(
		TypeError
	);

	// Must fail because transport has rtcpMux: false so rtcpPort must be given
	// in connect().
	await expect(
		plainTransport.connect({
			ip: '127.0.0.1',
			port: 1234,
			// @ts-expect-error --- Testing purposes.
			__rtcpPort: 1235,
		})
	).rejects.toThrow(TypeError);

	await expect(
		plainTransport.connect({
			ip: '127.0.0.1',
			// @ts-expect-error --- Testing purposes.
			__port: 'chicken',
			rtcpPort: 1235,
		})
	).rejects.toThrow(TypeError);
}, 2000);

test('PlainTransport methods reject if closed', async () => {
	const plainTransport = await ctx.router!.createPlainTransport({
		listenIp: '127.0.0.1',
	});

	const onObserverClose = jest.fn();

	plainTransport.observer.once('close', onObserverClose);
	plainTransport.close();

	expect(onObserverClose).toHaveBeenCalledTimes(1);
	expect(plainTransport.closed).toBe(true);

	await expect(plainTransport.dump()).rejects.toThrow(Error);

	await expect(plainTransport.getStats()).rejects.toThrow(Error);

	await expect(plainTransport.connect({})).rejects.toThrow(Error);
}, 2000);

test('router.createPlainTransport() with fixed port succeeds', async () => {
	const port = await pickPort({
		type: 'udp',
		ip: '127.0.0.1',
		reserveTimeout: 0,
	});
	const plainTransport = await ctx.router!.createPlainTransport({
		listenInfo: { protocol: 'udp', ip: '127.0.0.1', port },
	});

	expect(plainTransport.tuple.localPort).toEqual(port);
}, 2000);

test('PlainTransport emits "routerclose" if Router is closed', async () => {
	const plainTransport = await ctx.router!.createPlainTransport({
		listenIp: '127.0.0.1',
	});

	const onObserverClose = jest.fn();

	plainTransport.observer.once('close', onObserverClose);

	const promise = enhancedOnce<PlainTransportEvents>(
		plainTransport,
		'routerclose'
	);

	ctx.router!.close();
	await promise;

	expect(onObserverClose).toHaveBeenCalledTimes(1);
	expect(plainTransport.closed).toBe(true);
}, 2000);

test('PlainTransport emits "routerclose" if Worker is closed', async () => {
	const plainTransport = await ctx.router!.createPlainTransport({
		listenIp: '127.0.0.1',
	});

	const onObserverClose = jest.fn();

	plainTransport.observer.once('close', onObserverClose);

	const promise = enhancedOnce<PlainTransportEvents>(
		plainTransport,
		'routerclose'
	);

	ctx.worker!.close();
	await promise;

	expect(onObserverClose).toHaveBeenCalledTimes(1);
	expect(plainTransport.closed).toBe(true);
}, 2000);
