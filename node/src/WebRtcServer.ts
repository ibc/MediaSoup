import { Logger } from './Logger';
import { EnhancedEventEmitter } from './enhancedEvents';
import { Channel } from './Channel';
import {
	WebRtcServerInterface,
	IpPort,
	IceUserNameFragment,
	TupleHash,
	WebRtcServerDump,
	WebRtcServerEvents,
	WebRtcServerObserver,
	WebRtcServerObserverEvents,
} from './WebRtcServerTypes';
import { WebRtcTransportInterface } from './WebRtcTransportTypes';
import { AppData } from './types';
import * as utils from './utils';
import { Body as RequestBody, Method } from './fbs/request';
import * as FbsWorker from './fbs/worker';
import * as FbsWebRtcServer from './fbs/web-rtc-server';

type WebRtcServerInternal = {
	webRtcServerId: string;
};

const logger = new Logger('WebRtcServer');

export class WebRtcServerImpl<WebRtcServerAppData extends AppData = AppData>
	extends EnhancedEventEmitter<WebRtcServerEvents>
	implements WebRtcServerInterface
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
	readonly #webRtcTransports: Map<string, WebRtcTransportInterface> = new Map();

	// Observer instance.
	readonly #observer: WebRtcServerObserver =
		new EnhancedEventEmitter<WebRtcServerObserverEvents>();

	constructor({
		internal,
		channel,
		appData,
	}: {
		internal: WebRtcServerInternal;
		channel: Channel;
		appData?: WebRtcServerAppData;
	}) {
		super();

		logger.debug('constructor()');

		this.#internal = internal;
		this.#channel = channel;
		this.#appData = appData ?? ({} as WebRtcServerAppData);
	}

	get id(): string {
		return this.#internal.webRtcServerId;
	}

	get closed(): boolean {
		return this.#closed;
	}

	get appData(): WebRtcServerAppData {
		return this.#appData;
	}

	set appData(appData: WebRtcServerAppData) {
		this.#appData = appData;
	}

	get observer(): WebRtcServerObserver {
		return this.#observer;
	}

	/**
	 * Just for testing purposes.
	 */
	get webRtcTransportsForTesting(): Map<string, WebRtcTransportInterface> {
		return this.#webRtcTransports;
	}

	close(): void {
		if (this.#closed) {
			return;
		}

		logger.debug('close()');

		this.#closed = true;

		// Build the request.
		const requestOffset = new FbsWorker.CloseWebRtcServerRequestT(
			this.#internal.webRtcServerId
		).pack(this.#channel.bufferBuilder);

		this.#channel
			.request(
				Method.WORKER_WEBRTCSERVER_CLOSE,
				RequestBody.Worker_CloseWebRtcServerRequest,
				requestOffset
			)
			.catch(() => {});

		// Close every WebRtcTransport.
		for (const webRtcTransport of this.#webRtcTransports.values()) {
			webRtcTransport.listenServerClosed();

			// Emit observer event.
			this.#observer.safeEmit('webrtctransportunhandled', webRtcTransport);
		}
		this.#webRtcTransports.clear();

		this.emit('@close');

		// Emit observer event.
		this.#observer.safeEmit('close');
	}

	workerClosed(): void {
		if (this.#closed) {
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

	async dump(): Promise<WebRtcServerDump> {
		logger.debug('dump()');

		const response = await this.#channel.request(
			Method.WEBRTCSERVER_DUMP,
			undefined,
			undefined,
			this.#internal.webRtcServerId
		);

		/* Decode Response. */
		const dump = new FbsWebRtcServer.DumpResponse();

		response.body(dump);

		return parseWebRtcServerDump(dump);
	}

	handleWebRtcTransport(webRtcTransport: WebRtcTransportInterface): void {
		this.#webRtcTransports.set(webRtcTransport.id, webRtcTransport);

		// Emit observer event.
		this.#observer.safeEmit('webrtctransporthandled', webRtcTransport);

		webRtcTransport.on('@close', () => {
			this.#webRtcTransports.delete(webRtcTransport.id);

			// Emit observer event.
			this.#observer.safeEmit('webrtctransportunhandled', webRtcTransport);
		});
	}
}

function parseIpPort(binary: FbsWebRtcServer.IpPort): IpPort {
	return {
		ip: binary.ip()!,
		port: binary.port(),
	};
}

function parseIceUserNameFragment(
	binary: FbsWebRtcServer.IceUserNameFragment
): IceUserNameFragment {
	return {
		localIceUsernameFragment: binary.localIceUsernameFragment()!,
		webRtcTransportId: binary.webRtcTransportId()!,
	};
}

function parseTupleHash(binary: FbsWebRtcServer.TupleHash): TupleHash {
	return {
		tupleHash: Number(binary.tupleHash()),
		webRtcTransportId: binary.webRtcTransportId()!,
	};
}

function parseWebRtcServerDump(
	data: FbsWebRtcServer.DumpResponse
): WebRtcServerDump {
	return {
		id: data.id()!,
		udpSockets: utils.parseVector(data, 'udpSockets', parseIpPort),
		tcpServers: utils.parseVector(data, 'tcpServers', parseIpPort),
		webRtcTransportIds: utils.parseVector(data, 'webRtcTransportIds'),
		localIceUsernameFragments: utils.parseVector(
			data,
			'localIceUsernameFragments',
			parseIceUserNameFragment
		),
		tupleHashes: utils.parseVector(data, 'tupleHashes', parseTupleHash),
	};
}
