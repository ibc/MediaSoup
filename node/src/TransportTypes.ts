import type { EnhancedEventEmitter } from './enhancedEvents';
import type { Producer, ProducerOptions } from './ProducerTypes';
import type { Consumer, ConsumerOptions } from './ConsumerTypes';
import type { DataProducer, DataProducerOptions } from './DataProducerTypes';
import type { DataConsumer, DataConsumerOptions } from './DataConsumerTypes';
import type { SctpParameters } from './sctpParametersTypes';
import type { AppData } from './types';

/**
 * Transport type.
 */
export type TransportType = 'webrtc' | 'plain' | 'pipe' | 'direct';

export type TransportListenInfo = {
	/**
	 * Network protocol.
	 */
	protocol: TransportProtocol;

	/**
	 * Listening IPv4 or IPv6.
	 */
	ip: string;

	/**
	 * @deprecated Use |announcedAddress| instead.
	 *
	 * Announced IPv4, IPv6 or hostname (useful when running mediasoup behind NAT
	 * with private IP).
	 */
	announcedIp?: string;

	/**
	 * Announced IPv4, IPv6 or hostname (useful when running mediasoup behind NAT
	 * with private IP).
	 */
	announcedAddress?: string;

	/**
	 * Listening port.
	 */
	port?: number;

	/**
	 * Listening port range. If given then |port| will be ignored.
	 */
	portRange?: TransportPortRange;

	/**
	 * Socket flags.
	 */
	flags?: TransportSocketFlags;

	/**
	 * Send buffer size (bytes).
	 */
	sendBufferSize?: number;

	/**
	 * Recv buffer size (bytes).
	 */
	recvBufferSize?: number;
};

/**
 * Use TransportListenInfo instead.
 * @deprecated
 */
export type TransportListenIp = {
	/**
	 * Listening IPv4 or IPv6.
	 */
	ip: string;

	/**
	 * Announced IPv4, IPv6 or hostname (useful when running mediasoup behind NAT
	 * with private IP).
	 */
	announcedIp?: string;
};

/**
 * Transport protocol.
 */
export type TransportProtocol = 'udp' | 'tcp';

/**
 * Port range..
 */
export type TransportPortRange = {
	/**
	 * Lowest port in the range.
	 */
	min: number;
	/**
	 * Highest port in the range.
	 */
	max: number;
};

/**
 * UDP/TCP socket flags.
 */
export type TransportSocketFlags = {
	/**
	 * Disable dual-stack support so only IPv6 is used (only if |ip| is IPv6).
	 */
	ipv6Only?: boolean;
	/**
	 * Make different transports bind to the same IP and port (only for UDP).
	 * Useful for multicast scenarios with plain transport. Use with caution.
	 */
	udpReusePort?: boolean;
};

export type TransportTuple = {
	// @deprecated Use localAddress instead.
	localIp: string;
	localAddress: string;
	localPort: number;
	remoteIp?: string;
	remotePort?: number;
	protocol: TransportProtocol;
};

export type SctpState =
	| 'new'
	| 'connecting'
	| 'connected'
	| 'failed'
	| 'closed';

export type RtpListenerDump = {
	ssrcTable: { key: number; value: string }[];
	midTable: { key: number; value: string }[];
	ridTable: { key: number; value: string }[];
};

export type SctpListenerDump = {
	streamIdTable: { key: number; value: string }[];
};

export type RecvRtpHeaderExtensions = {
	mid?: number;
	rid?: number;
	rrid?: number;
	absSendTime?: number;
	transportWideCc01?: number;
};

export type BaseTransportDump = {
	id: string;
	producerIds: string[];
	consumerIds: string[];
	mapSsrcConsumerId: { key: number; value: string }[];
	mapRtxSsrcConsumerId: { key: number; value: string }[];
	recvRtpHeaderExtensions: RecvRtpHeaderExtensions;
	rtpListener: RtpListenerDump;
	maxMessageSize: number;
	dataProducerIds: string[];
	dataConsumerIds: string[];
	sctpParameters?: SctpParameters;
	sctpState?: SctpState;
	sctpListener?: SctpListenerDump;
	traceEventTypes?: string[];
};

export type BaseTransportStats = {
	transportId: string;
	timestamp: number;
	sctpState?: SctpState;
	bytesReceived: number;
	recvBitrate: number;
	bytesSent: number;
	sendBitrate: number;
	rtpBytesReceived: number;
	rtpRecvBitrate: number;
	rtpBytesSent: number;
	rtpSendBitrate: number;
	rtxBytesReceived: number;
	rtxRecvBitrate: number;
	rtxBytesSent: number;
	rtxSendBitrate: number;
	probationBytesSent: number;
	probationSendBitrate: number;
	availableOutgoingBitrate?: number;
	availableIncomingBitrate?: number;
	maxIncomingBitrate?: number;
	maxOutgoingBitrate?: number;
	minOutgoingBitrate?: number;
	rtpPacketLossReceived?: number;
	rtpPacketLossSent?: number;
};

