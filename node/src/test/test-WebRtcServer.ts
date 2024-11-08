import { pickPort } from 'pick-port';
import * as mediasoup from '../';
import { enhancedOnce } from '../enhancedEvents';
import { WorkerImpl } from '../Worker';
import { WorkerEvents, WebRtcServerEvents } from '../types';
import { WebRtcServerImpl } from '../WebRtcServer';
import { RouterImpl } from '../Router';
import { InvalidStateError } from '../errors';

type TestContext = {
	worker?: mediasoup.types.WorkerInterface;
};

const ctx: TestContext = {};

beforeEach(async () => {
	ctx.worker = await mediasoup.createWorker();
});

afterEach(async () => {
	ctx.worker?.close();

	if (ctx.worker?.subprocessClosed === false) {
		await enhancedOnce<WorkerEvents>(ctx.worker, 'subprocessclose');
	}
});

test('worker.createWebRtcServer() succeeds', async () => {
	const onObserverNewWebRtcServer = jest.fn();

	ctx.worker!.observer.once('newwebrtcserver', onObserverNewWebRtcServer);

	const port1 = await pickPort({
		type: 'udp',
		ip: '127.0.0.1',
		reserveTimeout: 0,
	});
	const port2 = await pickPort({
		type: 'tcp',
		ip: '127.0.0.1',
		reserveTimeout: 0,
	});

	const webRtcServer = await ctx.worker!.createWebRtcServer<{ foo?: number }>({
		listenInfos: [
			{
				protocol: 'udp',
				ip: '127.0.0.1',
				port: port1,
			},
			{
				protocol: 'tcp',
				ip: '127.0.0.1',
				announcedAddress: 'foo.bar.org',
				port: port2,
			},
		],
		appData: { foo: 123 },
	});

	expect(onObserverNewWebRtcServer).toHaveBeenCalledTimes(1);
	expect(onObserverNewWebRtcServer).toHaveBeenCalledWith(webRtcServer);
	expect(typeof webRtcServer.id).toBe('string');
	expect(webRtcServer.closed).toBe(false);
	expect(webRtcServer.appData).toEqual({ foo: 123 });

	await expect(ctx.worker!.dump()).resolves.toMatchObject({
		pid: ctx.worker!.pid,
		webRtcServerIds: [webRtcServer.id],
		routerIds: [],
		channelMessageHandlers: {
			channelRequestHandlers: [webRtcServer.id],
			channelNotificationHandlers: [],
		},
	});

	await expect(webRtcServer.dump()).resolves.toMatchObject({
		id: webRtcServer.id,
		udpSockets: [{ ip: '127.0.0.1', port: port1 }],
		tcpServers: [{ ip: '127.0.0.1', port: port2 }],
		webRtcTransportIds: [],
		localIceUsernameFragments: [],
		tupleHashes: [],
	});

	// API not exposed in the interface.
	expect((ctx.worker! as WorkerImpl).webRtcServersForTesting.size).toBe(1);

	ctx.worker!.close();

	expect(webRtcServer.closed).toBe(true);

	// API not exposed in the interface.
	expect((ctx.worker! as WorkerImpl).webRtcServersForTesting.size).toBe(0);
}, 2000);

