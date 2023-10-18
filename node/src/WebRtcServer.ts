import { Logger } from './Logger';
import { EnhancedEventEmitter } from './EnhancedEventEmitter';
import { Channel } from './Channel';
import { TransportListenInfo } from './Transport';
import { WebRtcTransport } from './WebRtcTransport';
import { AppData } from './types';
import * as utils from './utils';
import { Body as RequestBody, Method } from './fbs/request';
import * as FbsWorker from './fbs/worker';
import * as FbsWebRtcServer from './fbs/web-rtc-server';

export type WebRtcServerOptions<WebRtcServerAppData extends AppData = AppData> =
{
	/**
	 * Listen infos.
	 */
	listenInfos: TransportListenInfo[];

	/**
	 * Custom application data.
	 */
	appData?: WebRtcServerAppData;
};

/**
 * @deprecated
 * Use TransportListenInfo instead.
 */
export type WebRtcServerListenInfo = TransportListenInfo;

export type WebRtcServerEvents =
{ 
	workerclose: [];
	// Private events.
	'@close': [];
};

export type WebRtcServerObserverEvents =
{
	close: [];
	webrtctransporthandled: [WebRtcTransport];
	webrtctransportunhandled: [WebRtcTransport];
};

type WebRtcServerInternal =
{
	webRtcServerId: string;
};

const logger = new Logger('WebRtcServer');

export class WebRtcServer<WebRtcServerAppData extends AppData = AppData>
	extends EnhancedEventEmitter<WebRtcServerEvents>
{
	// Internal data.
	readonly #internal: WebRtcServerInternal;

	// Channel instance.
	readonly #channel: Channel;

	// Closed flag.
	#closed = false;

	// Custom app data.
	#appData: WebRtcServerAppData;

	// Transports map.
	readonly #webRtcTransports: Map<string, WebRtcTransport> = new Map();

	// Observer instance.
	readonly #observer = new EnhancedEventEmitter<WebRtcServerObserverEvents>();

	/**
	 * @private
	 */
	constructor(
		{
			internal,
			channel,
			appData
		}:
		{
			internal: WebRtcServerInternal;
			channel: Channel;
			appData?: WebRtcServerAppData;
		}
	)
	{
		super();

		logger.debug('constructor()');

		this.#internal = internal;
		this.#channel = channel;
		this.#appData = appData || {} as WebRtcServerAppData;
	}

	/**
	 * WebRtcServer id.
	 */
	get id(): string
	{
		return this.#internal.webRtcServerId;
	}

	/**
	 * Whether the WebRtcServer is closed.
	 */
	get closed(): boolean
	{
		return this.#closed;
	}

	/**
	 * App custom data.
	 */
	get appData(): WebRtcServerAppData
	{
		return this.#appData;
	}

	/**
	 * App custom data setter.
	 */
	set appData(appData: WebRtcServerAppData)
	{
		this.#appData = appData;
	}

	/**
	 * Observer.
	 */
	get observer(): EnhancedEventEmitter<WebRtcServerObserverEvents>
	{
		return this.#observer;
	}

	/**
	 * @private
	 * Just for testing purposes.
	 */
	get webRtcTransportsForTesting(): Map<string, WebRtcTransport>
	{
		return this.#webRtcTransports;
	}

	/**
	 * Close the WebRtcServer.
	 */
	close(): void
	{
		if (this.#closed)
		{
			return;
		}

		logger.debug('close()');

		this.#closed = true;

		// Build the request.
		const requestOffset = new FbsWorker.CloseWebRtcServerRequestT(
			this.#internal.webRtcServerId).pack(this.#channel.bufferBuilder);

		this.#channel.request(
			Method.WORKER_WEBRTCSERVER_CLOSE,
			RequestBody.Worker_CloseWebRtcServerRequest,
			requestOffset)
			.catch(() => {});

		// Close every WebRtcTransport.
		for (const webRtcTransport of this.#webRtcTransports.values())
		{
			webRtcTransport.listenServerClosed();

			// Emit observer event.
			this.#observer.safeEmit('webrtctransportunhandled', webRtcTransport);
		}
		this.#webRtcTransports.clear();

		this.emit('@close');

		// Emit observer event.
		this.#observer.safeEmit('close');
	}

	/**
	 * Worker was closed.
	 *
	 * @private
	 */
	workerClosed(): void
	{
		if (this.#closed)
		{
			return;
		}

		logger.debug('workerClosed()');

		this.#closed = true;

		// NOTE: No need to close WebRtcTransports since they are closed by their
		// respective Router parents.
		this.#webRtcTransports.clear();

		this.safeEmit('workerclose');

		// Emit observer event.
		this.#observer.safeEmit('close');
	}

	/**
	 * Dump WebRtcServer.
	 */
	async dump(): Promise<any>
	{
		logger.debug('dump()');

		const response = await this.#channel.request(
			Method.WEBRTCSERVER_DUMP, undefined, undefined, this.#internal.webRtcServerId);

		/* Decode Response. */
		const dump = new FbsWebRtcServer.DumpResponse();

		response.body(dump);

		return parseWebRtcServerDump(dump);
	}

	/**
	 * @private
	 */
	handleWebRtcTransport(webRtcTransport: WebRtcTransport): void
	{
		this.#webRtcTransports.set(webRtcTransport.id, webRtcTransport);

		// Emit observer event.
		this.#observer.safeEmit('webrtctransporthandled', webRtcTransport);

		webRtcTransport.on('@close', () =>
		{
			this.#webRtcTransports.delete(webRtcTransport.id);

			// Emit observer event.
			this.#observer.safeEmit('webrtctransportunhandled', webRtcTransport);
		});
	}
}

// TODO: This function should return WebRtcServerDump TypeScript type but we
// don't have it yet (same for many other dump() methods everywhere).
function parseWebRtcServerDump(
	data: FbsWebRtcServer.DumpResponse
): any
{
	return {
		id                        : data.id(),
		udpSockets                : utils.parseVector(
			data, 'udpSockets', (udpSocket: any) => udpSocket.unpack()
		),
		tcpServers                : utils.parseVector(
			data, 'tcpServers', (tcpServer: any) => tcpServer.unpack()
		),
		webRtcTransportIds        : utils.parseVector(data, 'webRtcTransportIds'),
		localIceUsernameFragments : utils.parseVector(
			data, 'localIceUsernameFragments', (localIceUsernameFragment: any) => localIceUsernameFragment.unpack()
		),
		tupleHashes               : utils.parseVector(
			data, 'tupleHashes', (tupleHash: any) =>
			{
				return {
					localIceUsernameFragment : Number(tupleHash.localIceUsernameFragment()),
					webRtcTransportId        : tupleHash.webRtcTransportId()
				};
			}
		)
	};
}
