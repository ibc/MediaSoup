import * as mediasoup from '../';
import { enhancedOnce } from '../enhancedEvents';
import { WorkerEvents, DataConsumerEvents } from '../types';
import * as utils from '../utils';

type TestContext = {
	dataProducerOptions: mediasoup.types.DataProducerOptions;
	worker?: mediasoup.types.Worker;
	router?: mediasoup.types.Router;
	webRtcTransport1?: mediasoup.types.WebRtcTransport;
	webRtcTransport2?: mediasoup.types.WebRtcTransport;
	directTransport?: mediasoup.types.DirectTransport;
	dataProducer?: mediasoup.types.DataProducer;
};

const ctx: TestContext = {
	dataProducerOptions: utils.deepFreeze<mediasoup.types.DataProducerOptions>({
		sctpStreamParameters: {
			streamId: 12345,
			ordered: false,
			maxPacketLifeTime: 5000,
		},
		label: 'foo',
		protocol: 'bar',
	}),
};

beforeEach(async () => {
	ctx.worker = await mediasoup.createWorker();
	ctx.router = await ctx.worker.createRouter();
	ctx.webRtcTransport1 = await ctx.router.createWebRtcTransport({
		listenIps: ['127.0.0.1'],
		enableSctp: true,
	});
	ctx.webRtcTransport2 = await ctx.router.createWebRtcTransport({
		listenIps: ['127.0.0.1'],
		enableSctp: true,
	});
	ctx.directTransport = await ctx.router.createDirectTransport();
	ctx.dataProducer = await ctx.webRtcTransport1.produceData(
		ctx.dataProducerOptions
	);
});

afterEach(async () => {
	ctx.worker?.close();

	if (ctx.worker?.subprocessClosed === false) {
		await enhancedOnce<WorkerEvents>(ctx.worker, 'subprocessclose');
	}
});

test('transport.consumeData() succeeds', async () => {
	const onObserverNewDataConsumer = jest.fn();

	ctx.webRtcTransport2!.observer.once(
		'newdataconsumer',
		onObserverNewDataConsumer
	);

	const dataConsumer1 = await ctx.webRtcTransport2!.consumeData({
		dataProducerId: ctx.dataProducer!.id,
		maxPacketLifeTime: 4000,
		// Valid values are 0...65535 so others and duplicated ones will be
		// discarded.
		subchannels: [0, 1, 1, 1, 2, 65535, 65536, 65537, 100],
		appData: { baz: 'LOL' },
	});

	expect(onObserverNewDataConsumer).toHaveBeenCalledTimes(1);
	expect(onObserverNewDataConsumer).toHaveBeenCalledWith(dataConsumer1);
	expect(typeof dataConsumer1.id).toBe('string');
	expect(dataConsumer1.dataProducerId).toBe(ctx.dataProducer!.id);
	expect(dataConsumer1.closed).toBe(false);
	expect(dataConsumer1.type).toBe('sctp');
	expect(typeof dataConsumer1.sctpStreamParameters).toBe('object');
	expect(typeof dataConsumer1.sctpStreamParameters?.streamId).toBe('number');
	expect(dataConsumer1.sctpStreamParameters?.ordered).toBe(false);
	expect(dataConsumer1.sctpStreamParameters?.maxPacketLifeTime).toBe(4000);
	expect(dataConsumer1.sctpStreamParameters?.maxRetransmits).toBeUndefined();
	expect(dataConsumer1.label).toBe('foo');
	expect(dataConsumer1.protocol).toBe('bar');
	expect(dataConsumer1.paused).toBe(false);
	expect(dataConsumer1.subchannels).toEqual(
		expect.arrayContaining([0, 1, 2, 100, 65535])
	);
	expect(dataConsumer1.appData).toEqual({ baz: 'LOL' });

	const dump = await ctx.router!.dump();

	expect(dump.mapDataProducerIdDataConsumerIds).toEqual(
		expect.arrayContaining([
			{ key: ctx.dataProducer!.id, values: [dataConsumer1.id] },
		])
	);

	expect(dump.mapDataConsumerIdDataProducerId).toEqual(
		expect.arrayContaining([
			{ key: dataConsumer1.id, value: ctx.dataProducer!.id },
		])
	);

	await expect(ctx.webRtcTransport2!.dump()).resolves.toMatchObject({
		id: ctx.webRtcTransport2!.id,
		dataProducerIds: [],
		dataConsumerIds: [dataConsumer1.id],
	});
}, 2000);