test('worker.createWebRtcServer() with portRange succeeds', async () => {
	const onObserverNewWebRtcServer = jest.fn();

	ctx.worker!.observer.once('newwebrtcserver', onObserverNewWebRtcServer);

	const port1 = await pickPort({
		type: 'udp',
		ip: '127.0.0.1',
		reserveTimeout: 0,
	});
	const port2 = await pickPort({
		type: 'udp',
		ip: '127.0.0.1',
		reserveTimeout: 0,
	});

	const webRtcServer = await ctx.worker!.createWebRtcServer({
		listenInfos: [
			{
				protocol: 'udp',
				ip: '127.0.0.1',
				portRange: { min: port1, max: port1 },
			},
			{
				protocol: 'tcp',
				ip: '127.0.0.1',
				announcedAddress: '1.2.3.4',
				portRange: { min: port2, max: port2 },
			},
		],
		appData: { foo: 123 },
	});

	expect(onObserverNewWebRtcServer).toHaveBeenCalledTimes(1);
	expect(onObserverNewWebRtcServer).toHaveBeenCalledWith(webRtcServer);
	expect(typeof webRtcServer.id).toBe('string');
	expect(webRtcServer.closed).toBe(false);
	expect(webRtcServer.appData).toEqual({ foo: 123 });

	await expect(ctx.worker!.dump()).resolves.toMatchObject({
		pid: ctx.worker!.pid,
		webRtcServerIds: [webRtcServer.id],
		routerIds: [],
		channelMessageHandlers: {
			channelRequestHandlers: [webRtcServer.id],
			channelNotificationHandlers: [],
		},
	});

	await expect(webRtcServer.dump()).resolves.toMatchObject({
		id: webRtcServer.id,
		udpSockets: [{ ip: '127.0.0.1', port: port1 }],
		tcpServers: [{ ip: '127.0.0.1', port: port2 }],
		webRtcTransportIds: [],
		localIceUsernameFragments: [],
		tupleHashes: [],
	});

	// API not exposed in the interface.
	expect((ctx.worker! as WorkerImpl).webRtcServersForTesting.size).toBe(1);

	ctx.worker!.close();

	expect(webRtcServer.closed).toBe(true);

	// API not exposed in the interface.
	expect((ctx.worker! as WorkerImpl).webRtcServersForTesting.size).toBe(0);
}, 2000);

test('worker.createWebRtcServer() without specifying port/portRange succeeds', async () => {
	const onObserverNewWebRtcServer = jest.fn();

	ctx.worker!.observer.once('newwebrtcserver', onObserverNewWebRtcServer);

	const webRtcServer = await ctx.worker!.createWebRtcServer({
		listenInfos: [
			{
				protocol: 'udp',
				ip: '127.0.0.1',
			},
			{
				protocol: 'tcp',
				ip: '127.0.0.1',
				announcedAddress: '1.2.3.4',
			},
		],
		appData: { foo: 123 },
	});

	expect(onObserverNewWebRtcServer).toHaveBeenCalledTimes(1);
	expect(onObserverNewWebRtcServer).toHaveBeenCalledWith(webRtcServer);
	expect(typeof webRtcServer.id).toBe('string');
	expect(webRtcServer.closed).toBe(false);
	expect(webRtcServer.appData).toEqual({ foo: 123 });

	await expect(ctx.worker!.dump()).resolves.toMatchObject({
		pid: ctx.worker!.pid,
		webRtcServerIds: [webRtcServer.id],
		routerIds: [],
		channelMessageHandlers: {
			channelRequestHandlers: [webRtcServer.id],
			channelNotificationHandlers: [],
		},
	});

	await expect(webRtcServer.dump()).resolves.toMatchObject({
		id: webRtcServer.id,
		udpSockets: [{ ip: '127.0.0.1', port: expect.any(Number) }],
		tcpServers: [{ ip: '127.0.0.1', port: expect.any(Number) }],
		webRtcTransportIds: [],
		localIceUsernameFragments: [],
		tupleHashes: [],
	});

	// API not exposed in the interface.
	expect((ctx.worker! as WorkerImpl).webRtcServersForTesting.size).toBe(1);

	ctx.worker!.close();

	expect(webRtcServer.closed).toBe(true);

	// API not exposed in the interface.
	expect((ctx.worker! as WorkerImpl).webRtcServersForTesting.size).toBe(0);
}, 2000);

test('worker.createWebRtcServer() with wrong arguments rejects with TypeError', async () => {
	// @ts-expect-error --- Testing purposes.
	await expect(ctx.worker!.createWebRtcServer({})).rejects.toThrow(TypeError);

	await expect(
		// @ts-expect-error --- Testing purposes.
		ctx.worker!.createWebRtcServer({ listenInfos: 'NOT-AN-ARRAY' })
	).rejects.toThrow(TypeError);

	await expect(
		// @ts-expect-error --- Testing purposes.
		ctx.worker!.createWebRtcServer({ listenInfos: ['NOT-AN-OBJECT'] })
	).rejects.toThrow(Error);

	// Empty listenInfos so should fail.
	await expect(
		ctx.worker!.createWebRtcServer({ listenInfos: [] })
	).rejects.toThrow(TypeError);
}, 2000);

