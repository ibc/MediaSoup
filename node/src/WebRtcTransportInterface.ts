import { EnhancedEventEmitter } from './enhancedEvents';
import {
	TransportInterface,
	TransportListenInfo,
	TransportListenIp,
	TransportProtocol,
	TransportTuple,
	SctpState,
	BaseTransportDump,
	BaseTransportStats,
	TransportEvents,
	TransportObserverEvents,
} from './TransportInterface';
import { WebRtcServerInterface } from './WebRtcServerInterface';
import { SctpParameters, NumSctpStreams } from './SctpParameters';
import { Either, AppData } from './types';

export type WebRtcTransportOptions<
	WebRtcTransportAppData extends AppData = AppData,
> = WebRtcTransportOptionsBase<WebRtcTransportAppData> & WebRtcTransportListen;

type WebRtcTransportOptionsBase<WebRtcTransportAppData> = {
	/**
	 * Listen in UDP. Default true.
	 */
	enableUdp?: boolean;

	/**
	 * Listen in TCP. Default true if webrtcServer is given, false otherwise.
	 */
	enableTcp?: boolean;

	/**
	 * Prefer UDP. Default false.
	 */
	preferUdp?: boolean;

	/**
	 * Prefer TCP. Default false.
	 */
	preferTcp?: boolean;

	/**
	 * ICE consent timeout (in seconds). If 0 it is disabled. Default 30.
	 */
	iceConsentTimeout?: number;

	/**
	 * Initial available outgoing bitrate (in bps). Default 600000.
	 */
	initialAvailableOutgoingBitrate?: number;

	/**
	 * Create a SCTP association. Default false.
	 */
	enableSctp?: boolean;

	/**
	 * SCTP streams number.
	 */
	numSctpStreams?: NumSctpStreams;

	/**
	 * Maximum allowed size for SCTP messages sent by DataProducers.
	 * Default 262144.
	 */
	maxSctpMessageSize?: number;

	/**
	 * Maximum SCTP send buffer used by DataConsumers.
	 * Default 262144.
	 */
	sctpSendBufferSize?: number;

	/**
	 * Custom application data.
	 */
	appData?: WebRtcTransportAppData;
};

type WebRtcTransportListen = Either<
	Either<
		WebRtcTransportListenIndividualListenInfo,
		WebRtcTransportListenIndividualListenIp
	>,
	WebRtcTransportListenServer
>;

type WebRtcTransportListenIndividualListenInfo = {
	/**
	 * Listening info.
	 */
	listenInfos: TransportListenInfo[];
};

type WebRtcTransportListenIndividualListenIp = {
	/**
	 * Listening IP address or addresses in order of preference (first one is the
	 * preferred one).
	 */
	listenIps: (TransportListenIp | string)[];

	/**
	 * Fixed port to listen on instead of selecting automatically from Worker's port
	 * range.
	 */
	port?: number;
};

type WebRtcTransportListenServer = {
	/**
	 * Instance of WebRtcServer.
	 */
	webRtcServer: WebRtcServerInterface;
};

export type IceParameters = {
	usernameFragment: string;
	password: string;
	iceLite?: boolean;
};

export type IceCandidate = {
	foundation: string;
	priority: number;
	// @deprecated Use |address| instead.
	ip: string;
	address: string;
	protocol: TransportProtocol;
	port: number;
	type: IceCandidateType;
	tcpType?: IceCandidateTcpType;
};

export type DtlsParameters = {
	role?: DtlsRole;
	fingerprints: DtlsFingerprint[];
};

/**
 * The hash function algorithm (as defined in the "Hash function Textual Names"
 * registry initially specified in RFC 4572 Section 8).
 */
export type FingerprintAlgorithm =
	| 'sha-1'
	| 'sha-224'
	| 'sha-256'
	| 'sha-384'
	| 'sha-512';

/**
 * The hash function algorithm and its corresponding certificate fingerprint
 * value (in lowercase hex string as expressed utilizing the syntax of
 * "fingerprint" in RFC 4572 Section 5).
 */
export type DtlsFingerprint = {
	algorithm: FingerprintAlgorithm;
	value: string;
};

export type IceRole = 'controlled' | 'controlling';

export type IceState =
	| 'new'
	| 'connected'
	| 'completed'
	| 'disconnected'
	| 'closed';

export type IceCandidateType = 'host';

export type IceCandidateTcpType = 'passive';

export type DtlsRole = 'auto' | 'client' | 'server';

export type DtlsState =
	| 'new'
	| 'connecting'
	| 'connected'
	| 'failed'
	| 'closed';

export type WebRtcTransportDump = BaseTransportDump & {
	iceRole: 'controlled';
	iceParameters: IceParameters;
	iceCandidates: IceCandidate[];
	iceState: IceState;
	iceSelectedTuple?: TransportTuple;
	dtlsParameters: DtlsParameters;
	dtlsState: DtlsState;
	dtlsRemoteCert?: string;
};

export type WebRtcTransportStat = BaseTransportStats & {
	type: string;
	iceRole: string;
	iceState: IceState;
	iceSelectedTuple?: TransportTuple;
	dtlsState: DtlsState;
};

export type WebRtcTransportEvents = TransportEvents & {
	icestatechange: [IceState];
	iceselectedtuplechange: [TransportTuple];
	dtlsstatechange: [DtlsState];
	sctpstatechange: [SctpState];
};

export type WebRtcTransportObserver =
	EnhancedEventEmitter<WebRtcTransportObserverEvents>;

export type WebRtcTransportObserverEvents = TransportObserverEvents & {
	icestatechange: [IceState];
	iceselectedtuplechange: [TransportTuple];
	dtlsstatechange: [DtlsState];
	sctpstatechange: [SctpState];
};

export interface WebRtcTransportInterface<
	WebRtcTransportAppData extends AppData = AppData,
> extends TransportInterface<
		WebRtcTransportAppData,
		WebRtcTransportEvents,
		WebRtcTransportObserver
	> {
	get observer(): WebRtcTransportObserver;

	get iceRole(): 'controlled';

	get iceParameters(): IceParameters;

	get iceCandidates(): IceCandidate[];

	get iceState(): IceState;

	/**
	 * ICE selected tuple.
	 */
	get iceSelectedTuple(): TransportTuple | undefined;

	/**
	 * DTLS parameters.
	 */
	get dtlsParameters(): DtlsParameters;

	/**
	 * DTLS state.
	 */
	get dtlsState(): DtlsState;

	/**
	 * Remote certificate in PEM format.
	 */
	get dtlsRemoteCert(): string | undefined;

	/**
	 * SCTP parameters.
	 */
	get sctpParameters(): SctpParameters | undefined;

	/**
	 * SCTP state.
	 */
	get sctpState(): SctpState | undefined;

	dump(): Promise<WebRtcTransportDump>;

	getStats(): Promise<WebRtcTransportStat[]>;

	connect({
		dtlsParameters,
	}: {
		dtlsParameters: DtlsParameters;
	}): Promise<void>;

	restartIce(): Promise<IceParameters>;
}