test('dataConsumer.dump() succeeds', async () => {
	const dataConsumer = await ctx.webRtcTransport2!.consumeData({
		dataProducerId: ctx.dataProducer!.id,
		maxPacketLifeTime: 4000,
		// Valid values are 0...65535 so others and duplicated ones will be
		// discarded.
		subchannels: [0, 1, 1, 1, 2, 65535, 65536, 65537, 100],
		appData: { baz: 'LOL' },
	});

	const dump = await dataConsumer.dump();

	expect(dump.id).toBe(dataConsumer.id);
	expect(dump.dataProducerId).toBe(dataConsumer.dataProducerId);
	expect(dump.type).toBe('sctp');
	expect(typeof dump.sctpStreamParameters).toBe('object');
	expect(dump.sctpStreamParameters!.streamId).toBe(
		dataConsumer.sctpStreamParameters?.streamId
	);
	expect(dump.sctpStreamParameters!.ordered).toBe(false);
	expect(dump.sctpStreamParameters!.maxPacketLifeTime).toBe(4000);
	expect(dump.sctpStreamParameters!.maxRetransmits).toBeUndefined();
	expect(dump.label).toBe('foo');
	expect(dump.protocol).toBe('bar');
	expect(dump.paused).toBe(false);
	expect(dump.dataProducerPaused).toBe(false);
	expect(dump.subchannels).toEqual(
		expect.arrayContaining([0, 1, 2, 100, 65535])
	);
}, 2000);

test('dataConsumer.getStats() succeeds', async () => {
	const dataConsumer = await ctx.webRtcTransport2!.consumeData({
		dataProducerId: ctx.dataProducer!.id,
	});

	await expect(dataConsumer.getStats()).resolves.toMatchObject([
		{
			type: 'data-consumer',
			label: dataConsumer.label,
			protocol: dataConsumer.protocol,
			messagesSent: 0,
			bytesSent: 0,
		},
	]);
}, 2000);

test('dataConsumer.setSubchannels() succeeds', async () => {
	const dataConsumer = await ctx.webRtcTransport2!.consumeData({
		dataProducerId: ctx.dataProducer!.id,
	});

	await dataConsumer.setSubchannels([999, 999, 998, 65536]);

	expect(dataConsumer.subchannels).toEqual(
		expect.arrayContaining([0, 998, 999])
	);
}, 2000);

test('dataConsumer.addSubchannel() and .removeSubchannel() succeed', async () => {
	const dataConsumer = await ctx.webRtcTransport2!.consumeData({
		dataProducerId: ctx.dataProducer!.id,
	});

	await dataConsumer.setSubchannels([]);
	expect(dataConsumer.subchannels).toEqual([]);

	await dataConsumer.addSubchannel(5);
	expect(dataConsumer.subchannels).toEqual(expect.arrayContaining([5]));

	await dataConsumer.addSubchannel(10);
	expect(dataConsumer.subchannels).toEqual(expect.arrayContaining([5, 10]));

	await dataConsumer.addSubchannel(5);
	expect(dataConsumer.subchannels).toEqual(expect.arrayContaining([5, 10]));

	await dataConsumer.removeSubchannel(666);
	expect(dataConsumer.subchannels).toEqual(expect.arrayContaining([5, 10]));

	await dataConsumer.removeSubchannel(5);
	expect(dataConsumer.subchannels).toEqual(expect.arrayContaining([10]));

	await dataConsumer.setSubchannels([]);
	expect(dataConsumer.subchannels).toEqual([]);
}, 2000);

test('transport.consumeData() on a DirectTransport succeeds', async () => {
	const onObserverNewDataConsumer = jest.fn();

	ctx.directTransport!.observer.once(
		'newdataconsumer',
		onObserverNewDataConsumer
	);

	const dataConsumer = await ctx.directTransport!.consumeData({
		dataProducerId: ctx.dataProducer!.id,
		paused: true,
		appData: { hehe: 'HEHE' },
	});

	expect(onObserverNewDataConsumer).toHaveBeenCalledTimes(1);
	expect(onObserverNewDataConsumer).toHaveBeenCalledWith(dataConsumer);
	expect(typeof dataConsumer.id).toBe('string');
	expect(dataConsumer.dataProducerId).toBe(ctx.dataProducer!.id);
	expect(dataConsumer.closed).toBe(false);
	expect(dataConsumer.type).toBe('direct');
	expect(dataConsumer.sctpStreamParameters).toBeUndefined();
	expect(dataConsumer.label).toBe('foo');
	expect(dataConsumer.protocol).toBe('bar');
	expect(dataConsumer.paused).toBe(true);
	expect(dataConsumer.appData).toEqual({ hehe: 'HEHE' });

	await expect(ctx.directTransport!.dump()).resolves.toMatchObject({
		id: ctx.directTransport!.id,
		dataProducerIds: [],
		dataConsumerIds: [dataConsumer.id],
	});
}, 2000);

test('dataConsumer.dump() on a DirectTransport succeeds', async () => {
	const dataConsumer = await ctx.directTransport!.consumeData({
		dataProducerId: ctx.dataProducer!.id,
		paused: true,
	});

	const dump = await dataConsumer.dump();

	expect(dump.id).toBe(dataConsumer.id);
	expect(dump.dataProducerId).toBe(dataConsumer.dataProducerId);
	expect(dump.type).toBe('direct');
	expect(dump.sctpStreamParameters).toBeUndefined();
	expect(dump.label).toBe('foo');
	expect(dump.protocol).toBe('bar');
	expect(dump.paused).toBe(true);
	expect(dump.subchannels).toEqual([]);
}, 2000);

