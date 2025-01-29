import type { EnhancedEventEmitter } from './enhancedEvents';
import type { TransportListenInfo } from './TransportTypes';
import type { WebRtcTransport } from './WebRtcTransportTypes';
import type { AppData } from './types';

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
 * @deprecated Use TransportListenInfo instead.
 */
export type WebRtcServerListenInfo = TransportListenInfo;

export type IpPort = {
	ip: string;
	port: number;
};

export type IceUserNameFragment = {
	localIceUsernameFragment: string;
	webRtcTransportId: string;
};

export type TupleHash = {
	tupleHash: number;
	webRtcTransportId: string;
};

export type WebRtcServerDump = {
	id: string;
	udpSockets: IpPort[];
	tcpServers: IpPort[];
	webRtcTransportIds: string[];
	localIceUsernameFragments: IceUserNameFragment[];
	tupleHashes: TupleHash[];
};

export type WebRtcServerEvents = {
	workerclose: [];
	// Private events.
	'@close': [];
};

export type WebRtcServerObserver =
	EnhancedEventEmitter<WebRtcServerObserverEvents>;

export type WebRtcServerObserverEvents = {
	close: [];
	webrtctransporthandled: [WebRtcTransport];
	webrtctransportunhandled: [WebRtcTransport];
};

export interface WebRtcServer<WebRtcServerAppData extends AppData = AppData>
	extends EnhancedEventEmitter<WebRtcServerEvents> {
	/**
	 * WebRtcServer id.
	 */
	get id(): string;

	/**
	 * Whether the WebRtcServer is closed.
	 */
	get closed(): boolean;

	/**
	 * App custom data.
	 */
	get appData(): WebRtcServerAppData;

	/**
	 * App custom data setter.
	 */
	set appData(appData: WebRtcServerAppData);

	/**
	 * Observer.
	 */
	get observer(): WebRtcServerObserver;

	/**
	 * Close the WebRtcServer.
	 */
	close(): void;

	/**
	 * Worker was closed.
	 *
	 * @private
	 */
	workerClosed(): void;

	/**
	 * Dump WebRtcServer.
	 */
	dump(): Promise<WebRtcServerDump>;

	/**
	 * @private
	 */
	handleWebRtcTransport(webRtcTransport: WebRtcTransport): void;
}
