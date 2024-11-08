import * as process from 'node:process';
import * as path from 'node:path';
import { spawn, ChildProcess } from 'node:child_process';
import { version } from './';
import { Logger } from './Logger';
import { EnhancedEventEmitter } from './enhancedEvents';
import * as ortc from './ortc';
import {
	WorkerInterface,
	WorkerSettings,
	WorkerUpdateableSettings,
	WorkerResourceUsage,
	WorkerDump,
	WorkerEvents,
	WorkerObserver,
	WorkerObserverEvents,
} from './WorkerTypes';
import { Channel } from './Channel';
import {
	WebRtcServerInterface,
	WebRtcServerOptions,
} from './WebRtcServerTypes';
import { WebRtcServerImpl } from './WebRtcServer';
import { RouterInterface, RouterOptions } from './RouterTypes';
import { RouterImpl } from './Router';
import { portRangeToFbs, socketFlagsToFbs } from './Transport';
import { RtpCodecCapability } from './RtpParameters';
import { AppData } from './types';
import * as utils from './utils';
import { Event } from './fbs/notification';
import * as FbsRequest from './fbs/request';
import * as FbsWorker from './fbs/worker';
import * as FbsTransport from './fbs/transport';
import { Protocol as FbsTransportProtocol } from './fbs/transport/protocol';

// If env MEDIASOUP_WORKER_BIN is given, use it as worker binary.
// Otherwise if env MEDIASOUP_BUILDTYPE is 'Debug' use the Debug binary.
// Otherwise use the Release binary.
export const workerBin = process.env.MEDIASOUP_WORKER_BIN
	? process.env.MEDIASOUP_WORKER_BIN
	: process.env.MEDIASOUP_BUILDTYPE === 'Debug'
		? path.join(
				__dirname,
				'..',
				'..',
				'worker',
				'out',
				'Debug',
				'mediasoup-worker'
			)
		: path.join(
				__dirname,
				'..',
				'..',
				'worker',
				'out',
				'Release',
				'mediasoup-worker'
			);

const logger = new Logger('Worker');
const workerLogger = new Logger('Worker');

