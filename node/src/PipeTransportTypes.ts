import { EnhancedEventEmitter } from './enhancedEvents';
import {
	Transport,
	TransportListenInfo,
	TransportListenIp,
	TransportTuple,
	SctpState,
	BaseTransportDump,
	BaseTransportStats,
	TransportEvents,
	TransportObserverEvents,
} from './TransportTypes';
import { Consumer } from './ConsumerTypes';
import { SrtpParameters } from './SrtpParameters';
import { SctpParameters, NumSctpStreams } from './SctpParameters';
import { Either, AppData } from './types';

export type PipeTransportOptions<
	PipeTransportAppData extends AppData = AppData,
> = {
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
	 * Default 268435456.
	 */
	maxSctpMessageSize?: number;

	/**
	 * Maximum SCTP send buffer used by DataConsumers.
	 * Default 268435456.
	 */
	sctpSendBufferSize?: number;

	/**
	 * Enable RTX and NACK for RTP retransmission. Useful if both Routers are
	 * located in different hosts and there is packet lost in the link. For this
	 * to work, both PipeTransports must enable this setting. Default false.
	 */
	enableRtx?: boolean;

	/**
	 * Enable SRTP. Useful to protect the RTP and RTCP traffic if both Routers
	 * are located in different hosts. For this to work, connect() must be called
	 * with remote SRTP parameters. Default false.
	 */
	enableSrtp?: boolean;

	/**
	 * Custom application data.
	 */
	appData?: PipeTransportAppData;
} & PipeTransportListen;

type PipeTransportListen = Either<
	PipeTransportListenInfo,
	PipeTransportListenIp
>;

type PipeTransportListenInfo = {
	/**
	 * Listening info.
	 */
	listenInfo: TransportListenInfo;
};

type PipeTransportListenIp = {
	/**
	 * Listening IP address.
	 */
	listenIp: TransportListenIp | string;

	/**
	 * Fixed port to listen on instead of selecting automatically from Worker's
	 * port range.
	 */
	port?: number;
};

export type PipeConsumerOptions<ConsumerAppData> = {
	/**
	 * The id of the Producer to consume.
	 */
	producerId: string;

	/**
	 * Custom application data.
	 */
	appData?: ConsumerAppData;
};

export type PipeTransportDump = BaseTransportDump & {
	tuple: TransportTuple;
	rtx: boolean;
	srtpParameters?: SrtpParameters;
};

export type PipeTransportStat = BaseTransportStats & {
	type: string;
	tuple: TransportTuple;
};

export type PipeTransportEvents = TransportEvents & {
	sctpstatechange: [SctpState];
};

export type PipeTransportObserver =
	EnhancedEventEmitter<PipeTransportObserverEvents>;

export type PipeTransportObserverEvents = TransportObserverEvents & {
	sctpstatechange: [SctpState];
};

export interface PipeTransport<PipeTransportAppData extends AppData = AppData>
	extends Transport<
		PipeTransportAppData,
		PipeTransportEvents,
		PipeTransportObserver
	> {
	/**
	 * Observer.
	 *
	 * @override
	 */
	get observer(): PipeTransportObserver;

	/**
	 * PipeTransport tuple.
	 */
	get tuple(): TransportTuple;

	/**
	 * SCTP parameters.
	 */
	get sctpParameters(): SctpParameters | undefined;

	/**
	 * SCTP state.
	 */
	get sctpState(): SctpState | undefined;

	/**
	 * SRTP parameters.
	 */
	get srtpParameters(): SrtpParameters | undefined;

	/**
	 * Dump PipeTransport.
	 *
	 * @override
	 */
	dump(): Promise<PipeTransportDump>;

	/**
	 * Get PipeTransport stats.
	 *
	 * @override
	 */
	getStats(): Promise<PipeTransportStat[]>;

	/**
	 * Provide the PipeTransport remote parameters.
	 *
	 * @override
	 */
	connect({
		ip,
		port,
		srtpParameters,
	}: {
		ip: string;
		port: number;
		srtpParameters?: SrtpParameters;
	}): Promise<void>;

	/**
	 * Create a pipe Consumer.
	 *
	 * @override
	 */
	consume<ConsumerAppData extends AppData = AppData>({
		producerId,
		appData,
	}: PipeConsumerOptions<ConsumerAppData>): Promise<Consumer<ConsumerAppData>>;
}
