import * as flatbuffers from 'flatbuffers';
import { Logger } from './Logger';
import { EnhancedEventEmitter } from './enhancedEvents';
import * as ortc from './ortc';
import { Channel } from './Channel';
import { RouterInternal } from './Router';
import { WebRtcTransportData } from './WebRtcTransport';
import { PlainTransportData } from './PlainTransport';
import { PipeTransportData } from './PipeTransport';
import { DirectTransportData } from './DirectTransport';
import {
	Producer,
	ProducerOptions,
	producerTypeFromFbs,
	producerTypeToFbs,
} from './Producer';
import {
	Consumer,
	ConsumerOptions,
	ConsumerType,
	ConsumerLayers,
} from './Consumer';
import {
	DataProducer,
	DataProducerOptions,
	DataProducerType,
	dataProducerTypeToFbs,
	parseDataProducerDumpResponse,
} from './DataProducer';
import {
	DataConsumer,
	DataConsumerOptions,
	DataConsumerType,
	dataConsumerTypeToFbs,
	parseDataConsumerDumpResponse,
} from './DataConsumer';
import {
	MediaKind,
	RtpCapabilities,
	RtpParameters,
	serializeRtpEncodingParameters,
	serializeRtpParameters,
} from './RtpParameters';
import {
	parseSctpParametersDump,
	serializeSctpStreamParameters,
	SctpParameters,
	SctpStreamParameters,
} from './SctpParameters';
import { AppData } from './types';
import * as utils from './utils';
import { TraceDirection as FbsTraceDirection } from './fbs/common';
import * as FbsRequest from './fbs/request';
import { MediaKind as FbsMediaKind } from './fbs/rtp-parameters/media-kind';
import * as FbsConsumer from './fbs/consumer';
import * as FbsDataConsumer from './fbs/data-consumer';
import * as FbsDataProducer from './fbs/data-producer';
import * as FbsTransport from './fbs/transport';
import * as FbsRouter from './fbs/router';
import * as FbsRtpParameters from './fbs/rtp-parameters';
import { SctpState as FbsSctpState } from './fbs/sctp-association/sctp-state';

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
	info: any;
};

export type SctpState =
	| 'new'
	| 'connecting'
	| 'connected'
	| 'failed'
	| 'closed';

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

export type TransportConstructorOptions<TransportAppData> = {
	internal: TransportInternal;
	data: TransportData;
	channel: Channel;
	appData?: TransportAppData;
	getRouterRtpCapabilities: () => RtpCapabilities;
	getProducerById: (producerId: string) => Producer | undefined;
	getDataProducerById: (dataProducerId: string) => DataProducer | undefined;
};

export type TransportInternal = RouterInternal & {
	transportId: string;
};

