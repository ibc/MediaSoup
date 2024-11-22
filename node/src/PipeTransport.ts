import * as flatbuffers from 'flatbuffers';
import { Logger } from './Logger';
import { EnhancedEventEmitter } from './enhancedEvents';
import * as ortc from './ortc';
import type {
	PipeTransport,
	PipeConsumerOptions,
	PipeTransportDump,
	PipeTransportStat,
	PipeTransportEvents,
	PipeTransportObserver,
	PipeTransportObserverEvents,
} from './PipeTransportTypes';
import type { Transport, TransportTuple, SctpState } from './TransportTypes';
import {
	TransportImpl,
	TransportConstructorOptions,
	parseBaseTransportDump,
	parseBaseTransportStats,
	parseSctpState,
	parseTuple,
	parseTransportTraceEventData,
} from './Transport';
import type { Producer } from './ProducerTypes';
import type { Consumer, ConsumerType } from './ConsumerTypes';
import { ConsumerImpl } from './Consumer';
import type { RtpParameters } from './rtpParametersTypes';
import {
	serializeRtpEncodingParameters,
	serializeRtpParameters,
} from './rtpParametersFbsUtils';
import type { SctpParameters } from './sctpParametersTypes';
import type { SrtpParameters } from './srtpParametersTypes';
import {
	parseSrtpParameters,
	serializeSrtpParameters,
} from './srtpParametersFbsUtils';
import type { AppData } from './types';
import { generateUUIDv4 } from './utils';
import { MediaKind as FbsMediaKind } from './fbs/rtp-parameters/media-kind';
import * as FbsRtpParameters from './fbs/rtp-parameters';
import { Event, Notification } from './fbs/notification';
import * as FbsRequest from './fbs/request';
import * as FbsTransport from './fbs/transport';
import * as FbsPipeTransport from './fbs/pipe-transport';

type PipeTransportConstructorOptions<PipeTransportAppData> =
	TransportConstructorOptions<PipeTransportAppData> & {
		data: PipeTransportData;
	};

export type PipeTransportData = {
	tuple: TransportTuple;
	sctpParameters?: SctpParameters;
	sctpState?: SctpState;
	rtx: boolean;
	srtpParameters?: SrtpParameters;
};

const logger = new Logger('PipeTransport');