test('dataConsumer.getStats() on a DirectTransport succeeds', async () => {
	const dataConsumer = await ctx.directTransport!.consumeData({
		dataProducerId: ctx.dataProducer!.id,
	});

	await expect(dataConsumer.getStats()).resolves.toMatchObject([
		{
			type: 'data-consumer',
			label: dataConsumer.label,
			protocol: dataConsumer.protocol,
			messagesSent: 0,
			bytesSent: 0,
		},
	]);
}, 2000);

test('dataConsumer.pause() and resume() succeed', async () => {
	const onObserverPause = jest.fn();
	const onObserverResume = jest.fn();

	const dataConsumer = await ctx.webRtcTransport2!.consumeData({
		dataProducerId: ctx.dataProducer!.id,
	});

	dataConsumer.observer.on('pause', onObserverPause);
	dataConsumer.observer.on('resume', onObserverResume);

	await dataConsumer.pause();

	expect(dataConsumer.paused).toBe(true);

	const dump1 = await dataConsumer.dump();

	expect(dump1.paused).toBe(true);

	await dataConsumer.resume();

	expect(dataConsumer.paused).toBe(false);

	const dump2 = await dataConsumer.dump();

	expect(dump2.paused).toBe(false);

	// Even if we don't await for pause()/resume() completion, the observer must
	// fire 'pause' and 'resume' events if state was the opposite.
	void dataConsumer.pause();
	void dataConsumer.resume();
	void dataConsumer.pause();
	void dataConsumer.pause();
	void dataConsumer.pause();
	await dataConsumer.resume();

	expect(onObserverPause).toHaveBeenCalledTimes(3);
	expect(onObserverResume).toHaveBeenCalledTimes(3);
}, 2000);

test('dataProducer.pause() and resume() emit events', async () => {
	const dataConsumer = await ctx.webRtcTransport2!.consumeData({
		dataProducerId: ctx.dataProducer!.id,
	});
	const promises = [];
	const events: string[] = [];

	dataConsumer.observer.once('resume', () => {
		events.push('resume');
	});

	dataConsumer.observer.once('pause', () => {
		events.push('pause');
	});

	promises.push(ctx.dataProducer!.pause());
	promises.push(ctx.dataProducer!.resume());

	await Promise.all(promises);

	// Must also wait a bit for the corresponding events in the data consumer.
	await new Promise(resolve => setTimeout(resolve, 100));

	expect(events).toEqual(['pause', 'resume']);
	expect(dataConsumer.paused).toBe(false);
}, 2000);

test('dataConsumer.close() succeeds', async () => {
	const onObserverClose = jest.fn();
	const dataConsumer = await ctx.webRtcTransport2!.consumeData({
		dataProducerId: ctx.dataProducer!.id,
	});

	dataConsumer.observer.once('close', onObserverClose);
	dataConsumer.close();

	expect(onObserverClose).toHaveBeenCalledTimes(1);
	expect(dataConsumer.closed).toBe(true);

	const dump = await ctx.router!.dump();

	expect(dump.mapDataProducerIdDataConsumerIds).toEqual(
		expect.arrayContaining([{ key: ctx.dataProducer!.id, values: [] }])
	);

	expect(dump.mapDataConsumerIdDataProducerId).toEqual([]);

	await expect(ctx.webRtcTransport2!.dump()).resolves.toMatchObject({
		id: ctx.webRtcTransport2!.id,
		dataProducerIds: [],
		dataConsumerIds: [],
	});
}, 2000);

test('Consumer methods reject if closed', async () => {
	const dataConsumer = await ctx.webRtcTransport2!.consumeData({
		dataProducerId: ctx.dataProducer!.id,
	});

	dataConsumer.close();

	await expect(dataConsumer.dump()).rejects.toThrow(Error);

	await expect(dataConsumer.getStats()).rejects.toThrow(Error);
}, 2000);

test('DataConsumer emits "dataproducerclose" if DataProducer is closed', async () => {
	const dataConsumer = await ctx.webRtcTransport2!.consumeData({
		dataProducerId: ctx.dataProducer!.id,
	});
	const onObserverClose = jest.fn();

	dataConsumer.observer.once('close', onObserverClose);

	const promise = enhancedOnce<DataConsumerEvents>(
		dataConsumer,
		'dataproducerclose'
	);

	ctx.dataProducer!.close();
	await promise;

	expect(onObserverClose).toHaveBeenCalledTimes(1);
	expect(dataConsumer.closed).toBe(true);
}, 2000);

test('DataConsumer emits "transportclose" if Transport is closed', async () => {
	const dataConsumer = await ctx.webRtcTransport2!.consumeData({
		dataProducerId: ctx.dataProducer!.id,
	});
	const onObserverClose = jest.fn();

	dataConsumer.observer.once('close', onObserverClose);

	const promise = enhancedOnce<DataConsumerEvents>(
		dataConsumer,
		'transportclose'
	);

	ctx.webRtcTransport2!.close();
	await promise;

	expect(onObserverClose).toHaveBeenCalledTimes(1);
	expect(dataConsumer.closed).toBe(true);

	await expect(ctx.router!.dump()).resolves.toMatchObject({
		mapDataProducerIdDataConsumerIds: {},
		mapDataConsumerIdDataProducerId: {},
	});
}, 2000);