/**
 * Valid types for 'trace' event.
 */
export type TransportTraceEventType = 'probation' | 'bwe';

/**
 * 'trace' event data.
 */
export type TransportTraceEventData = {
	/**
	 * Trace type.
	 */
	type: TransportTraceEventType;

	/**
	 * Event timestamp.
	 */
	timestamp: number;

	/**
	 * Event direction.
	 */
	direction: 'in' | 'out';

	/**
	 * Per type information.
	 */
	info: unknown;
};

export type TransportEvents = {
	routerclose: [];
	listenserverclose: [];
	trace: [TransportTraceEventData];
	listenererror: [string, Error];
	// Private events.
	'@close': [];
	'@newproducer': [Producer];
	'@producerclose': [Producer];
	'@newdataproducer': [DataProducer];
	'@dataproducerclose': [DataProducer];
	'@listenserverclose': [];
};

export type TransportObserver = EnhancedEventEmitter<TransportObserverEvents>;

export type TransportObserverEvents = {
	close: [];
	newproducer: [Producer];
	newconsumer: [Consumer];
	newdataproducer: [DataProducer];
	newdataconsumer: [DataConsumer];
	trace: [TransportTraceEventData];
};

export interface Transport<
	TransportAppData extends AppData = AppData,
	Events extends TransportEvents = TransportEvents,
	Observer extends TransportObserver = TransportObserver,
> extends EnhancedEventEmitter<Events> {
	/**
	 * Transport id.
	 */
	get id(): string;

	/**
	 * Whether the Transport is closed.
	 */
	get closed(): boolean;

	/**
	 * Transport type.
	 *
	 * @virtual
	 * @privateRemarks
	 * - It's marked as virtual getter since each Transport class overrides it.
	 */
	get type(): TransportType;

	/**
	 * App custom data.
	 */
	get appData(): TransportAppData;

	/**
	 * App custom data setter.
	 */
	set appData(appData: TransportAppData);

	/**
	 * Observer.
	 *
	 * @virtual
	 */
	get observer(): Observer;

	/**
	 * Close the Transport.
	 *
	 * @virtual
	 */
	close(): void;

	/**
	 * Router was closed.
	 *
	 * @private
	 * @virtual
	 */
	routerClosed(): void;

	/**
	 * Listen server was closed (this just happens in WebRtcTransports when their
	 * associated WebRtcServer is closed).
	 *
	 * @private
	 * @virtual
	 */
	listenServerClosed(): void;

	/**
	 * Dump Transport.
	 *
	 * @abstract
	 */
	dump(): Promise<unknown>;

	/**
	 * Get Transport stats.
	 *
	 * @abstract
	 */
	getStats(): Promise<unknown[]>;

	/**
	 * Provide the Transport remote parameters.
	 *
	 * @abstract
	 */
	connect(params: unknown): Promise<void>;

	/**
	 * Set maximum incoming bitrate for receiving media.
	 *
	 * @virtual
	 * @privateRemarks
	 * - It's marked as virtual method because DirectTransport overrides it.
	 */
	setMaxIncomingBitrate(bitrate: number): Promise<void>;

	/**
	 * Set maximum outgoing bitrate for sending media.
	 *
	 * @virtual
	 * @privateRemarks
	 * - It's marked as virtual method because DirectTransport overrides it.
	 */
	setMaxOutgoingBitrate(bitrate: number): Promise<void>;

	/**
	 * Set minimum outgoing bitrate for sending media.
	 *
	 * @virtual
	 * @privateRemarks
	 * - It's marked as virtual method because DirectTransport overrides it.
	 */
	setMinOutgoingBitrate(bitrate: number): Promise<void>;

	/**
	 * Create a Producer.
	 */
	produce<ProducerAppData extends AppData = AppData>(
		options: ProducerOptions<ProducerAppData>
	): Promise<Producer<ProducerAppData>>;

	/**
	 * Create a Consumer.
	 *
	 * @virtual
	 * @privateRemarks
	 * - It's marked as virtual method because PipeTransport overrides it.
	 */
	consume<ConsumerAppData extends AppData = AppData>(
		options: ConsumerOptions<ConsumerAppData>
	): Promise<Consumer<ConsumerAppData>>;

	/**
	 * Create a DataProducer.
	 */
	produceData<DataProducerAppData extends AppData = AppData>(
		options?: DataProducerOptions<DataProducerAppData>
	): Promise<DataProducer<DataProducerAppData>>;

	/**
	 * Create a DataConsumer.
	 */
	consumeData<DataConsumerAppData extends AppData = AppData>(
		options: DataConsumerOptions<DataConsumerAppData>
	): Promise<DataConsumer<DataConsumerAppData>>;

	/**
	 * Enable 'trace' event.
	 */
	enableTraceEvent(types?: TransportTraceEventType[]): Promise<void>;
}