test('worker.createWebRtcServer() with unavailable listenInfos rejects with Error', async () => {
	const worker2 = await mediasoup.createWorker();
	const port1 = await pickPort({
		type: 'udp',
		ip: '127.0.0.1',
		reserveTimeout: 0,
	});
	const port2 = await pickPort({
		type: 'udp',
		ip: '127.0.0.1',
		reserveTimeout: 0,
	});

	// Using an unavailable listen IP.
	await expect(
		ctx.worker!.createWebRtcServer({
			listenInfos: [
				{
					protocol: 'udp',
					ip: '127.0.0.1',
					port: port1,
				},
				{
					protocol: 'udp',
					ip: '1.2.3.4',
					port: port2,
				},
			],
		})
	).rejects.toThrow(Error);

	// Using the same UDP port in two listenInfos.
	await expect(
		ctx.worker!.createWebRtcServer({
			listenInfos: [
				{
					protocol: 'udp',
					ip: '127.0.0.1',
					port: port1,
				},
				{
					protocol: 'udp',
					ip: '127.0.0.1',
					announcedAddress: '1.2.3.4',
					port: port1,
				},
			],
		})
	).rejects.toThrow(Error);

	await ctx.worker!.createWebRtcServer({
		listenInfos: [
			{
				protocol: 'udp',
				ip: '127.0.0.1',
				port: port1,
			},
		],
	});

	// Using the same UDP port in a second Worker.
	await expect(
		worker2.createWebRtcServer({
			listenInfos: [
				{
					protocol: 'udp',
					ip: '127.0.0.1',
					port: port1,
				},
			],
		})
	).rejects.toThrow(Error);

	worker2.close();
}, 2000);

test('worker.createWebRtcServer() rejects with InvalidStateError if Worker is closed', async () => {
	ctx.worker!.close();

	const port = await pickPort({
		type: 'udp',
		ip: '127.0.0.1',
		reserveTimeout: 0,
	});

	await expect(
		ctx.worker!.createWebRtcServer({
			listenInfos: [{ protocol: 'udp', ip: '127.0.0.1', port }],
		})
	).rejects.toThrow(InvalidStateError);
}, 2000);

test('webRtcServer.close() succeeds', async () => {
	const port = await pickPort({
		type: 'udp',
		ip: '127.0.0.1',
		reserveTimeout: 0,
	});
	const webRtcServer = await ctx.worker!.createWebRtcServer({
		listenInfos: [{ protocol: 'udp', ip: '127.0.0.1', port }],
	});
	const onObserverClose = jest.fn();

	webRtcServer.observer.once('close', onObserverClose);
	webRtcServer.close();

	expect(onObserverClose).toHaveBeenCalledTimes(1);
	expect(webRtcServer.closed).toBe(true);
}, 2000);

test('WebRtcServer emits "workerclose" if Worker is closed', async () => {
	const port = await pickPort({
		type: 'udp',
		ip: '127.0.0.1',
		reserveTimeout: 0,
	});
	const webRtcServer = await ctx.worker!.createWebRtcServer({
		listenInfos: [{ protocol: 'tcp', ip: '127.0.0.1', port }],
	});
	const onObserverClose = jest.fn();

	webRtcServer.observer.once('close', onObserverClose);

	const promise = enhancedOnce<WebRtcServerEvents>(webRtcServer, 'workerclose');

	ctx.worker!.close();
	await promise;

	expect(onObserverClose).toHaveBeenCalledTimes(1);
	expect(webRtcServer.closed).toBe(true);
}, 2000);

