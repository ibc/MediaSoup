import { Logger, LoggerEmitter } from './Logger';
import { EnhancedEventEmitter } from './enhancedEvents';
import type { Worker, WorkerSettings } from './WorkerTypes';
import { WorkerImpl, workerBin } from './Worker';
import * as utils from './utils';
import { supportedRtpCapabilities } from './supportedRtpCapabilities';
import type { RtpCapabilities } from './rtpParametersTypes';
import type * as types from './types';

/**
 * Expose all types.
 */
export { types };

/**
 * Expose mediasoup version.
 */
// eslint-disable-next-line @typescript-eslint/no-require-imports
export const version: string = require('../../package.json').version;

/**
 * Expose parseScalabilityMode() function.
 */
export { parse as parseScalabilityMode } from './scalabilityModes';

export type Observer = EnhancedEventEmitter<ObserverEvents>;

export type ObserverEvents = {
	newworker: [Worker];
};

const observer: Observer = new EnhancedEventEmitter<ObserverEvents>();

/**
 * Observer.
 */
export { observer };

/**
 * Full path of the mediasoup-worker binary.
 */
export { workerBin };

const logger = new Logger();

/**
 * Set event listeners for mediasoup generated logs. If called with no arguments
 * then no events will be emitted.
 *
 * @example
 * ```ts
 * mediasoup.setLogEventListeners({
 *   ondebug: undefined,
 *   onwarn: (namespace: string, log: string) => {
 *     MyEnterpriseLogger.warn(`${namespace} ${log}`);
 *   },
 *   onerror: (namespace: string, log: string, error?: Error) => {
 *     if (error) {
 *       MyEnterpriseLogger.error(`${namespace} ${log}: ${error}`);
 *     } else {
 *       MyEnterpriseLogger.error(`${namespace} ${log}`);
 *     }
 *   }
 * });
 * ```
 */
export function setLogEventListeners(
	listeners?: types.LogEventListeners
): void {
	logger.debug('setLogEventListeners()');

	let debugLogEmitter: LoggerEmitter | undefined;
	let warnLogEmitter: LoggerEmitter | undefined;
	let errorLogEmitter: LoggerEmitter | undefined;

	if (listeners?.ondebug) {
		debugLogEmitter = new EnhancedEventEmitter();

		debugLogEmitter.on('debuglog', listeners.ondebug);
	}

	if (listeners?.onwarn) {
		warnLogEmitter = new EnhancedEventEmitter();

		warnLogEmitter.on('warnlog', listeners.onwarn);
	}

	if (listeners?.onerror) {
		errorLogEmitter = new EnhancedEventEmitter();

		errorLogEmitter.on('errorlog', listeners.onerror);
	}

	Logger.setEmitters(debugLogEmitter, warnLogEmitter, errorLogEmitter);
}

/**
 * Create a Worker.
 */
export async function createWorker<
	WorkerAppData extends types.AppData = types.AppData,
>({
	logLevel = 'error',
	logTags,
	rtcMinPort = 10000,
	rtcMaxPort = 59999,
	dtlsCertificateFile,
	dtlsPrivateKeyFile,
	libwebrtcFieldTrials,
	disableLiburing,
	appData,
}: WorkerSettings<WorkerAppData> = {}): Promise<Worker<WorkerAppData>> {
	logger.debug('createWorker()');

	if (appData && typeof appData !== 'object') {
		throw new TypeError('if given, appData must be an object');
	}

	const worker: Worker<WorkerAppData> = new WorkerImpl({
		logLevel,
		logTags,
		rtcMinPort,
		rtcMaxPort,
		dtlsCertificateFile,
		dtlsPrivateKeyFile,
		libwebrtcFieldTrials,
		disableLiburing,
		appData,
	});

	return new Promise((resolve, reject) => {
		worker.on('@success', () => {
			// Emit observer event.
			observer.safeEmit('newworker', worker);

			resolve(worker);
		});

		worker.on('@failure', reject);
	});
}

/**
 * Get a cloned copy of the mediasoup supported RTP capabilities.
 */
export function getSupportedRtpCapabilities(): RtpCapabilities {
	return utils.clone<RtpCapabilities>(supportedRtpCapabilities);
}