export type BaseTransportDump = {
	id: string;
	direct: boolean;
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

type TransportData =
	| WebRtcTransportData
	| PlainTransportData
	| PipeTransportData
	| DirectTransportData;

type RtpListenerDump = {
	ssrcTable: { key: number; value: string }[];
	midTable: { key: number; value: string }[];
	ridTable: { key: number; value: string }[];
};

type SctpListenerDump = {
	streamIdTable: { key: number; value: string }[];
};

type RecvRtpHeaderExtensions = {
	mid?: number;
	rid?: number;
	rrid?: number;
	absSendTime?: number;
	transportWideCc01?: number;
};

const logger = new Logger('Transport');

export abstract class Transport<
	TransportAppData extends AppData = AppData,
	Events extends TransportEvents = TransportEvents,
	Observer extends TransportObserver = TransportObserver,
> extends EnhancedEventEmitter<Events> {
	// Internal data.
	protected readonly internal: TransportInternal;

	// Transport data. This is set by the subclass.
	readonly #data: TransportData;

	// Channel instance.
	protected readonly channel: Channel;

	// Close flag.
	#closed = false;

	// Custom app data.
	#appData: TransportAppData;

	// Method to retrieve Router RTP capabilities.
	readonly #getRouterRtpCapabilities: () => RtpCapabilities;

	// Method to retrieve a Producer.
	protected readonly getProducerById: (
		producerId: string
	) => Producer | undefined;

	// Method to retrieve a DataProducer.
	protected readonly getDataProducerById: (
		dataProducerId: string
	) => DataProducer | undefined;

	// Producers map.
	readonly #producers: Map<string, Producer> = new Map();

	// Consumers map.
	protected readonly consumers: Map<string, Consumer> = new Map();

	// DataProducers map.
	protected readonly dataProducers: Map<string, DataProducer> = new Map();

	// DataConsumers map.
	protected readonly dataConsumers: Map<string, DataConsumer> = new Map();

	// RTCP CNAME for Producers.
	#cnameForProducers?: string;

	// Next MID for Consumers. It's converted into string when used.
	#nextMidForConsumers = 0;

	// Buffer with available SCTP stream ids.
	#sctpStreamIds?: Buffer;

	// Next SCTP stream id.
	#nextSctpStreamId = 0;

	// Observer instance.
	readonly #observer: Observer;

	/**
	 * @private
	 * @interface
	 */
	constructor(
		{
			internal,
			data,
			channel,
			appData,
			getRouterRtpCapabilities,
			getProducerById,
			getDataProducerById,
		}: TransportConstructorOptions<TransportAppData>,
		observer: Observer
	) {
		super();

		logger.debug('constructor()');

		this.internal = internal;
		this.#data = data;
		this.channel = channel;
		this.#appData = appData ?? ({} as TransportAppData);
		this.#getRouterRtpCapabilities = getRouterRtpCapabilities;
		this.getProducerById = getProducerById;
		this.getDataProducerById = getDataProducerById;
		this.#observer = observer;
	}

	/**
	 * Transport id.
	 */
	get id(): string {
		return this.internal.transportId;
	}

	/**
	 * Whether the Transport is closed.
	 */
	get closed(): boolean {
		return this.#closed;
	}

	/**
	 * App custom data.
	 */
	get appData(): TransportAppData {
		return this.#appData;
	}

	/**
	 * App custom data setter.
	 */
	set appData(appData: TransportAppData) {
		this.#appData = appData;
	}

	/**
	 * Observer.
	 */
	get observer(): Observer {
		return this.#observer;
	}

	/**
	 * @private
	 * Just for testing purposes.
	 */
	get channelForTesting(): Channel {
		return this.channel;
	}

	/**
	 * Close the Transport.
	 */
	close(): void {
		if (this.#closed) {
			return;
		}

		logger.debug('close()');

		this.#closed = true;

		// Remove notification subscriptions.
		this.channel.removeAllListeners(this.internal.transportId);

		/* Build Request. */
		const requestOffset = new FbsRouter.CloseTransportRequestT(
			this.internal.transportId
		).pack(this.channel.bufferBuilder);

		this.channel
			.request(
				FbsRequest.Method.ROUTER_CLOSE_TRANSPORT,
				FbsRequest.Body.Router_CloseTransportRequest,
				requestOffset,
				this.internal.routerId
			)
			.catch(() => {});

		// Close every Producer.
		for (const producer of this.#producers.values()) {
			producer.transportClosed();

			// Must tell the Router.
			this.emit('@producerclose', producer);
		}
		this.#producers.clear();

		// Close every Consumer.
		for (const consumer of this.consumers.values()) {
			consumer.transportClosed();
		}
		this.consumers.clear();

		// Close every DataProducer.
		for (const dataProducer of this.dataProducers.values()) {
			dataProducer.transportClosed();

			// Must tell the Router.
			this.emit('@dataproducerclose', dataProducer);
		}
		this.dataProducers.clear();

		// Close every DataConsumer.
		for (const dataConsumer of this.dataConsumers.values()) {
			dataConsumer.transportClosed();
		}
		this.dataConsumers.clear();

		this.emit('@close');

		// Emit observer event.
		this.#observer.safeEmit('close');
	}

	/**
	 * Router was closed.
	 *
	 * @private
	 * @virtual
	 */
	routerClosed(): void {
		if (this.#closed) {
			return;
		}

		logger.debug('routerClosed()');

		this.#closed = true;

		// Remove notification subscriptions.
		this.channel.removeAllListeners(this.internal.transportId);

		// Close every Producer.
		for (const producer of this.#producers.values()) {
			producer.transportClosed();

			// NOTE: No need to tell the Router since it already knows (it has
			// been closed in fact).
		}
		this.#producers.clear();

		// Close every Consumer.
		for (const consumer of this.consumers.values()) {
			consumer.transportClosed();
		}
		this.consumers.clear();

		// Close every DataProducer.
		for (const dataProducer of this.dataProducers.values()) {
			dataProducer.transportClosed();

			// NOTE: No need to tell the Router since it already knows (it has
			// been closed in fact).
		}
		this.dataProducers.clear();

		// Close every DataConsumer.
		for (const dataConsumer of this.dataConsumers.values()) {
			dataConsumer.transportClosed();
		}
		this.dataConsumers.clear();

		this.safeEmit('routerclose');

		// Emit observer event.
		this.#observer.safeEmit('close');
	}

	/**
	 * Listen server was closed (this just happens in WebRtcTransports when their
	 * associated WebRtcServer is closed).
	 *
	 * @private
	 */
	listenServerClosed(): void {
		if (this.#closed) {
			return;
		}

		logger.debug('listenServerClosed()');

		this.#closed = true;

		// Remove notification subscriptions.
		this.channel.removeAllListeners(this.internal.transportId);

		// Close every Producer.
		for (const producer of this.#producers.values()) {
			producer.transportClosed();

			// NOTE: No need to tell the Router since it already knows (it has
			// been closed in fact).
		}
		this.#producers.clear();

		// Close every Consumer.
		for (const consumer of this.consumers.values()) {
			consumer.transportClosed();
		}
		this.consumers.clear();

		// Close every DataProducer.
		for (const dataProducer of this.dataProducers.values()) {
			dataProducer.transportClosed();

			// NOTE: No need to tell the Router since it already knows (it has
			// been closed in fact).
		}
		this.dataProducers.clear();

		// Close every DataConsumer.
		for (const dataConsumer of this.dataConsumers.values()) {
			dataConsumer.transportClosed();
		}
		this.dataConsumers.clear();

		// Need to emit this event to let the parent Router know since
		// transport.listenServerClosed() is called by the listen server.
		// NOTE: Currently there is just WebRtcServer for WebRtcTransports.
		this.emit('@listenserverclose');

		this.safeEmit('listenserverclose');

		// Emit observer event.
		this.#observer.safeEmit('close');
	}

	/**
	 * Dump Transport.
	 *
	 * @abstract
	 */
	// eslint-disable-next-line @typescript-eslint/require-await
	async dump(): Promise<any> {
		// Should not happen.
		throw new Error('method implemented in the subclass');
	}

	/**
	 * Get Transport stats.
	 *
	 * @abstract
	 */
	// eslint-disable-next-line @typescript-eslint/require-await
	async getStats(): Promise<any[]> {
		// Should not happen.
		throw new Error('method implemented in the subclass');
	}

	/**
	 * Provide the Transport remote parameters.
	 *
	 * @abstract
	 */
	// eslint-disable-next-line @typescript-eslint/no-unused-vars, @typescript-eslint/require-await
	async connect(params: any): Promise<void> {
		// Should not happen.
		throw new Error('method implemented in the subclass');
	}

	/**
	 * Set maximum incoming bitrate for receiving media.
	 */
	async setMaxIncomingBitrate(bitrate: number): Promise<void> {
		logger.debug(`setMaxIncomingBitrate() [bitrate:${bitrate}]`);

		/* Build Request. */
		const requestOffset =
			FbsTransport.SetMaxIncomingBitrateRequest.createSetMaxIncomingBitrateRequest(
				this.channel.bufferBuilder,
				bitrate
			);

		await this.channel.request(
			FbsRequest.Method.TRANSPORT_SET_MAX_INCOMING_BITRATE,
			FbsRequest.Body.Transport_SetMaxIncomingBitrateRequest,
			requestOffset,
			this.internal.transportId
		);
	}

	/**
	 * Set maximum outgoing bitrate for sending media.
	 */
	async setMaxOutgoingBitrate(bitrate: number): Promise<void> {
		logger.debug(`setMaxOutgoingBitrate() [bitrate:${bitrate}]`);

		/* Build Request. */
		const requestOffset = new FbsTransport.SetMaxOutgoingBitrateRequestT(
			bitrate
		).pack(this.channel.bufferBuilder);

		await this.channel.request(
			FbsRequest.Method.TRANSPORT_SET_MAX_OUTGOING_BITRATE,
			FbsRequest.Body.Transport_SetMaxOutgoingBitrateRequest,
			requestOffset,
			this.internal.transportId
		);
	}

	/**
	 * Set minimum outgoing bitrate for sending media.
	 */
	async setMinOutgoingBitrate(bitrate: number): Promise<void> {
		logger.debug(`setMinOutgoingBitrate() [bitrate:${bitrate}]`);

		/* Build Request. */
		const requestOffset = new FbsTransport.SetMinOutgoingBitrateRequestT(
			bitrate
		).pack(this.channel.bufferBuilder);

		await this.channel.request(
			FbsRequest.Method.TRANSPORT_SET_MIN_OUTGOING_BITRATE,
			FbsRequest.Body.Transport_SetMinOutgoingBitrateRequest,
			requestOffset,
			this.internal.transportId
		);
	}

	/**
	 * Create a Producer.
	 */
	async produce<ProducerAppData extends AppData = AppData>({
		id = undefined,
		kind,
		rtpParameters,
		paused = false,
		keyFrameRequestDelay,
		appData,
	}: ProducerOptions<ProducerAppData>): Promise<Producer<ProducerAppData>> {
		logger.debug('produce()');

		if (id && this.#producers.has(id)) {
			throw new TypeError(`a Producer with same id "${id}" already exists`);
		} else if (!['audio', 'video'].includes(kind)) {
			throw new TypeError(`invalid kind "${kind}"`);
		} else if (appData && typeof appData !== 'object') {
			throw new TypeError('if given, appData must be an object');
		}

		// Clone given RTP parameters to not modify input data.
		const clonedRtpParameters = utils.clone<RtpParameters>(rtpParameters);

		// This may throw.
		ortc.validateRtpParameters(clonedRtpParameters);

		// If missing or empty encodings, add one.
		if (
			!clonedRtpParameters.encodings ||
			!Array.isArray(clonedRtpParameters.encodings) ||
			clonedRtpParameters.encodings.length === 0
		) {
			clonedRtpParameters.encodings = [{}];
		}

		// Don't do this in PipeTransports since there we must keep CNAME value in
		// each Producer.
		if (this.constructor.name !== 'PipeTransport') {
			// If CNAME is given and we don't have yet a CNAME for Producers in this
			// Transport, take it.
			if (!this.#cnameForProducers && clonedRtpParameters.rtcp?.cname) {
				this.#cnameForProducers = clonedRtpParameters.rtcp.cname;
			}
			// Otherwise if we don't have yet a CNAME for Producers and the RTP
			// parameters do not include CNAME, create a random one.
			else if (!this.#cnameForProducers) {
				this.#cnameForProducers = utils.generateUUIDv4().substr(0, 8);
			}

			// Override Producer's CNAME.
			clonedRtpParameters.rtcp = clonedRtpParameters.rtcp ?? {};
			clonedRtpParameters.rtcp.cname = this.#cnameForProducers;
		}

		const routerRtpCapabilities = this.#getRouterRtpCapabilities();

		// This may throw.
		const rtpMapping = ortc.getProducerRtpParametersMapping(
			clonedRtpParameters,
			routerRtpCapabilities
		);

		// This may throw.
		const consumableRtpParameters = ortc.getConsumableRtpParameters(
			kind,
			clonedRtpParameters,
			routerRtpCapabilities,
			rtpMapping
		);

		const producerId = id ?? utils.generateUUIDv4();
		const requestOffset = createProduceRequest({
			builder: this.channel.bufferBuilder,
			producerId,
			kind,
			rtpParameters: clonedRtpParameters,
			rtpMapping,
			keyFrameRequestDelay,
			paused,
		});

		const response = await this.channel.request(
			FbsRequest.Method.TRANSPORT_PRODUCE,
			FbsRequest.Body.Transport_ProduceRequest,
			requestOffset,
			this.internal.transportId
		);

		/* Decode Response. */
		const produceResponse = new FbsTransport.ProduceResponse();

		response.body(produceResponse);

		const status = produceResponse.unpack();

		const data = {
			kind,
			rtpParameters: clonedRtpParameters,
			type: producerTypeFromFbs(status.type),
			consumableRtpParameters,
		};

		const producer: Producer<ProducerAppData> = new Producer({
			internal: {
				...this.internal,
				producerId,
			},
			data,
			channel: this.channel,
			appData,
			paused,
		});

		this.#producers.set(producer.id, producer);
		producer.on('@close', () => {
			this.#producers.delete(producer.id);
			this.emit('@producerclose', producer);
		});

		this.emit('@newproducer', producer);

		// Emit observer event.
		this.#observer.safeEmit('newproducer', producer);

		return producer;
	}

	/**
	 * Create a Consumer.
	 *
	 * @virtual
	 */
	async consume<ConsumerAppData extends AppData = AppData>({
		producerId,
		rtpCapabilities,
		paused = false,
		mid,
		preferredLayers,
		ignoreDtx = false,
		enableRtx,
		pipe = false,
		appData,
	}: ConsumerOptions<ConsumerAppData>): Promise<Consumer<ConsumerAppData>> {
		logger.debug('consume()');

		if (!producerId || typeof producerId !== 'string') {
			throw new TypeError('missing producerId');
		} else if (appData && typeof appData !== 'object') {
			throw new TypeError('if given, appData must be an object');
		} else if (mid && (typeof mid !== 'string' || mid.length === 0)) {
			throw new TypeError('if given, mid must be non empty string');
		}

		// Clone given RTP capabilities to not modify input data.
		const clonedRtpCapabilities = utils.clone<RtpCapabilities>(rtpCapabilities);

		// This may throw.
		ortc.validateRtpCapabilities(clonedRtpCapabilities);

		const producer = this.getProducerById(producerId);

		if (!producer) {
			throw Error(`Producer with id "${producerId}" not found`);
		}

		// If enableRtx is not given, set it to true if video and false if audio.
		if (enableRtx === undefined) {
			enableRtx = producer.kind === 'video';
		}

		// This may throw.
		const rtpParameters = ortc.getConsumerRtpParameters({
			consumableRtpParameters: producer.consumableRtpParameters,
			remoteRtpCapabilities: clonedRtpCapabilities,
			pipe,
			enableRtx,
		});

		// Set MID.
		if (!pipe) {
			if (mid) {
				rtpParameters.mid = mid;
			} else {
				rtpParameters.mid = `${this.#nextMidForConsumers++}`;

				// We use up to 8 bytes for MID (string).
				if (this.#nextMidForConsumers === 100000000) {
					logger.error(
						`consume() | reaching max MID value "${this.#nextMidForConsumers}"`
					);

					this.#nextMidForConsumers = 0;
				}
			}
		}

		const consumerId = utils.generateUUIDv4();
		const requestOffset = createConsumeRequest({
			builder: this.channel.bufferBuilder,
			producer,
			consumerId,
			rtpParameters,
			paused,
			preferredLayers,
			ignoreDtx,
			pipe,
		});

		const response = await this.channel.request(
			FbsRequest.Method.TRANSPORT_CONSUME,
			FbsRequest.Body.Transport_ConsumeRequest,
			requestOffset,
			this.internal.transportId
		);

		/* Decode Response. */
		const consumeResponse = new FbsTransport.ConsumeResponse();

		response.body(consumeResponse);

		const status = consumeResponse.unpack();

		const data = {
			producerId,
			kind: producer.kind,
			rtpParameters,
			type: pipe ? 'pipe' : (producer.type as ConsumerType),
		};

		const consumer: Consumer<ConsumerAppData> = new Consumer({
			internal: {
				...this.internal,
				consumerId,
			},
			data,
			channel: this.channel,
			appData,
			paused: status.paused,
			producerPaused: status.producerPaused,
			score: status.score ?? undefined,
			preferredLayers: status.preferredLayers
				? {
						spatialLayer: status.preferredLayers.spatialLayer,
						temporalLayer: status.preferredLayers.temporalLayer ?? undefined,
					}
				: undefined,
		});

		this.consumers.set(consumer.id, consumer);
		consumer.on('@close', () => this.consumers.delete(consumer.id));
		consumer.on('@producerclose', () => this.consumers.delete(consumer.id));

		// Emit observer event.
		this.#observer.safeEmit('newconsumer', consumer);

		return consumer;
	}

	/**
	 * Create a DataProducer.
	 */
	async produceData<DataProducerAppData extends AppData = AppData>({
		id = undefined,
		sctpStreamParameters,
		label = '',
		protocol = '',
		paused = false,
		appData,
	}: DataProducerOptions<DataProducerAppData> = {}): Promise<
		DataProducer<DataProducerAppData>
	> {
		logger.debug('produceData()');

		if (id && this.dataProducers.has(id)) {
			throw new TypeError(`a DataProducer with same id "${id}" already exists`);
		} else if (appData && typeof appData !== 'object') {
			throw new TypeError('if given, appData must be an object');
		}

		let type: DataProducerType;

		// Clone given SCTP stream parameters to not modify input data.
		let clonedSctpStreamParameters = utils.clone<
			SctpStreamParameters | undefined
		>(sctpStreamParameters);

		// If this is not a DirectTransport, sctpStreamParameters are required.
		if (this.constructor.name !== 'DirectTransport') {
			type = 'sctp';

			// This may throw.
			ortc.validateSctpStreamParameters(clonedSctpStreamParameters!);
		}
		// If this is a DirectTransport, sctpStreamParameters must not be given.
		else {
			type = 'direct';

			if (sctpStreamParameters) {
				logger.warn(
					'produceData() | sctpStreamParameters are ignored when producing data on a DirectTransport'
				);

				clonedSctpStreamParameters = undefined;
			}
		}

		const dataProducerId = id ?? utils.generateUUIDv4();
		const requestOffset = createProduceDataRequest({
			builder: this.channel.bufferBuilder,
			dataProducerId,
			type,
			sctpStreamParameters: clonedSctpStreamParameters,
			label,
			protocol,
			paused,
		});

		const response = await this.channel.request(
			FbsRequest.Method.TRANSPORT_PRODUCE_DATA,
			FbsRequest.Body.Transport_ProduceDataRequest,
			requestOffset,
			this.internal.transportId
		);

		/* Decode Response. */
		const produceDataResponse = new FbsDataProducer.DumpResponse();

		response.body(produceDataResponse);

		const dump = parseDataProducerDumpResponse(produceDataResponse);

		const dataProducer: DataProducer<DataProducerAppData> = new DataProducer({
			internal: {
				...this.internal,
				dataProducerId,
			},
			data: {
				type: dump.type,
				sctpStreamParameters: dump.sctpStreamParameters,
				label: dump.label,
				protocol: dump.protocol,
			},
			channel: this.channel,
			paused,
			appData,
		});

		this.dataProducers.set(dataProducer.id, dataProducer);
		dataProducer.on('@close', () => {
			this.dataProducers.delete(dataProducer.id);
			this.emit('@dataproducerclose', dataProducer);
		});

		this.emit('@newdataproducer', dataProducer);

		// Emit observer event.
		this.#observer.safeEmit('newdataproducer', dataProducer);

		return dataProducer;
	}

	/**
	 * Create a DataConsumer.
	 */
	async consumeData<DataConsumerAppData extends AppData = AppData>({
		dataProducerId,
		ordered,
		maxPacketLifeTime,
		maxRetransmits,
		sctpStreamId,
		paused = false,
		subchannels,
		appData,
	}: DataConsumerOptions<DataConsumerAppData>): Promise<
		DataConsumer<DataConsumerAppData>
	> {
		logger.debug('consumeData()');

		if (!dataProducerId || typeof dataProducerId !== 'string') {
			throw new TypeError('missing dataProducerId');
		} else if (appData && typeof appData !== 'object') {
			throw new TypeError('if given, appData must be an object');
		}

		const dataProducer = this.getDataProducerById(dataProducerId);

		if (!dataProducer) {
			throw Error(`DataProducer with id "${dataProducerId}" not found`);
		}

		let type: DataConsumerType;
		let sctpStreamParameters: SctpStreamParameters | undefined;

		// If this is not a DirectTransport, use sctpStreamParameters from the
		// DataProducer (if type 'sctp') unless they are given in method parameters.
		if (this.constructor.name !== 'DirectTransport') {
			type = 'sctp';

			sctpStreamParameters =
				utils.clone<SctpStreamParameters | undefined>(
					dataProducer.sctpStreamParameters
				) ?? ({} as SctpStreamParameters);

			// Override if given.
			if (ordered !== undefined) {
				sctpStreamParameters.ordered = ordered;
			}

			if (maxPacketLifeTime !== undefined) {
				sctpStreamParameters.maxPacketLifeTime = maxPacketLifeTime;
			}

			if (maxRetransmits !== undefined) {
				sctpStreamParameters.maxRetransmits = maxRetransmits;
			}

			// This may throw.
			sctpStreamParameters.streamId = this.getNextSctpStreamId(sctpStreamId);
			ortc.validateSctpStreamParameters(sctpStreamParameters);
			this.#sctpStreamIds![sctpStreamParameters.streamId] = 1;
		}
		// If this is a DirectTransport, sctpStreamParameters must not be used.
		else {
			type = 'direct';

			if (
				ordered !== undefined ||
				maxPacketLifeTime !== undefined ||
				maxRetransmits !== undefined ||
				sctpStreamId !== undefined
			) {
				logger.warn(
					'consumeData() | ordered, maxPacketLifeTime, maxRetransmits and sctpStreamId are ignored when consuming data on a DirectTransport'
				);
			}
		}

		const { label, protocol } = dataProducer;
		const dataConsumerId = utils.generateUUIDv4();

		const requestOffset = createConsumeDataRequest({
			builder: this.channel.bufferBuilder,
			dataConsumerId,
			dataProducerId,
			type,
			sctpStreamParameters,
			label,
			protocol,
			paused,
			subchannels,
		});

		const response = await this.channel.request(
			FbsRequest.Method.TRANSPORT_CONSUME_DATA,
			FbsRequest.Body.Transport_ConsumeDataRequest,
			requestOffset,
			this.internal.transportId
		);

		/* Decode Response. */
		const consumeDataResponse = new FbsDataConsumer.DumpResponse();

		response.body(consumeDataResponse);

		const dump = parseDataConsumerDumpResponse(consumeDataResponse);

		const dataConsumer: DataConsumer<DataConsumerAppData> = new DataConsumer({
			internal: {
				...this.internal,
				dataConsumerId,
			},
			data: {
				dataProducerId: dump.dataProducerId,
				type: dump.type,
				sctpStreamParameters: dump.sctpStreamParameters,
				label: dump.label,
				protocol: dump.protocol,
				bufferedAmountLowThreshold: dump.bufferedAmountLowThreshold,
			},
			channel: this.channel,
			paused: dump.paused,
			subchannels: dump.subchannels,
			dataProducerPaused: dump.dataProducerPaused,
			appData,
		});

		this.dataConsumers.set(dataConsumer.id, dataConsumer);
		dataConsumer.on('@close', () => {
			this.dataConsumers.delete(dataConsumer.id);

			if (this.#sctpStreamIds) {
				this.#sctpStreamIds[sctpStreamParameters!.streamId] = 0;
			}
		});
		dataConsumer.on('@dataproducerclose', () => {
			this.dataConsumers.delete(dataConsumer.id);

			if (this.#sctpStreamIds) {
				this.#sctpStreamIds[sctpStreamParameters!.streamId] = 0;
			}
		});

		// Emit observer event.
		this.#observer.safeEmit('newdataconsumer', dataConsumer);

		return dataConsumer;
	}

	/**
	 * Enable 'trace' event.
	 */
	async enableTraceEvent(types: TransportTraceEventType[] = []): Promise<void> {
		logger.debug('enableTraceEvent()');

		if (!Array.isArray(types)) {
			throw new TypeError('types must be an array');
		} else if (types.find(type => typeof type !== 'string')) {
			throw new TypeError('every type must be a string');
		}

		// Convert event types.
		const fbsEventTypes: FbsTransport.TraceEventType[] = [];

		for (const eventType of types) {
			try {
				fbsEventTypes.push(transportTraceEventTypeToFbs(eventType));
			} catch (error) {
				// Ignore invalid event types.
			}
		}

		/* Build Request. */
		const requestOffset = new FbsTransport.EnableTraceEventRequestT(
			fbsEventTypes
		).pack(this.channel.bufferBuilder);

		await this.channel.request(
			FbsRequest.Method.TRANSPORT_ENABLE_TRACE_EVENT,
			FbsRequest.Body.Transport_EnableTraceEventRequest,
			requestOffset,
			this.internal.transportId
		);
	}

	private getNextSctpStreamId(sctpStreamId?: number): number {
		if (
			!this.#data.sctpParameters ||
			typeof this.#data.sctpParameters.MIS !== 'number'
		) {
			throw new TypeError('missing sctpParameters.MIS');
		}

		const numStreams = this.#data.sctpParameters.MIS;

		if (!this.#sctpStreamIds) {
			this.#sctpStreamIds = Buffer.alloc(numStreams, 0);
		}

		if (sctpStreamId === undefined) {
			for (let idx = 0; idx < this.#sctpStreamIds.length; ++idx) {
				sctpStreamId =
					(this.#nextSctpStreamId + idx) % this.#sctpStreamIds.length;

				if (!this.#sctpStreamIds[sctpStreamId]) {
					this.#nextSctpStreamId = sctpStreamId + 1;

					return sctpStreamId;
				}
			}

			throw new Error('no sctpStreamId available');
		} else if (sctpStreamId >= this.#sctpStreamIds.length) {
			throw new Error('invalid sctpStreamId');
		} else if (this.#sctpStreamIds[sctpStreamId]) {
			throw new Error('sctpStreamId already assigned');
		}

		return sctpStreamId;
	}
}

export function portRangeToFbs(
	portRange: TransportPortRange = { min: 0, max: 0 }
): FbsTransport.PortRangeT {
	return new FbsTransport.PortRangeT(portRange.min, portRange.max);
}

export function socketFlagsToFbs(
	flags: TransportSocketFlags = {}
): FbsTransport.SocketFlagsT {
	return new FbsTransport.SocketFlagsT(
		Boolean(flags.ipv6Only),
		Boolean(flags.udpReusePort)
	);
}

export function parseSctpState(fbsSctpState: FbsSctpState): SctpState {
	switch (fbsSctpState) {
		case FbsSctpState.NEW: {
			return 'new';
		}

		case FbsSctpState.CONNECTING: {
			return 'connecting';
		}

		case FbsSctpState.CONNECTED: {
			return 'connected';
		}

		case FbsSctpState.FAILED: {
			return 'failed';
		}

		case FbsSctpState.CLOSED: {
			return 'closed';
		}
	}
}

export function parseProtocol(
	protocol: FbsTransport.Protocol
): TransportProtocol {
	switch (protocol) {
		case FbsTransport.Protocol.UDP: {
			return 'udp';
		}

		case FbsTransport.Protocol.TCP: {
			return 'tcp';
		}
	}
}

export function serializeProtocol(
	protocol: TransportProtocol
): FbsTransport.Protocol {
	switch (protocol) {
		case 'udp': {
			return FbsTransport.Protocol.UDP;
		}

		case 'tcp': {
			return FbsTransport.Protocol.TCP;
		}
	}
}

export function parseTuple(binary: FbsTransport.Tuple): TransportTuple {
	return {
		// @deprecated Use localAddress instead.
		localIp: binary.localAddress()!,
		localAddress: binary.localAddress()!,
		localPort: binary.localPort(),
		remoteIp: binary.remoteIp() ?? undefined,
		remotePort: binary.remotePort(),
		protocol: parseProtocol(binary.protocol()),
	};
}

export function parseBaseTransportDump(
	binary: FbsTransport.Dump
): BaseTransportDump {
	// Retrieve producerIds.
	const producerIds = utils.parseVector<string>(binary, 'producerIds');
	// Retrieve consumerIds.
	const consumerIds = utils.parseVector<string>(binary, 'consumerIds');
	// Retrieve map SSRC consumerId.
	const mapSsrcConsumerId = utils.parseUint32StringVector(
		binary,
		'mapSsrcConsumerId'
	);
	// Retrieve map RTX SSRC consumerId.
	const mapRtxSsrcConsumerId = utils.parseUint32StringVector(
		binary,
		'mapRtxSsrcConsumerId'
	);
	// Retrieve dataProducerIds.
	const dataProducerIds = utils.parseVector<string>(binary, 'dataProducerIds');
	// Retrieve dataConsumerIds.
	const dataConsumerIds = utils.parseVector<string>(binary, 'dataConsumerIds');
	// Retrieve recvRtpHeaderExtesions.
	const recvRtpHeaderExtensions = parseRecvRtpHeaderExtensions(
		binary.recvRtpHeaderExtensions()!
	);
	// Retrieve RtpListener.
	const rtpListener = parseRtpListenerDump(binary.rtpListener()!);

	// Retrieve SctpParameters.
	const fbsSctpParameters = binary.sctpParameters();
	let sctpParameters: SctpParameters | undefined;

	if (fbsSctpParameters) {
		sctpParameters = parseSctpParametersDump(fbsSctpParameters);
	}

	// Retrieve sctpState.
	const sctpState =
		binary.sctpState() === null
			? undefined
			: parseSctpState(binary.sctpState()!);

	// Retrive sctpListener.
	const sctpListener = binary.sctpListener()
		? parseSctpListenerDump(binary.sctpListener()!)
		: undefined;

	// Retrieve traceEventTypes.
	const traceEventTypes = utils.parseVector<TransportTraceEventType>(
		binary,
		'traceEventTypes',
		transportTraceEventTypeFromFbs
	);

	return {
		id: binary.id()!,
		direct: binary.direct(),
		producerIds: producerIds,
		consumerIds: consumerIds,
		mapSsrcConsumerId: mapSsrcConsumerId,
		mapRtxSsrcConsumerId: mapRtxSsrcConsumerId,
		dataProducerIds: dataProducerIds,
		dataConsumerIds: dataConsumerIds,
		recvRtpHeaderExtensions: recvRtpHeaderExtensions,
		rtpListener: rtpListener,
		maxMessageSize: binary.maxMessageSize(),
		sctpParameters: sctpParameters,
		sctpState: sctpState,
		sctpListener: sctpListener,
		traceEventTypes: traceEventTypes,
	};
}

export function parseBaseTransportStats(
	binary: FbsTransport.Stats
): BaseTransportStats {
	const sctpState =
		binary.sctpState() === null
			? undefined
			: parseSctpState(binary.sctpState()!);

	return {
		transportId: binary.transportId()!,
		timestamp: Number(binary.timestamp()),
		sctpState,
		bytesReceived: Number(binary.bytesReceived()),
		recvBitrate: Number(binary.recvBitrate()),
		bytesSent: Number(binary.bytesSent()),
		sendBitrate: Number(binary.sendBitrate()),
		rtpBytesReceived: Number(binary.rtpBytesReceived()),
		rtpRecvBitrate: Number(binary.rtpRecvBitrate()),
		rtpBytesSent: Number(binary.rtpBytesSent()),
		rtpSendBitrate: Number(binary.rtpSendBitrate()),
		rtxBytesReceived: Number(binary.rtxBytesReceived()),
		rtxRecvBitrate: Number(binary.rtxRecvBitrate()),
		rtxBytesSent: Number(binary.rtxBytesSent()),
		rtxSendBitrate: Number(binary.rtxSendBitrate()),
		probationBytesSent: Number(binary.probationBytesSent()),
		probationSendBitrate: Number(binary.probationSendBitrate()),
		availableOutgoingBitrate:
			typeof binary.availableOutgoingBitrate() === 'number'
				? Number(binary.availableOutgoingBitrate())
				: undefined,
		availableIncomingBitrate:
			typeof binary.availableIncomingBitrate() === 'number'
				? Number(binary.availableIncomingBitrate())
				: undefined,
		maxIncomingBitrate:
			typeof binary.maxIncomingBitrate() === 'number'
				? Number(binary.maxIncomingBitrate())
				: undefined,
		maxOutgoingBitrate:
			typeof binary.maxOutgoingBitrate() === 'number'
				? Number(binary.maxOutgoingBitrate())
				: undefined,
		minOutgoingBitrate:
			typeof binary.minOutgoingBitrate() === 'number'
				? Number(binary.minOutgoingBitrate())
				: undefined,
		rtpPacketLossReceived:
			typeof binary.rtpPacketLossReceived() === 'number'
				? Number(binary.rtpPacketLossReceived())
				: undefined,
		rtpPacketLossSent:
			typeof binary.rtpPacketLossSent() === 'number'
				? Number(binary.rtpPacketLossSent())
				: undefined,
	};
}

export function parseTransportTraceEventData(
	trace: FbsTransport.TraceNotification
): TransportTraceEventData {
	switch (trace.type()) {
		case FbsTransport.TraceEventType.BWE: {
			const info = new FbsTransport.BweTraceInfo();

			trace.info(info);

			return {
				type: 'bwe',
				timestamp: Number(trace.timestamp()),
				direction:
					trace.direction() === FbsTraceDirection.DIRECTION_IN ? 'in' : 'out',
				info: parseBweTraceInfo(info),
			};
		}

		case FbsTransport.TraceEventType.PROBATION: {
			return {
				type: 'probation',
				timestamp: Number(trace.timestamp()),
				direction:
					trace.direction() === FbsTraceDirection.DIRECTION_IN ? 'in' : 'out',
				info: {},
			};
		}
	}
}

function parseRecvRtpHeaderExtensions(
	binary: FbsTransport.RecvRtpHeaderExtensions
): RecvRtpHeaderExtensions {
	return {
		mid: binary.mid() !== null ? binary.mid()! : undefined,
		rid: binary.rid() !== null ? binary.rid()! : undefined,
		rrid: binary.rrid() !== null ? binary.rrid()! : undefined,
		absSendTime:
			binary.absSendTime() !== null ? binary.absSendTime()! : undefined,
		transportWideCc01:
			binary.transportWideCc01() !== null
				? binary.transportWideCc01()!
				: undefined,
	};
}

function transportTraceEventTypeToFbs(
	eventType: TransportTraceEventType
): FbsTransport.TraceEventType {
	switch (eventType) {
		case 'probation': {
			return FbsTransport.TraceEventType.PROBATION;
		}

		case 'bwe': {
			return FbsTransport.TraceEventType.BWE;
		}
	}
}

function transportTraceEventTypeFromFbs(
	eventType: FbsTransport.TraceEventType
): TransportTraceEventType {
	switch (eventType) {
		case FbsTransport.TraceEventType.PROBATION: {
			return 'probation';
		}

		case FbsTransport.TraceEventType.BWE: {
			return 'bwe';
		}
	}
}

function parseBweTraceInfo(binary: FbsTransport.BweTraceInfo): {
	desiredBitrate: number;
	effectiveDesiredBitrate: number;
	minBitrate: number;
	maxBitrate: number;
	startBitrate: number;
	maxPaddingBitrate: number;
	availableBitrate: number;
	bweType: 'transport-cc' | 'remb';
} {
	return {
		desiredBitrate: binary.desiredBitrate(),
		effectiveDesiredBitrate: binary.effectiveDesiredBitrate(),
		minBitrate: binary.minBitrate(),
		maxBitrate: binary.maxBitrate(),
		startBitrate: binary.startBitrate(),
		maxPaddingBitrate: binary.maxPaddingBitrate(),
		availableBitrate: binary.availableBitrate(),
		bweType:
			binary.bweType() === FbsTransport.BweType.TRANSPORT_CC
				? 'transport-cc'
				: 'remb',
	};
}

function createConsumeRequest({
	builder,
	producer,
	consumerId,
	rtpParameters,
	paused,
	preferredLayers,
	ignoreDtx,
	pipe,
}: {
	builder: flatbuffers.Builder;
	producer: Producer;
	consumerId: string;
	rtpParameters: RtpParameters;
	paused: boolean;
	preferredLayers?: ConsumerLayers;
	ignoreDtx?: boolean;
	pipe: boolean;
}): number {
	const rtpParametersOffset = serializeRtpParameters(builder, rtpParameters);
	const consumerIdOffset = builder.createString(consumerId);
	const producerIdOffset = builder.createString(producer.id);
	let consumableRtpEncodingsOffset: number | undefined;
	let preferredLayersOffset: number | undefined;

	if (producer.consumableRtpParameters.encodings) {
		consumableRtpEncodingsOffset = serializeRtpEncodingParameters(
			builder,
			producer.consumableRtpParameters.encodings
		);
	}

	if (preferredLayers) {
		FbsConsumer.ConsumerLayers.startConsumerLayers(builder);
		FbsConsumer.ConsumerLayers.addSpatialLayer(
			builder,
			preferredLayers.spatialLayer
		);

		if (preferredLayers.temporalLayer !== undefined) {
			FbsConsumer.ConsumerLayers.addTemporalLayer(
				builder,
				preferredLayers.temporalLayer
			);
		}

		preferredLayersOffset =
			FbsConsumer.ConsumerLayers.endConsumerLayers(builder);
	}

	const ConsumeRequest = FbsTransport.ConsumeRequest;

	// Create Consume Request.
	ConsumeRequest.startConsumeRequest(builder);
	ConsumeRequest.addConsumerId(builder, consumerIdOffset);
	ConsumeRequest.addProducerId(builder, producerIdOffset);
	ConsumeRequest.addKind(
		builder,
		producer.kind === 'audio' ? FbsMediaKind.AUDIO : FbsMediaKind.VIDEO
	);
	ConsumeRequest.addRtpParameters(builder, rtpParametersOffset);
	ConsumeRequest.addType(
		builder,
		pipe ? FbsRtpParameters.Type.PIPE : producerTypeToFbs(producer.type)
	);

	if (consumableRtpEncodingsOffset) {
		ConsumeRequest.addConsumableRtpEncodings(
			builder,
			consumableRtpEncodingsOffset
		);
	}

	ConsumeRequest.addPaused(builder, paused);

	if (preferredLayersOffset) {
		ConsumeRequest.addPreferredLayers(builder, preferredLayersOffset);
	}

	ConsumeRequest.addIgnoreDtx(builder, Boolean(ignoreDtx));

	return ConsumeRequest.endConsumeRequest(builder);
}

function createProduceRequest({
	builder,
	producerId,
	kind,
	rtpParameters,
	rtpMapping,
	keyFrameRequestDelay,
	paused,
}: {
	builder: flatbuffers.Builder;
	producerId: string;
	kind: MediaKind;
	rtpParameters: RtpParameters;
	rtpMapping: ortc.RtpMapping;
	keyFrameRequestDelay?: number;
	paused: boolean;
}): number {
	const producerIdOffset = builder.createString(producerId);
	const rtpParametersOffset = serializeRtpParameters(builder, rtpParameters);
	const rtpMappingOffset = ortc.serializeRtpMapping(builder, rtpMapping);

	FbsTransport.ProduceRequest.startProduceRequest(builder);
	FbsTransport.ProduceRequest.addProducerId(builder, producerIdOffset);
	FbsTransport.ProduceRequest.addKind(
		builder,
		kind === 'audio' ? FbsMediaKind.AUDIO : FbsMediaKind.VIDEO
	);
	FbsTransport.ProduceRequest.addRtpParameters(builder, rtpParametersOffset);
	FbsTransport.ProduceRequest.addRtpMapping(builder, rtpMappingOffset);
	FbsTransport.ProduceRequest.addKeyFrameRequestDelay(
		builder,
		keyFrameRequestDelay ?? 0
	);
	FbsTransport.ProduceRequest.addPaused(builder, paused);

	return FbsTransport.ProduceRequest.endProduceRequest(builder);
}

function createProduceDataRequest({
	builder,
	dataProducerId,
	type,
	sctpStreamParameters,
	label,
	protocol,
	paused,
}: {
	builder: flatbuffers.Builder;
	dataProducerId: string;
	type: DataProducerType;
	sctpStreamParameters?: SctpStreamParameters;
	label: string;
	protocol: string;
	paused: boolean;
}): number {
	const dataProducerIdOffset = builder.createString(dataProducerId);
	const labelOffset = builder.createString(label);
	const protocolOffset = builder.createString(protocol);

	let sctpStreamParametersOffset = 0;

	if (sctpStreamParameters) {
		sctpStreamParametersOffset = serializeSctpStreamParameters(
			builder,
			sctpStreamParameters
		);
	}

	FbsTransport.ProduceDataRequest.startProduceDataRequest(builder);
	FbsTransport.ProduceDataRequest.addDataProducerId(
		builder,
		dataProducerIdOffset
	);
	FbsTransport.ProduceDataRequest.addType(builder, dataProducerTypeToFbs(type));

	if (sctpStreamParametersOffset) {
		FbsTransport.ProduceDataRequest.addSctpStreamParameters(
			builder,
			sctpStreamParametersOffset
		);
	}

	FbsTransport.ProduceDataRequest.addLabel(builder, labelOffset);
	FbsTransport.ProduceDataRequest.addProtocol(builder, protocolOffset);
	FbsTransport.ProduceDataRequest.addPaused(builder, paused);

	return FbsTransport.ProduceDataRequest.endProduceDataRequest(builder);
}

function createConsumeDataRequest({
	builder,
	dataConsumerId,
	dataProducerId,
	type,
	sctpStreamParameters,
	label,
	protocol,
	paused,
	subchannels = [],
}: {
	builder: flatbuffers.Builder;
	dataConsumerId: string;
	dataProducerId: string;
	type: DataConsumerType;
	sctpStreamParameters?: SctpStreamParameters;
	label: string;
	protocol: string;
	paused: boolean;
	subchannels?: number[];
}): number {
	const dataConsumerIdOffset = builder.createString(dataConsumerId);
	const dataProducerIdOffset = builder.createString(dataProducerId);
	const labelOffset = builder.createString(label);
	const protocolOffset = builder.createString(protocol);

	let sctpStreamParametersOffset = 0;

	if (sctpStreamParameters) {
		sctpStreamParametersOffset = serializeSctpStreamParameters(
			builder,
			sctpStreamParameters
		);
	}

	const subchannelsOffset =
		FbsTransport.ConsumeDataRequest.createSubchannelsVector(
			builder,
			subchannels
		);

	FbsTransport.ConsumeDataRequest.startConsumeDataRequest(builder);
	FbsTransport.ConsumeDataRequest.addDataConsumerId(
		builder,
		dataConsumerIdOffset
	);
	FbsTransport.ConsumeDataRequest.addDataProducerId(
		builder,
		dataProducerIdOffset
	);
	FbsTransport.ConsumeDataRequest.addType(builder, dataConsumerTypeToFbs(type));

	if (sctpStreamParametersOffset) {
		FbsTransport.ConsumeDataRequest.addSctpStreamParameters(
			builder,
			sctpStreamParametersOffset
		);
	}

	FbsTransport.ConsumeDataRequest.addLabel(builder, labelOffset);
	FbsTransport.ConsumeDataRequest.addProtocol(builder, protocolOffset);
	FbsTransport.ConsumeDataRequest.addPaused(builder, paused);
	FbsTransport.ConsumeDataRequest.addSubchannels(builder, subchannelsOffset);

	return FbsTransport.ConsumeDataRequest.endConsumeDataRequest(builder);
}

function parseRtpListenerDump(
	binary: FbsTransport.RtpListener
): RtpListenerDump {
	// Retrieve ssrcTable.
	const ssrcTable = utils.parseUint32StringVector(binary, 'ssrcTable');
	// Retrieve midTable.
	const midTable = utils.parseUint32StringVector(binary, 'midTable');
	// Retrieve ridTable.
	const ridTable = utils.parseUint32StringVector(binary, 'ridTable');

	return {
		ssrcTable,
		midTable,
		ridTable,
	};
}

function parseSctpListenerDump(
	binary: FbsTransport.SctpListener
): SctpListenerDump {
	// Retrieve streamIdTable.
	const streamIdTable = utils.parseUint32StringVector(binary, 'streamIdTable');

	return { streamIdTable };
}