test('router.createWebRtcTransport() with webRtcServer succeeds and transport is closed', async () => {
	const port1 = await pickPort({
		type: 'udp',
		ip: '127.0.0.1',
		reserveTimeout: 0,
	});
	const port2 = await pickPort({
		type: 'tcp',
		ip: '127.0.0.1',
		reserveTimeout: 0,
	});
	const webRtcServer = await ctx.worker!.createWebRtcServer({
		listenInfos: [
			{ protocol: 'udp', ip: '127.0.0.1', port: port1 },
			{ protocol: 'tcp', ip: '127.0.0.1', port: port2 },
		],
	});

	const onObserverWebRtcTransportHandled = jest.fn();
	const onObserverWebRtcTransportUnhandled = jest.fn();

	webRtcServer.observer.once(
		'webrtctransporthandled',
		onObserverWebRtcTransportHandled
	);
	webRtcServer.observer.once(
		'webrtctransportunhandled',
		onObserverWebRtcTransportUnhandled
	);

	const router = await ctx.worker!.createRouter();

	const onObserverNewTransport = jest.fn();

	router.observer.once('newtransport', onObserverNewTransport);

	const transport = await router.createWebRtcTransport({
		webRtcServer,
		// Let's disable UDP so resulting ICE candidates should only contain TCP.
		enableUdp: false,
		appData: { foo: 'bar' },
	});

	await expect(router.dump()).resolves.toMatchObject({
		transportIds: [transport.id],
	});

	expect(onObserverWebRtcTransportHandled).toHaveBeenCalledTimes(1);
	expect(onObserverWebRtcTransportHandled).toHaveBeenCalledWith(transport);
	expect(onObserverNewTransport).toHaveBeenCalledTimes(1);
	expect(onObserverNewTransport).toHaveBeenCalledWith(transport);
	expect(typeof transport.id).toBe('string');
	expect(transport.closed).toBe(false);
	expect(transport.appData).toEqual({ foo: 'bar' });

	const iceCandidates = transport.iceCandidates;

	expect(iceCandidates.length).toBe(1);
	expect(iceCandidates[0].ip).toBe('127.0.0.1');
	expect(iceCandidates[0].port).toBe(port2);
	expect(iceCandidates[0].protocol).toBe('tcp');
	expect(iceCandidates[0].type).toBe('host');
	expect(iceCandidates[0].tcpType).toBe('passive');

	expect(transport.iceState).toBe('new');
	expect(transport.iceSelectedTuple).toBeUndefined();

	// API not exposed in the interface.
	expect(
		(webRtcServer as WebRtcServerImpl).webRtcTransportsForTesting.size
	).toBe(1);
	// API not exposed in the interface.
	expect((router as RouterImpl).transportsForTesting.size).toBe(1);

	await expect(webRtcServer.dump()).resolves.toMatchObject({
		id: webRtcServer.id,
		udpSockets: [{ ip: '127.0.0.1', port: port1 }],
		tcpServers: [{ ip: '127.0.0.1', port: port2 }],
		webRtcTransportIds: [transport.id],
		localIceUsernameFragments: [
			{ /* localIceUsernameFragment: xxx, */ webRtcTransportId: transport.id },
		],
		tupleHashes: [],
	});

	transport.close();

	expect(transport.closed).toBe(true);
	expect(onObserverWebRtcTransportUnhandled).toHaveBeenCalledTimes(1);
	expect(onObserverWebRtcTransportUnhandled).toHaveBeenCalledWith(transport);
	// API not exposed in the interface.
	expect(
		(webRtcServer as WebRtcServerImpl).webRtcTransportsForTesting.size
	).toBe(0);
	// API not exposed in the interface.
	expect((router as RouterImpl).transportsForTesting.size).toBe(0);

	await expect(webRtcServer.dump()).resolves.toMatchObject({
		id: webRtcServer.id,
		udpSockets: [{ ip: '127.0.0.1', port: port1 }],
		tcpServers: [{ ip: '127.0.0.1', port: port2 }],
		webRtcTransportIds: [],
		localIceUsernameFragments: [],
		tupleHashes: [],
	});
}, 2000);

