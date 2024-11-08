import * as mediasoup from '../';
import { enhancedOnce } from '../enhancedEvents';
import { WorkerEvents, AudioLevelObserverEvents } from '../types';
import * as utils from '../utils';

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

test('router.createAudioLevelObserver() succeeds', async () => {
	const onObserverNewRtpObserver = jest.fn();

	ctx.router!.observer.once('newrtpobserver', onObserverNewRtpObserver);

	const audioLevelObserver = await ctx.router!.createAudioLevelObserver();

	expect(onObserverNewRtpObserver).toHaveBeenCalledTimes(1);
	expect(onObserverNewRtpObserver).toHaveBeenCalledWith(audioLevelObserver);
	expect(typeof audioLevelObserver.id).toBe('string');
	expect(audioLevelObserver.closed).toBe(false);
	expect(audioLevelObserver.paused).toBe(false);
	expect(audioLevelObserver.appData).toEqual({});

	await expect(ctx.router!.dump()).resolves.toMatchObject({
		rtpObserverIds: [audioLevelObserver.id],
	});
}, 2000);

test('router.createAudioLevelObserver() with wrong arguments rejects with TypeError', async () => {
	await expect(
		ctx.router!.createAudioLevelObserver({ maxEntries: 0 })
	).rejects.toThrow(TypeError);

	await expect(
		ctx.router!.createAudioLevelObserver({ maxEntries: -10 })
	).rejects.toThrow(TypeError);

	await expect(
		// @ts-expect-error --- Testing purposes.
		ctx.router!.createAudioLevelObserver({ threshold: 'foo' })
	).rejects.toThrow(TypeError);

	await expect(
		// @ts-expect-error --- Testing purposes.
		ctx.router!.createAudioLevelObserver({ interval: false })
	).rejects.toThrow(TypeError);

	await expect(
		// @ts-expect-error --- Testing purposes.
		ctx.router!.createAudioLevelObserver({ appData: 'NOT-AN-OBJECT' })
	).rejects.toThrow(TypeError);
}, 2000);

test('audioLevelObserver.pause() and resume() succeed', async () => {
	const audioLevelObserver = await ctx.router!.createAudioLevelObserver();

	await audioLevelObserver.pause();

	expect(audioLevelObserver.paused).toBe(true);

	await audioLevelObserver.resume();

	expect(audioLevelObserver.paused).toBe(false);
}, 2000);

test('audioLevelObserver.close() succeeds', async () => {
	const audioLevelObserver = await ctx.router!.createAudioLevelObserver({
		maxEntries: 8,
	});

	let dump = await ctx.router!.dump();

	expect(dump.rtpObserverIds.length).toBe(1);

	audioLevelObserver.close();

	expect(audioLevelObserver.closed).toBe(true);

	dump = await ctx.router!.dump();

	expect(dump.rtpObserverIds.length).toBe(0);
}, 2000);

test('AudioLevelObserver emits "routerclose" if Router is closed', async () => {
	const audioLevelObserver = await ctx.router!.createAudioLevelObserver();

	const promise = enhancedOnce<AudioLevelObserverEvents>(
		audioLevelObserver,
		'routerclose'
	);

	ctx.router!.close();
	await promise;

	expect(audioLevelObserver.closed).toBe(true);
}, 2000);

test('AudioLevelObserver emits "routerclose" if Worker is closed', async () => {
	const audioLevelObserver = await ctx.router!.createAudioLevelObserver();

	const promise = enhancedOnce<AudioLevelObserverEvents>(
		audioLevelObserver,
		'routerclose'
	);

	ctx.worker!.close();
	await promise;

	expect(audioLevelObserver.closed).toBe(true);
}, 2000);
