import * as flatbuffers from 'flatbuffers';
import { Logger } from './Logger';
import { EnhancedEventEmitter } from './enhancedEvents';
import type {
	PlainTransport,
	PlainTransportDump,
	PlainTransportStat,
	PlainTransportEvents,
	PlainTransportObserver,
	PlainTransportObserverEvents,
} from './PlainTransportTypes';
import type { Transport, TransportTuple, SctpState } from './TransportTypes';
import {
	TransportImpl,
	TransportConstructorOptions,
	parseSctpState,
	parseTuple,
	parseBaseTransportDump,
	parseBaseTransportStats,
	parseTransportTraceEventData,
} from './Transport';
import type { SctpParameters } from './sctpParametersTypes';
import type { SrtpParameters } from './srtpParametersTypes';
import {
	parseSrtpParameters,
	serializeSrtpParameters,
} from './srtpParametersFbsUtils';
import type { AppData } from './types';
import { Event, Notification } from './fbs/notification';
import * as FbsRequest from './fbs/request';
import * as FbsTransport from './fbs/transport';
import * as FbsPlainTransport from './fbs/plain-transport';

type PlainTransportConstructorOptions<PlainTransportAppData> =
	TransportConstructorOptions<PlainTransportAppData> & {
		data: PlainTransportData;
	};

export type PlainTransportData = {
	rtcpMux?: boolean;
	comedia?: boolean;
	tuple: TransportTuple;
	rtcpTuple?: TransportTuple;
	sctpParameters?: SctpParameters;
	sctpState?: SctpState;
	srtpParameters?: SrtpParameters;
};

const logger = new Logger('PlainTransport');