test('router.createWebRtcTransport() with webRtcServer succeeds and webRtcServer is closed', async () => {
	const port1 = await pickPort({
		type: 'udp',
		ip: '127.0.0.1',
		reserveTimeout: 0,
	});
	const port2 = await pickPort({
		type: 'tcp',
		ip: '127.0.0.1',
		reserveTimeout: 0,
	});
	const webRtcServer = await ctx.worker!.createWebRtcServer({
		listenInfos: [
			{ protocol: 'udp', ip: '127.0.0.1', port: port1 },
			{ protocol: 'tcp', ip: '127.0.0.1', port: port2 },
		],
	});

	const onObserverWebRtcTransportHandled = jest.fn();
	const onObserverWebRtcTransportUnhandled = jest.fn();

	webRtcServer.observer.once(
		'webrtctransporthandled',
		onObserverWebRtcTransportHandled
	);
	webRtcServer.observer.once(
		'webrtctransportunhandled',
		onObserverWebRtcTransportUnhandled
	);

	const router = await ctx.worker!.createRouter();
	const transport = await router.createWebRtcTransport({
		webRtcServer,
		appData: { foo: 'bar' },
	});

	expect(onObserverWebRtcTransportHandled).toHaveBeenCalledTimes(1);
	expect(onObserverWebRtcTransportHandled).toHaveBeenCalledWith(transport);

	await expect(router.dump()).resolves.toMatchObject({
		transportIds: [transport.id],
	});

	expect(typeof transport.id).toBe('string');
	expect(transport.closed).toBe(false);
	expect(transport.appData).toEqual({ foo: 'bar' });

	const iceCandidates = transport.iceCandidates;

	expect(iceCandidates.length).toBe(2);
	expect(iceCandidates[0].ip).toBe('127.0.0.1');
	expect(iceCandidates[0].port).toBe(port1);
	expect(iceCandidates[0].protocol).toBe('udp');
	expect(iceCandidates[0].type).toBe('host');
	expect(iceCandidates[0].tcpType).toBeUndefined();
	expect(iceCandidates[1].ip).toBe('127.0.0.1');
	expect(iceCandidates[1].port).toBe(port2);
	expect(iceCandidates[1].protocol).toBe('tcp');
	expect(iceCandidates[1].type).toBe('host');
	expect(iceCandidates[1].tcpType).toBe('passive');

	expect(transport.iceState).toBe('new');
	expect(transport.iceSelectedTuple).toBeUndefined();

	// API not exposed in the interface.
	expect(
		(webRtcServer as WebRtcServerImpl).webRtcTransportsForTesting.size
	).toBe(1);
	// API not exposed in the interface.
	expect((router as RouterImpl).transportsForTesting.size).toBe(1);

	await expect(webRtcServer.dump()).resolves.toMatchObject({
		id: webRtcServer.id,
		udpSockets: [{ ip: '127.0.0.1', port: port1 }],
		tcpServers: [{ ip: '127.0.0.1', port: port2 }],
		webRtcTransportIds: [transport.id],
		localIceUsernameFragments: [
			{ /* localIceUsernameFragment: xxx, */ webRtcTransportId: transport.id },
		],
		tupleHashes: [],
	});

	// Let's restart ICE in the transport so it should add a new entry in
	// localIceUsernameFragments in the WebRtcServer.
	await transport.restartIce();

	await expect(webRtcServer.dump()).resolves.toMatchObject({
		id: webRtcServer.id,
		udpSockets: [{ ip: '127.0.0.1', port: port1 }],
		tcpServers: [{ ip: '127.0.0.1', port: port2 }],
		webRtcTransportIds: [transport.id],
		localIceUsernameFragments: [
			{ /* localIceUsernameFragment: xxx, */ webRtcTransportId: transport.id },
			{ /* localIceUsernameFragment: yyy, */ webRtcTransportId: transport.id },
		],
		tupleHashes: [],
	});

	const onObserverClose = jest.fn();

	webRtcServer.observer.once('close', onObserverClose);

	const onListenServerClose = jest.fn();

	transport.once('listenserverclose', onListenServerClose);

	webRtcServer.close();

	expect(webRtcServer.closed).toBe(true);
	expect(onObserverClose).toHaveBeenCalledTimes(1);
	expect(onListenServerClose).toHaveBeenCalledTimes(1);
	expect(onObserverWebRtcTransportUnhandled).toHaveBeenCalledTimes(1);
	expect(onObserverWebRtcTransportUnhandled).toHaveBeenCalledWith(transport);
	expect(transport.closed).toBe(true);
	expect(transport.iceState).toBe('closed');
	expect(transport.iceSelectedTuple).toBe(undefined);
	expect(transport.dtlsState).toBe('closed');
	expect(transport.sctpState).toBe(undefined);
	// API not exposed in the interface.
	expect(
		(webRtcServer as WebRtcServerImpl).webRtcTransportsForTesting.size
	).toBe(0);
	// API not exposed in the interface.
	expect((router as RouterImpl).transportsForTesting.size).toBe(0);

	await expect(ctx.worker!.dump()).resolves.toMatchObject({
		pid: ctx.worker!.pid,
		webRtcServerIds: [],
		routerIds: [router.id],
		channelMessageHandlers: {
			channelRequestHandlers: [router.id],
			channelNotificationHandlers: [],
		},
	});

	await expect(router.dump()).resolves.toMatchObject({
		id: router.id,
		transportIds: [],
	});
}, 2000);