export class WorkerImpl<WorkerAppData extends AppData = AppData>
	extends EnhancedEventEmitter<WorkerEvents>
	implements WorkerInterface
{
	// mediasoup-worker child process.
	#child: ChildProcess;

	// Worker process PID.
	readonly #pid: number;

	// Channel instance.
	readonly #channel: Channel;

	// Closed flag.
	#closed = false;

	// Died dlag.
	#died = false;

	// Worker subprocess closed flag.
	#subprocessClosed = false;

	// Custom app data.
	#appData: WorkerAppData;

	// WebRtcServers set.
	readonly #webRtcServers: Set<WebRtcServerInterface> = new Set();

	// Routers set.
	readonly #routers: Set<RouterInterface> = new Set();

	// Observer instance.
	readonly #observer: WorkerObserver =
		new EnhancedEventEmitter<WorkerObserverEvents>();

	/**
	 * @private
	 */
	constructor({
		logLevel,
		logTags,
		rtcMinPort,
		rtcMaxPort,
		dtlsCertificateFile,
		dtlsPrivateKeyFile,
		libwebrtcFieldTrials,
		disableLiburing,
		appData,
	}: WorkerSettings<WorkerAppData>) {
		super();

		logger.debug('constructor()');

		let spawnBin = workerBin;
		let spawnArgs: string[] = [];

		if (process.env.MEDIASOUP_USE_VALGRIND === 'true') {
			spawnBin = process.env.MEDIASOUP_VALGRIND_BIN ?? 'valgrind';

			if (process.env.MEDIASOUP_VALGRIND_OPTIONS) {
				spawnArgs = spawnArgs.concat(
					process.env.MEDIASOUP_VALGRIND_OPTIONS.split(/\s+/)
				);
			}

			spawnArgs.push(workerBin);
		}

		if (typeof logLevel === 'string' && logLevel) {
			spawnArgs.push(`--logLevel=${logLevel}`);
		}

		for (const logTag of Array.isArray(logTags) ? logTags : []) {
			if (typeof logTag === 'string' && logTag) {
				spawnArgs.push(`--logTag=${logTag}`);
			}
		}

		if (typeof rtcMinPort === 'number' && !Number.isNaN(rtcMinPort)) {
			spawnArgs.push(`--rtcMinPort=${rtcMinPort}`);
		}

		if (typeof rtcMaxPort === 'number' && !Number.isNaN(rtcMaxPort)) {
			spawnArgs.push(`--rtcMaxPort=${rtcMaxPort}`);
		}

		if (typeof dtlsCertificateFile === 'string' && dtlsCertificateFile) {
			spawnArgs.push(`--dtlsCertificateFile=${dtlsCertificateFile}`);
		}

		if (typeof dtlsPrivateKeyFile === 'string' && dtlsPrivateKeyFile) {
			spawnArgs.push(`--dtlsPrivateKeyFile=${dtlsPrivateKeyFile}`);
		}

		if (typeof libwebrtcFieldTrials === 'string' && libwebrtcFieldTrials) {
			spawnArgs.push(`--libwebrtcFieldTrials=${libwebrtcFieldTrials}`);
		}

		if (disableLiburing) {
			spawnArgs.push(`--disableLiburing=true`);
		}

		logger.debug(`spawning worker process: ${spawnBin} ${spawnArgs.join(' ')}`);

		this.#child = spawn(
			// command
			spawnBin,
			// args
			spawnArgs,
			// options
			{
				env: {
					MEDIASOUP_VERSION: version,
					// Let the worker process inherit all environment variables, useful
					// if a custom and not in the path GCC is used so the user can set
					// LD_LIBRARY_PATH environment variable for runtime.
					...process.env,
				},

				detached: false,

				// fd 0 (stdin)   : Just ignore it.
				// fd 1 (stdout)  : Pipe it for 3rd libraries that log their own stuff.
				// fd 2 (stderr)  : Same as stdout.
				// fd 3 (channel) : Producer Channel fd.
				// fd 4 (channel) : Consumer Channel fd.
				stdio: ['ignore', 'pipe', 'pipe', 'pipe', 'pipe'],
				windowsHide: true,
			}
		);

		this.#pid = this.#child.pid!;

		this.#channel = new Channel({
			producerSocket: this.#child.stdio[3],
			consumerSocket: this.#child.stdio[4],
			pid: this.#pid,
		});

		this.#appData = appData ?? ({} as WorkerAppData);

		let spawnDone = false;

		// Listen for 'running' notification.
		this.#channel.once(String(this.#pid), (event: Event) => {
			if (!spawnDone && event === Event.WORKER_RUNNING) {
				spawnDone = true;

				logger.debug(`worker process running [pid:${this.#pid}]`);

				this.emit('@success');
			}
		});

		this.#child.on('exit', (code, signal) => {
			// If killed by ourselves, do nothing.
			if (this.#child.killed) {
				return;
			}

			if (!spawnDone) {
				spawnDone = true;

				if (code === 42) {
					logger.error(
						`worker process failed due to wrong settings [pid:${this.#pid}]`
					);

					this.close();
					this.emit('@failure', new TypeError('wrong settings'));
				} else {
					logger.error(
						`worker process failed unexpectedly [pid:${this.#pid}, code:${code}, signal:${signal}]`
					);

					this.close();
					this.emit(
						'@failure',
						new Error(`[pid:${this.#pid}, code:${code}, signal:${signal}]`)
					);
				}
			} else {
				logger.error(
					`worker process died unexpectedly [pid:${this.#pid}, code:${code}, signal:${signal}]`
				);

				this.workerDied(
					new Error(`[pid:${this.#pid}, code:${code}, signal:${signal}]`)
				);
			}
		});

		this.#child.on('error', error => {
			// If killed by ourselves, do nothing.
			if (this.#child.killed) {
				return;
			}

			if (!spawnDone) {
				spawnDone = true;

				logger.error(
					`worker process failed [pid:${this.#pid}]: ${error.message}`
				);

				this.close();
				this.emit('@failure', error);
			} else {
				logger.error(
					`worker process error [pid:${this.#pid}]: ${error.message}`
				);

				this.workerDied(error);
			}
		});

		this.#child.on('close', (code, signal) => {
			logger.debug(
				`worker subprocess closed [pid:${this.#pid}, code:${code}, signal:${signal}]`
			);

			this.#subprocessClosed = true;

			this.safeEmit('subprocessclose');
		});

		// Be ready for 3rd party worker libraries logging to stdout.
		this.#child.stdout!.on('data', buffer => {
			for (const line of buffer.toString('utf8').split('\n')) {
				if (line) {
					workerLogger.debug(`(stdout) ${line}`);
				}
			}
		});

		// In case of a worker bug, mediasoup will log to stderr.
		this.#child.stderr!.on('data', buffer => {
			for (const line of buffer.toString('utf8').split('\n')) {
				if (line) {
					workerLogger.error(`(stderr) ${line}`);
				}
			}
		});
	}

	/**
	 * Worker process identifier (PID).
	 */
	get pid(): number {
		return this.#pid;
	}

	/**
	 * Whether the Worker is closed.
	 */
	get closed(): boolean {
		return this.#closed;
	}

	/**
	 * Whether the Worker died.
	 */
	get died(): boolean {
		return this.#died;
	}

	/**
	 * Whether the Worker subprocess is closed.
	 */
	get subprocessClosed(): boolean {
		return this.#subprocessClosed;
	}

	/**
	 * App custom data.
	 */
	get appData(): WorkerAppData {
		return this.#appData;
	}

	/**
	 * App custom data setter.
	 */
	set appData(appData: WorkerAppData) {
		this.#appData = appData;
	}

	/**
	 * Observer.
	 */
	get observer(): WorkerObserver {
		return this.#observer;
	}

	/**
	 * Just for testing purposes.
	 *
	 * @private
	 */
	get webRtcServersForTesting(): Set<WebRtcServerInterface> {
		return this.#webRtcServers;
	}

	/**
	 * Just for testing purposes.
	 *
	 * @private
	 */
	get routersForTesting(): Set<RouterInterface> {
		return this.#routers;
	}

	/**
	 * Close the Worker.
	 */
	close(): void {
		if (this.#closed) {
			return;
		}

		logger.debug('close()');

		this.#closed = true;

		// Kill the worker process.
		this.#child.kill('SIGTERM');

		// Close the Channel instance.
		this.#channel.close();

		// Close every Router.
		for (const router of this.#routers) {
			router.workerClosed();
		}
		this.#routers.clear();

		// Close every WebRtcServer.
		for (const webRtcServer of this.#webRtcServers) {
			webRtcServer.workerClosed();
		}
		this.#webRtcServers.clear();

		// Emit observer event.
		this.#observer.safeEmit('close');
	}

	/**
	 * Dump Worker.
	 */
	async dump(): Promise<WorkerDump> {
		logger.debug('dump()');

		// Send the request and wait for the response.
		const response = await this.#channel.request(FbsRequest.Method.WORKER_DUMP);

		/* Decode Response. */
		const dump = new FbsWorker.DumpResponse();

		response.body(dump);

		return parseWorkerDumpResponse(dump);
	}

	/**
	 * Get mediasoup-worker process resource usage.
	 */
	async getResourceUsage(): Promise<WorkerResourceUsage> {
		logger.debug('getResourceUsage()');

		const response = await this.#channel.request(
			FbsRequest.Method.WORKER_GET_RESOURCE_USAGE
		);

		/* Decode Response. */
		const resourceUsage = new FbsWorker.ResourceUsageResponse();

		response.body(resourceUsage);

		const ru = resourceUsage.unpack();

		return {
			ru_utime: Number(ru.ruUtime),
			ru_stime: Number(ru.ruStime),
			ru_maxrss: Number(ru.ruMaxrss),
			ru_ixrss: Number(ru.ruIxrss),
			ru_idrss: Number(ru.ruIdrss),
			ru_isrss: Number(ru.ruIsrss),
			ru_minflt: Number(ru.ruMinflt),
			ru_majflt: Number(ru.ruMajflt),
			ru_nswap: Number(ru.ruNswap),
			ru_inblock: Number(ru.ruInblock),
			ru_oublock: Number(ru.ruOublock),
			ru_msgsnd: Number(ru.ruMsgsnd),
			ru_msgrcv: Number(ru.ruMsgrcv),
			ru_nsignals: Number(ru.ruNsignals),
			ru_nvcsw: Number(ru.ruNvcsw),
			ru_nivcsw: Number(ru.ruNivcsw),
		};
	}

	/**
	 * Update settings.
	 */
	async updateSettings({
		logLevel,
		logTags,
	}: WorkerUpdateableSettings<WorkerAppData> = {}): Promise<void> {
		logger.debug('updateSettings()');

		// Build the request.
		const requestOffset = new FbsWorker.UpdateSettingsRequestT(
			logLevel,
			logTags
		).pack(this.#channel.bufferBuilder);

		await this.#channel.request(
			FbsRequest.Method.WORKER_UPDATE_SETTINGS,
			FbsRequest.Body.Worker_UpdateSettingsRequest,
			requestOffset
		);
	}

	/**
	 * Create a WebRtcServer.
	 */
	async createWebRtcServer<WebRtcServerAppData extends AppData = AppData>({
		listenInfos,
		appData,
	}: WebRtcServerOptions<WebRtcServerAppData>): Promise<
		WebRtcServerInterface<WebRtcServerAppData>
	> {
		logger.debug('createWebRtcServer()');

		if (appData && typeof appData !== 'object') {
			throw new TypeError('if given, appData must be an object');
		}

		// Build the request.
		const fbsListenInfos: FbsTransport.ListenInfoT[] = [];

		for (const listenInfo of listenInfos) {
			fbsListenInfos.push(
				new FbsTransport.ListenInfoT(
					listenInfo.protocol === 'udp'
						? FbsTransportProtocol.UDP
						: FbsTransportProtocol.TCP,
					listenInfo.ip,
					listenInfo.announcedAddress ?? listenInfo.announcedIp,
					listenInfo.port,
					portRangeToFbs(listenInfo.portRange),
					socketFlagsToFbs(listenInfo.flags),
					listenInfo.sendBufferSize,
					listenInfo.recvBufferSize
				)
			);
		}

		const webRtcServerId = utils.generateUUIDv4();

		const createWebRtcServerRequestOffset =
			new FbsWorker.CreateWebRtcServerRequestT(
				webRtcServerId,
				fbsListenInfos
			).pack(this.#channel.bufferBuilder);

		await this.#channel.request(
			FbsRequest.Method.WORKER_CREATE_WEBRTCSERVER,
			FbsRequest.Body.Worker_CreateWebRtcServerRequest,
			createWebRtcServerRequestOffset
		);

		const webRtcServer: WebRtcServerInterface<WebRtcServerAppData> =
			new WebRtcServerImpl({
				internal: { webRtcServerId },
				channel: this.#channel,
				appData,
			});

		this.#webRtcServers.add(webRtcServer);
		webRtcServer.on('@close', () => this.#webRtcServers.delete(webRtcServer));

		// Emit observer event.
		this.#observer.safeEmit('newwebrtcserver', webRtcServer);

		return webRtcServer;
	}

	/**
	 * Create a Router.
	 */
	async createRouter<RouterAppData extends AppData = AppData>({
		mediaCodecs,
		appData,
	}: RouterOptions<RouterAppData> = {}): Promise<
		RouterInterface<RouterAppData>
	> {
		logger.debug('createRouter()');

		if (appData && typeof appData !== 'object') {
			throw new TypeError('if given, appData must be an object');
		}

		// Clone given media codecs to not modify input data.
		const clonedMediaCodecs = utils.clone<RtpCodecCapability[] | undefined>(
			mediaCodecs
		);

		// This may throw.
		const rtpCapabilities =
			ortc.generateRouterRtpCapabilities(clonedMediaCodecs);

		const routerId = utils.generateUUIDv4();

		// Get flatbuffer builder.
		const createRouterRequestOffset = new FbsWorker.CreateRouterRequestT(
			routerId
		).pack(this.#channel.bufferBuilder);

		await this.#channel.request(
			FbsRequest.Method.WORKER_CREATE_ROUTER,
			FbsRequest.Body.Worker_CreateRouterRequest,
			createRouterRequestOffset
		);

		const data = { rtpCapabilities };
		const router: RouterInterface<RouterAppData> = new RouterImpl({
			internal: {
				routerId,
			},
			data,
			channel: this.#channel,
			appData,
		});

		this.#routers.add(router);
		router.on('@close', () => this.#routers.delete(router));

		// Emit observer event.
		this.#observer.safeEmit('newrouter', router);

		return router;
	}

	private workerDied(error: Error): void {
		if (this.#closed) {
			return;
		}

		logger.debug(`died() [error:${error.toString()}]`);

		this.#closed = true;
		this.#died = true;

		// Close the Channel instance.
		this.#channel.close();

		// Close every Router.
		for (const router of this.#routers) {
			router.workerClosed();
		}
		this.#routers.clear();

		// Close every WebRtcServer.
		for (const webRtcServer of this.#webRtcServers) {
			webRtcServer.workerClosed();
		}
		this.#webRtcServers.clear();

		this.safeEmit('died', error);

		// Emit observer event.
		this.#observer.safeEmit('close');
	}
}

export function parseWorkerDumpResponse(
	binary: FbsWorker.DumpResponse
): WorkerDump {
	const dump: WorkerDump = {
		pid: binary.pid(),
		webRtcServerIds: utils.parseVector(binary, 'webRtcServerIds'),
		routerIds: utils.parseVector(binary, 'routerIds'),
		channelMessageHandlers: {
			channelRequestHandlers: utils.parseVector(
				binary.channelMessageHandlers()!,
				'channelRequestHandlers'
			),
			channelNotificationHandlers: utils.parseVector(
				binary.channelMessageHandlers()!,
				'channelNotificationHandlers'
			),
		},
	};

	if (binary.liburing()) {
		dump.liburing = {
			sqeProcessCount: Number(binary.liburing()!.sqeProcessCount()),
			sqeMissCount: Number(binary.liburing()!.sqeMissCount()),
			userDataMissCount: Number(binary.liburing()!.userDataMissCount()),
		};
	}

	return dump;
}
