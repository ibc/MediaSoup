import * as fs from 'node:fs';
import * as path from 'node:path';
import { enhancedOnce } from '../enhancedEvents';
import * as mediasoup from '../';
import type { WorkerEvents } from '../types';

const PKG = JSON.parse(
	fs.readFileSync(path.join(__dirname, '..', '..', '..', 'package.json'), {
		encoding: 'utf-8',
	})
);

const { version, getSupportedRtpCapabilities, parseScalabilityMode } =
	mediasoup;

test('mediasoup.version matches version field in package.json', () => {
	expect(version).toBe(PKG.version);
});

test('setLoggerEventListeners() works', async () => {
	const onDebug = jest.fn();

	mediasoup.setLogEventListeners({
		ondebug: onDebug,
		onwarn: undefined,
		onerror: undefined,
	});

	const worker = await mediasoup.createWorker();

	worker.close();

	expect(onDebug).toHaveBeenCalled();

	if (worker.subprocessClosed === false) {
		await enhancedOnce<WorkerEvents>(worker, 'subprocessclose');
	}
}, 2000);

test('mediasoup.getSupportedRtpCapabilities() returns the mediasoup RTP capabilities', () => {
	const rtpCapabilities = getSupportedRtpCapabilities();

	expect(typeof rtpCapabilities).toBe('object');

	// Mangle retrieved codecs to check that, if called again,
	// getSupportedRtpCapabilities() returns a cloned object.
	// @ts-expect-error --- Testing purposes.
	rtpCapabilities.codecs = 'bar';

	const rtpCapabilities2 = getSupportedRtpCapabilities();

	expect(rtpCapabilities2).not.toEqual(rtpCapabilities);
});

test('parseScalabilityMode() works', () => {
	expect(parseScalabilityMode('L1T3')).toEqual({
		spatialLayers: 1,
		temporalLayers: 3,
		ksvc: false,
	});

	expect(parseScalabilityMode('L3T2_KEY')).toEqual({
		spatialLayers: 3,
		temporalLayers: 2,
		ksvc: true,
	});

	expect(parseScalabilityMode('S2T3')).toEqual({
		spatialLayers: 2,
		temporalLayers: 3,
		ksvc: false,
	});

	expect(parseScalabilityMode('foo')).toEqual({
		spatialLayers: 1,
		temporalLayers: 1,
		ksvc: false,
	});

	expect(parseScalabilityMode(undefined)).toEqual({
		spatialLayers: 1,
		temporalLayers: 1,
		ksvc: false,
	});

	expect(parseScalabilityMode('S0T3')).toEqual({
		spatialLayers: 1,
		temporalLayers: 1,
		ksvc: false,
	});

	expect(parseScalabilityMode('S1T0')).toEqual({
		spatialLayers: 1,
		temporalLayers: 1,
		ksvc: false,
	});

	expect(parseScalabilityMode('L20T3')).toEqual({
		spatialLayers: 20,
		temporalLayers: 3,
		ksvc: false,
	});

	expect(parseScalabilityMode('S200T3')).toEqual({
		spatialLayers: 1,
		temporalLayers: 1,
		ksvc: false,
	});

	expect(parseScalabilityMode('L4T7_KEY_SHIFT')).toEqual({
		spatialLayers: 4,
		temporalLayers: 7,
		ksvc: true,
	});
});