export class PipeTransportImpl<PipeTransportAppData extends AppData = AppData>
	extends TransportImpl<
		PipeTransportAppData,
		PipeTransportEvents,
		PipeTransportObserver
	>
	implements Transport, PipeTransport
{
	// PipeTransport data.
	readonly #data: PipeTransportData;

	constructor(options: PipeTransportConstructorOptions<PipeTransportAppData>) {
		const observer: PipeTransportObserver =
			new EnhancedEventEmitter<PipeTransportObserverEvents>();

		super(options, observer);

		logger.debug('constructor()');

		const { data } = options;

		this.#data = {
			tuple: data.tuple,
			sctpParameters: data.sctpParameters,
			sctpState: data.sctpState,
			rtx: data.rtx,
			srtpParameters: data.srtpParameters,
		};

		this.handleWorkerNotifications();
		this.handleListenerError();
	}

	get type(): 'pipe' {
		return 'pipe';
	}

	get observer(): PipeTransportObserver {
		return super.observer;
	}

	get tuple(): TransportTuple {
		return this.#data.tuple;
	}

	get sctpParameters(): SctpParameters | undefined {
		return this.#data.sctpParameters;
	}

	get sctpState(): SctpState | undefined {
		return this.#data.sctpState;
	}

	get srtpParameters(): SrtpParameters | undefined {
		return this.#data.srtpParameters;
	}

	close(): void {
		if (this.closed) {
			return;
		}

		if (this.#data.sctpState) {
			this.#data.sctpState = 'closed';
		}

		super.close();
	}

	routerClosed(): void {
		if (this.closed) {
			return;
		}

		if (this.#data.sctpState) {
			this.#data.sctpState = 'closed';
		}

		super.routerClosed();
	}

	async dump(): Promise<PipeTransportDump> {
		logger.debug('dump()');

		const response = await this.channel.request(
			FbsRequest.Method.TRANSPORT_DUMP,
			undefined,
			undefined,
			this.internal.transportId
		);

		/* Decode Response. */
		const data = new FbsPipeTransport.DumpResponse();

		response.body(data);

		return parsePipeTransportDumpResponse(data);
	}

	async getStats(): Promise<PipeTransportStat[]> {
		logger.debug('getStats()');

		const response = await this.channel.request(
			FbsRequest.Method.TRANSPORT_GET_STATS,
			undefined,
			undefined,
			this.internal.transportId
		);

		/* Decode Response. */
		const data = new FbsPipeTransport.GetStatsResponse();

		response.body(data);

		return [parseGetStatsResponse(data)];
	}

	async connect({
		ip,
		port,
		srtpParameters,
	}: {
		ip: string;
		port: number;
		srtpParameters?: SrtpParameters;
	}): Promise<void> {
		logger.debug('connect()');

		const requestOffset = createConnectRequest({
			builder: this.channel.bufferBuilder,
			ip,
			port,
			srtpParameters,
		});

		// Wait for response.
		const response = await this.channel.request(
			FbsRequest.Method.PIPETRANSPORT_CONNECT,
			FbsRequest.Body.PipeTransport_ConnectRequest,
			requestOffset,
			this.internal.transportId
		);

		/* Decode Response. */
		const data = new FbsPipeTransport.ConnectResponse();

		response.body(data);

		// Update data.
		if (data.tuple()) {
			this.#data.tuple = parseTuple(data.tuple()!);
		}
	}

	async consume<ConsumerAppData extends AppData = AppData>({
		producerId,
		appData,
	}: PipeConsumerOptions<ConsumerAppData>): Promise<Consumer<ConsumerAppData>> {
		logger.debug('consume()');

		if (!producerId || typeof producerId !== 'string') {
			throw new TypeError('missing producerId');
		} else if (appData && typeof appData !== 'object') {
			throw new TypeError('if given, appData must be an object');
		}

		const producer = this.getProducerById(producerId);

		if (!producer) {
			throw Error(`Producer with id "${producerId}" not found`);
		}

		// This may throw.
		const rtpParameters = ortc.getPipeConsumerRtpParameters({
			consumableRtpParameters: producer.consumableRtpParameters,
			enableRtx: this.#data.rtx,
		});

		const consumerId = generateUUIDv4();

		const consumeRequestOffset = createConsumeRequest({
			builder: this.channel.bufferBuilder,
			consumerId,
			producer,
			rtpParameters,
		});

		const response = await this.channel.request(
			FbsRequest.Method.TRANSPORT_CONSUME,
			FbsRequest.Body.Transport_ConsumeRequest,
			consumeRequestOffset,
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
			type: 'pipe' as ConsumerType,
		};

		const consumer: Consumer<ConsumerAppData> = new ConsumerImpl({
			internal: {
				...this.internal,
				consumerId,
			},
			data,
			channel: this.channel,
			appData,
			paused: status.paused,
			producerPaused: status.producerPaused,
		});

		this.consumers.set(consumer.id, consumer);
		consumer.on('@close', () => this.consumers.delete(consumer.id));
		consumer.on('@producerclose', () => this.consumers.delete(consumer.id));

		// Emit observer event.
		this.observer.safeEmit('newconsumer', consumer);

		return consumer;
	}

	private handleWorkerNotifications(): void {
		this.channel.on(
			this.internal.transportId,
			(event: Event, data?: Notification) => {
				switch (event) {
					case Event.TRANSPORT_SCTP_STATE_CHANGE: {
						const notification = new FbsTransport.SctpStateChangeNotification();

						data!.body(notification);

						const sctpState = parseSctpState(notification.sctpState());

						this.#data.sctpState = sctpState;

						this.safeEmit('sctpstatechange', sctpState);

						// Emit observer event.
						this.observer.safeEmit('sctpstatechange', sctpState);

						break;
					}

					case Event.TRANSPORT_TRACE: {
						const notification = new FbsTransport.TraceNotification();

						data!.body(notification);

						const trace = parseTransportTraceEventData(notification);

						this.safeEmit('trace', trace);

						// Emit observer event.
						this.observer.safeEmit('trace', trace);

						break;
					}

					default: {
						logger.error(`ignoring unknown event "${event}"`);
					}
				}
			}
		);
	}

	private handleListenerError(): void {
		this.on('listenererror', (eventName, error) => {
			logger.error(
				`event listener threw an error [eventName:${eventName}]:`,
				error
			);
		});
	}
}