export class PlainTransportImpl<PlainTransportAppData extends AppData = AppData>
	extends TransportImpl<
		PlainTransportAppData,
		PlainTransportEvents,
		PlainTransportObserver
	>
	implements Transport, PlainTransport
{
	// PlainTransport data.
	readonly #data: PlainTransportData;

	constructor(
		options: PlainTransportConstructorOptions<PlainTransportAppData>
	) {
		const observer: PlainTransportObserver =
			new EnhancedEventEmitter<PlainTransportObserverEvents>();

		super(options, observer);

		logger.debug('constructor()');

		const { data } = options;

		this.#data = {
			rtcpMux: data.rtcpMux,
			comedia: data.comedia,
			tuple: data.tuple,
			rtcpTuple: data.rtcpTuple,
			sctpParameters: data.sctpParameters,
			sctpState: data.sctpState,
			srtpParameters: data.srtpParameters,
		};

		this.handleWorkerNotifications();
		this.handleListenerError();
	}

	get type(): 'plain' {
		return 'plain';
	}

	get observer(): PlainTransportObserver {
		return super.observer;
	}

	get tuple(): TransportTuple {
		return this.#data.tuple;
	}

	get rtcpTuple(): TransportTuple | undefined {
		return this.#data.rtcpTuple;
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

	async dump(): Promise<PlainTransportDump> {
		logger.debug('dump()');

		const response = await this.channel.request(
			FbsRequest.Method.TRANSPORT_DUMP,
			undefined,
			undefined,
			this.internal.transportId
		);

		/* Decode Response. */
		const data = new FbsPlainTransport.DumpResponse();

		response.body(data);

		return parsePlainTransportDumpResponse(data);
	}

	async getStats(): Promise<PlainTransportStat[]> {
		logger.debug('getStats()');

		const response = await this.channel.request(
			FbsRequest.Method.TRANSPORT_GET_STATS,
			undefined,
			undefined,
			this.internal.transportId
		);

		/* Decode Response. */
		const data = new FbsPlainTransport.GetStatsResponse();

		response.body(data);

		return [parseGetStatsResponse(data)];
	}

	async connect({
		ip,
		port,
		rtcpPort,
		srtpParameters,
	}: {
		ip?: string;
		port?: number;
		rtcpPort?: number;
		srtpParameters?: SrtpParameters;
	}): Promise<void> {
		logger.debug('connect()');

		const requestOffset = createConnectRequest({
			builder: this.channel.bufferBuilder,
			ip,
			port,
			rtcpPort,
			srtpParameters,
		});

		// Wait for response.
		const response = await this.channel.request(
			FbsRequest.Method.PLAINTRANSPORT_CONNECT,
			FbsRequest.Body.PlainTransport_ConnectRequest,
			requestOffset,
			this.internal.transportId
		);

		/* Decode Response. */
		const data = new FbsPlainTransport.ConnectResponse();

		response.body(data);

		// Update data.
		if (data.tuple()) {
			this.#data.tuple = parseTuple(data.tuple()!);
		}

		if (data.rtcpTuple()) {
			this.#data.rtcpTuple = parseTuple(data.rtcpTuple()!);
		}

		if (data.srtpParameters()) {
			this.#data.srtpParameters = parseSrtpParameters(data.srtpParameters()!);
		}
	}

	private handleWorkerNotifications(): void {
		this.channel.on(
			this.internal.transportId,
			(event: Event, data?: Notification) => {
				switch (event) {
					case Event.PLAINTRANSPORT_TUPLE: {
						const notification = new FbsPlainTransport.TupleNotification();

						data!.body(notification);

						const tuple = parseTuple(notification.tuple()!);

						this.#data.tuple = tuple;

						this.safeEmit('tuple', tuple);

						// Emit observer event.
						this.observer.safeEmit('tuple', tuple);

						break;
					}

					case Event.PLAINTRANSPORT_RTCP_TUPLE: {
						const notification = new FbsPlainTransport.RtcpTupleNotification();

						data!.body(notification);

						const rtcpTuple = parseTuple(notification.tuple()!);

						this.#data.rtcpTuple = rtcpTuple;

						this.safeEmit('rtcptuple', rtcpTuple);

						// Emit observer event.
						this.observer.safeEmit('rtcptuple', rtcpTuple);

						break;
					}

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

export function parsePlainTransportDumpResponse(
	binary: FbsPlainTransport.DumpResponse
): PlainTransportDump {
	// Retrieve BaseTransportDump.
	const baseTransportDump = parseBaseTransportDump(binary.base()!);
	// Retrieve RTP Tuple.
	const tuple = parseTuple(binary.tuple()!);

	// Retrieve RTCP Tuple.
	let rtcpTuple: TransportTuple | undefined;

	if (binary.rtcpTuple()) {
		rtcpTuple = parseTuple(binary.rtcpTuple()!);
	}

	// Retrieve SRTP Parameters.
	let srtpParameters: SrtpParameters | undefined;

	if (binary.srtpParameters()) {
		srtpParameters = parseSrtpParameters(binary.srtpParameters()!);
	}

	return {
		...baseTransportDump,
		rtcpMux: binary.rtcpMux(),
		comedia: binary.comedia(),
		tuple: tuple,
		rtcpTuple: rtcpTuple,
		srtpParameters: srtpParameters,
	};
}

function parseGetStatsResponse(
	binary: FbsPlainTransport.GetStatsResponse
): PlainTransportStat {
	const base = parseBaseTransportStats(binary.base()!);

	return {
		...base,
		type: 'plain-rtp-transport',
		rtcpMux: binary.rtcpMux(),
		comedia: binary.comedia(),
		tuple: parseTuple(binary.tuple()!),
		rtcpTuple: binary.rtcpTuple() ? parseTuple(binary.rtcpTuple()!) : undefined,
	};
}

function createConnectRequest({
	builder,
	ip,
	port,
	rtcpPort,
	srtpParameters,
}: {
	builder: flatbuffers.Builder;
	ip?: string;
	port?: number;
	rtcpPort?: number;
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
	FbsPlainTransport.ConnectRequest.startConnectRequest(builder);
	FbsPlainTransport.ConnectRequest.addIp(builder, ipOffset);

	if (typeof port === 'number') {
		FbsPlainTransport.ConnectRequest.addPort(builder, port);
	}
	if (typeof rtcpPort === 'number') {
		FbsPlainTransport.ConnectRequest.addRtcpPort(builder, rtcpPort);
	}
	if (srtpParameters) {
		FbsPlainTransport.ConnectRequest.addSrtpParameters(
			builder,
			srtpParametersOffset
		);
	}

	return FbsPlainTransport.ConnectRequest.endConnectRequest(builder);
}
