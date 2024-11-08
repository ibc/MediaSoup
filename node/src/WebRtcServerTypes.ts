import { EnhancedEventEmitter } from './enhancedEvents';
import { TransportListenInfo } from './TransportTypes';
import { WebRtcTransportInterface } from './WebRtcTransportTypes';
import { AppData } from './types';

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
	listenererror: [string, Error];
	// Private events.
	'@close': [];
};

export type WebRtcServerObserver =
	EnhancedEventEmitter<WebRtcServerObserverEvents>;

export type WebRtcServerObserverEvents = {
	close: [];
	webrtctransporthandled: [WebRtcTransportInterface];
	webrtctransportunhandled: [WebRtcTransportInterface];
};

export interface WebRtcServerInterface<
	WebRtcServerAppData extends AppData = AppData,
> extends EnhancedEventEmitter<WebRtcServerEvents> {
	get id(): string;

	get closed(): boolean;

	get appData(): WebRtcServerAppData;

	/**
	 * App custom data setter.
	 */
	set appData(appData: WebRtcServerAppData);

	/**
	 * Observer.
	 */
	get observer(): WebRtcServerObserver;

	close(): void;

	/**
	 * Worker was closed.
	 *
	 * @private
	 */
	workerClosed(): void;

	dump(): Promise<WebRtcServerDump>;

	/**
	 * @private
	 */
	handleWebRtcTransport(webRtcTransport: WebRtcTransportInterface): void;
}