/*
 * flatbuffers helpers.
 */

export function parsePipeTransportDumpResponse(
	binary: FbsPipeTransport.DumpResponse
): PipeTransportDump {
	// Retrieve BaseTransportDump.
	const baseTransportDump = parseBaseTransportDump(binary.base()!);
	// Retrieve RTP Tuple.
	const tuple = parseTuple(binary.tuple()!);

	// Retrieve SRTP Parameters.
	let srtpParameters: SrtpParameters | undefined;

	if (binary.srtpParameters()) {
		srtpParameters = parseSrtpParameters(binary.srtpParameters()!);
	}

	return {
		...baseTransportDump,
		tuple: tuple,
		rtx: binary.rtx(),
		srtpParameters: srtpParameters,
	};
}

function parseGetStatsResponse(
	binary: FbsPipeTransport.GetStatsResponse
): PipeTransportStat {
	const base = parseBaseTransportStats(binary.base()!);

	return {
		...base,
		type: 'pipe-transport',
		tuple: parseTuple(binary.tuple()!),
	};
}

function createConsumeRequest({
	builder,
	consumerId,
	producer,
	rtpParameters,
}: {
	builder: flatbuffers.Builder;
	consumerId: string;
	producer: Producer;
	rtpParameters: RtpParameters;
}): number {
	// Build the request.
	const producerIdOffset = builder.createString(producer.id);
	const consumerIdOffset = builder.createString(consumerId);
	const rtpParametersOffset = serializeRtpParameters(builder, rtpParameters);
	let consumableRtpEncodingsOffset: number | undefined;

	if (producer.consumableRtpParameters.encodings) {
		consumableRtpEncodingsOffset = serializeRtpEncodingParameters(
			builder,
			producer.consumableRtpParameters.encodings
		);
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
	ConsumeRequest.addType(builder, FbsRtpParameters.Type.PIPE);

	if (consumableRtpEncodingsOffset) {
		ConsumeRequest.addConsumableRtpEncodings(
			builder,
			consumableRtpEncodingsOffset
		);
	}

	return ConsumeRequest.endConsumeRequest(builder);
}

function createConnectRequest({
	builder,
	ip,
	port,
	srtpParameters,
}: {
	builder: flatbuffers.Builder;
	ip?: string;
	port?: number;
	srtpParameters?: SrtpParameters;
}): number {
	let ipOffset = 0;
	let srtpParametersOffset = 0;

	if (ip) {
		ipOffset = builder.createString(ip);
	}

	// Serialize SrtpParameters.
	if (srtpParameters) {
		srtpParametersOffset = serializeSrtpParameters(builder, srtpParameters);
	}

	// Create PlainTransportConnectData.
	FbsPipeTransport.ConnectRequest.startConnectRequest(builder);
	FbsPipeTransport.ConnectRequest.addIp(builder, ipOffset);

	if (typeof port === 'number') {
		FbsPipeTransport.ConnectRequest.addPort(builder, port);
	}
	if (srtpParameters) {
		FbsPipeTransport.ConnectRequest.addSrtpParameters(
			builder,
			srtpParametersOffset
		);
	}

	return FbsPipeTransport.ConnectRequest.endConnectRequest(builder);
}
