import { Logger } from './Logger';
import { EnhancedEventEmitter } from './enhancedEvents';
import {
	ConsumerInterface,
	ConsumerType,
	ConsumerScore,
	ConsumerLayers,
	ConsumerDump,
	SimpleConsumerDump,
	SimulcastConsumerDump,
	SvcConsumerDump,
	PipeConsumerDump,
	BaseConsumerDump,
	RtpStreamDump,
	RtpStreamParametersDump,
	RtxStreamDump,
	RtxStreamParameters,
	ConsumerStat,
	ConsumerTraceEventType,
	ConsumerTraceEventData,
	ConsumerEvents,
	ConsumerObserver,
	ConsumerObserverEvents,
} from './ConsumerTypes';
import { Channel } from './Channel';
import { TransportInternal } from './Transport';
import { ProducerStat } from './ProducerTypes';
import {
	MediaKind,
	RtpParameters,
	parseRtpEncodingParameters,
	parseRtpParameters,
} from './RtpParameters';
import { parseRtpStreamStats } from './RtpStream';
import { AppData } from './types';
import * as utils from './utils';
import { Event, Notification } from './fbs/notification';
import { TraceDirection as FbsTraceDirection } from './fbs/common';
import * as FbsRequest from './fbs/request';
import * as FbsTransport from './fbs/transport';
import * as FbsConsumer from './fbs/consumer';
import * as FbsConsumerTraceInfo from './fbs/consumer/trace-info';
import * as FbsRtpStream from './fbs/rtp-stream';
import * as FbsRtxStream from './fbs/rtx-stream';
import { Type as FbsRtpParametersType } from './fbs/rtp-parameters';
import * as FbsRtpParameters from './fbs/rtp-parameters';

type ConsumerInternal = TransportInternal & {
	consumerId: string;
};

type ConsumerData = {
	producerId: string;
	kind: MediaKind;
	rtpParameters: RtpParameters;
	type: ConsumerType;
};

const logger = new Logger('Consumer');

export class ConsumerImpl<ConsumerAppData extends AppData = AppData>
	extends EnhancedEventEmitter<ConsumerEvents>
	implements ConsumerInterface
{
	// Internal data.
	readonly #internal: ConsumerInternal;

	// Consumer data.
	readonly #data: ConsumerData;

	// Channel instance.
	readonly #channel: Channel;

	// Closed flag.
	#closed = false;

	// Custom app data.
	#appData: ConsumerAppData;

	// Paused flag.
	#paused = false;

	// Associated Producer paused flag.
	#producerPaused = false;

	// Current priority.
	#priority = 1;

	// Current score.
	#score: ConsumerScore;

	// Preferred layers.
	#preferredLayers?: ConsumerLayers;

	// Curent layers.
	#currentLayers?: ConsumerLayers;

	// Observer instance.
	readonly #observer: ConsumerObserver =
		new EnhancedEventEmitter<ConsumerObserverEvents>();

	constructor({
		internal,
		data,
		channel,
		appData,
		paused,
		producerPaused,
		score = { score: 10, producerScore: 10, producerScores: [] },
		preferredLayers,
	}: {
		internal: ConsumerInternal;
		data: ConsumerData;
		channel: Channel;
		appData?: ConsumerAppData;
		paused: boolean;
		producerPaused: boolean;
		score?: ConsumerScore;
		preferredLayers?: ConsumerLayers;
	}) {
		super();

		logger.debug('constructor()');

		this.#internal = internal;
		this.#data = data;
		this.#channel = channel;
		this.#paused = paused;
		this.#producerPaused = producerPaused;
		this.#score = score;
		this.#preferredLayers = preferredLayers;
		this.#appData = appData ?? ({} as ConsumerAppData);

		this.handleWorkerNotifications();
	}

	get id(): string {
		return this.#internal.consumerId;
	}

	get producerId(): string {
		return this.#data.producerId;
	}

	get closed(): boolean {
		return this.#closed;
	}

	get kind(): MediaKind {
		return this.#data.kind;
	}

	get rtpParameters(): RtpParameters {
		return this.#data.rtpParameters;
	}

	get type(): ConsumerType {
		return this.#data.type;
	}

	get paused(): boolean {
		return this.#paused;
	}

	get producerPaused(): boolean {
		return this.#producerPaused;
	}

	get priority(): number {
		return this.#priority;
	}

	get score(): ConsumerScore {
		return this.#score;
	}

	get preferredLayers(): ConsumerLayers | undefined {
		return this.#preferredLayers;
	}

	get currentLayers(): ConsumerLayers | undefined {
		return this.#currentLayers;
	}

	get appData(): ConsumerAppData {
		return this.#appData;
	}

	set appData(appData: ConsumerAppData) {
		this.#appData = appData;
	}

	get observer(): ConsumerObserver {
		return this.#observer;
	}

	/**
	 * Just for testing purposes.
	 *
	 * @private
	 */
	get channelForTesting(): Channel {
		return this.#channel;
	}

	close(): void {
		if (this.#closed) {
			return;
		}

		logger.debug('close()');

		this.#closed = true;

		// Remove notification subscriptions.
		this.#channel.removeAllListeners(this.#internal.consumerId);

		/* Build Request. */
		const requestOffset = new FbsTransport.CloseConsumerRequestT(
			this.#internal.consumerId
		).pack(this.#channel.bufferBuilder);

		this.#channel
			.request(
				FbsRequest.Method.TRANSPORT_CLOSE_CONSUMER,
				FbsRequest.Body.Transport_CloseConsumerRequest,
				requestOffset,
				this.#internal.transportId
			)
			.catch(() => {});

		this.emit('@close');

		// Emit observer event.
		this.#observer.safeEmit('close');
	}

	transportClosed(): void {
		if (this.#closed) {
			return;
		}

		logger.debug('transportClosed()');

		this.#closed = true;

		// Remove notification subscriptions.
		this.#channel.removeAllListeners(this.#internal.consumerId);

		this.safeEmit('transportclose');

		// Emit observer event.
		this.#observer.safeEmit('close');
	}

	async dump(): Promise<ConsumerDump> {
		logger.debug('dump()');

		const response = await this.#channel.request(
			FbsRequest.Method.CONSUMER_DUMP,
			undefined,
			undefined,
			this.#internal.consumerId
		);

		/* Decode Response. */
		const data = new FbsConsumer.DumpResponse();

		response.body(data);

		return parseConsumerDumpResponse(data);
	}

	async getStats(): Promise<(ConsumerStat | ProducerStat)[]> {
		logger.debug('getStats()');

		const response = await this.#channel.request(
			FbsRequest.Method.CONSUMER_GET_STATS,
			undefined,
			undefined,
			this.#internal.consumerId
		);

		/* Decode Response. */
		const data = new FbsConsumer.GetStatsResponse();

		response.body(data);

		return parseConsumerStats(data);
	}

	async pause(): Promise<void> {
		logger.debug('pause()');

		await this.#channel.request(
			FbsRequest.Method.CONSUMER_PAUSE,
			undefined,
			undefined,
			this.#internal.consumerId
		);

		const wasPaused = this.#paused;

		this.#paused = true;

		// Emit observer event.
		if (!wasPaused && !this.#producerPaused) {
			this.#observer.safeEmit('pause');
		}
	}

	async resume(): Promise<void> {
		logger.debug('resume()');

		await this.#channel.request(
			FbsRequest.Method.CONSUMER_RESUME,
			undefined,
			undefined,
			this.#internal.consumerId
		);

		const wasPaused = this.#paused;

		this.#paused = false;

		// Emit observer event.
		if (wasPaused && !this.#producerPaused) {
			this.#observer.safeEmit('resume');
		}
	}

	async setPreferredLayers({
		spatialLayer,
		temporalLayer,
	}: ConsumerLayers): Promise<void> {
		logger.debug('setPreferredLayers()');

		if (typeof spatialLayer !== 'number') {
			throw new TypeError('spatialLayer must be a number');
		}
		if (temporalLayer && typeof temporalLayer !== 'number') {
			throw new TypeError('if given, temporalLayer must be a number');
		}

		const builder = this.#channel.bufferBuilder;

		const preferredLayersOffset =
			FbsConsumer.ConsumerLayers.createConsumerLayers(
				builder,
				spatialLayer,
				temporalLayer ?? null
			);
		const requestOffset =
			FbsConsumer.SetPreferredLayersRequest.createSetPreferredLayersRequest(
				builder,
				preferredLayersOffset
			);

		const response = await this.#channel.request(
			FbsRequest.Method.CONSUMER_SET_PREFERRED_LAYERS,
			FbsRequest.Body.Consumer_SetPreferredLayersRequest,
			requestOffset,
			this.#internal.consumerId
		);

		/* Decode Response. */
		const data = new FbsConsumer.SetPreferredLayersResponse();

		let preferredLayers: ConsumerLayers | undefined;

		// Response is empty for non Simulcast Consumers.
		if (response.body(data)) {
			const status = data.unpack();

			if (status.preferredLayers) {
				preferredLayers = {
					spatialLayer: status.preferredLayers.spatialLayer,
					temporalLayer: status.preferredLayers.temporalLayer ?? undefined,
				};
			}
		}

		this.#preferredLayers = preferredLayers;
	}

	async setPriority(priority: number): Promise<void> {
		logger.debug('setPriority()');

		if (typeof priority !== 'number' || priority < 0) {
			throw new TypeError('priority must be a positive number');
		}

		const requestOffset =
			FbsConsumer.SetPriorityRequest.createSetPriorityRequest(
				this.#channel.bufferBuilder,
				priority
			);

		const response = await this.#channel.request(
			FbsRequest.Method.CONSUMER_SET_PRIORITY,
			FbsRequest.Body.Consumer_SetPriorityRequest,
			requestOffset,
			this.#internal.consumerId
		);

		const data = new FbsConsumer.SetPriorityResponse();

		response.body(data);

		const status = data.unpack();

		this.#priority = status.priority;
	}

	async unsetPriority(): Promise<void> {
		logger.debug('unsetPriority()');

		await this.setPriority(1);
	}

	async requestKeyFrame(): Promise<void> {
		logger.debug('requestKeyFrame()');

		await this.#channel.request(
			FbsRequest.Method.CONSUMER_REQUEST_KEY_FRAME,
			undefined,
			undefined,
			this.#internal.consumerId
		);
	}

	async enableTraceEvent(types: ConsumerTraceEventType[] = []): Promise<void> {
		logger.debug('enableTraceEvent()');

		if (!Array.isArray(types)) {
			throw new TypeError('types must be an array');
		}
		if (types.find(type => typeof type !== 'string')) {
			throw new TypeError('every type must be a string');
		}

		// Convert event types.
		const fbsEventTypes: FbsConsumer.TraceEventType[] = [];

		for (const eventType of types) {
			try {
				fbsEventTypes.push(consumerTraceEventTypeToFbs(eventType));
			} catch (error) {
				logger.warn('enableTraceEvent() | [error:${error}]');
			}
		}

		/* Build Request. */
		const requestOffset = new FbsConsumer.EnableTraceEventRequestT(
			fbsEventTypes
		).pack(this.#channel.bufferBuilder);

		await this.#channel.request(
			FbsRequest.Method.CONSUMER_ENABLE_TRACE_EVENT,
			FbsRequest.Body.Consumer_EnableTraceEventRequest,
			requestOffset,
			this.#internal.consumerId
		);
	}

	private handleWorkerNotifications(): void {
		this.#channel.on(
			this.#internal.consumerId,
			(event: Event, data?: Notification) => {
				switch (event) {
					case Event.CONSUMER_PRODUCER_CLOSE: {
						if (this.#closed) {
							break;
						}

						this.#closed = true;

						// Remove notification subscriptions.
						this.#channel.removeAllListeners(this.#internal.consumerId);

						this.emit('@producerclose');
						this.safeEmit('producerclose');

						// Emit observer event.
						this.#observer.safeEmit('close');

						break;
					}

					case Event.CONSUMER_PRODUCER_PAUSE: {
						if (this.#producerPaused) {
							break;
						}

						this.#producerPaused = true;

						this.safeEmit('producerpause');

						// Emit observer event.
						if (!this.#paused) {
							this.#observer.safeEmit('pause');
						}

						break;
					}

					case Event.CONSUMER_PRODUCER_RESUME: {
						if (!this.#producerPaused) {
							break;
						}

						this.#producerPaused = false;

						this.safeEmit('producerresume');

						// Emit observer event.
						if (!this.#paused) {
							this.#observer.safeEmit('resume');
						}

						break;
					}

					case Event.CONSUMER_SCORE: {
						const notification = new FbsConsumer.ScoreNotification();

						data!.body(notification);

						const score: ConsumerScore = notification.score()!.unpack();

						this.#score = score;

						this.safeEmit('score', score);

						// Emit observer event.
						this.#observer.safeEmit('score', score);

						break;
					}

					case Event.CONSUMER_LAYERS_CHANGE: {
						const notification = new FbsConsumer.LayersChangeNotification();

						data!.body(notification);

						const layers: ConsumerLayers | undefined = notification.layers()
							? parseConsumerLayers(notification.layers()!)
							: undefined;

						this.#currentLayers = layers;

						this.safeEmit('layerschange', layers);

						// Emit observer event.
						this.#observer.safeEmit('layerschange', layers);

						break;
					}

					case Event.CONSUMER_TRACE: {
						const notification = new FbsConsumer.TraceNotification();

						data!.body(notification);

						const trace: ConsumerTraceEventData =
							parseTraceEventData(notification);

						this.safeEmit('trace', trace);

						// Emit observer event.
						this.observer.safeEmit('trace', trace);

						this.safeEmit('trace', trace);

						// Emit observer event.
						this.#observer.safeEmit('trace', trace);

						break;
					}

					case Event.CONSUMER_RTP: {
						if (this.#closed) {
							break;
						}

						const notification = new FbsConsumer.RtpNotification();

						data!.body(notification);

						this.safeEmit('rtp', Buffer.from(notification.dataArray()!));

						break;
					}

					default: {
						logger.error(`ignoring unknown event "${event}"`);
					}
				}
			}
		);
	}
}

export function parseTraceEventData(
	trace: FbsConsumer.TraceNotification
): ConsumerTraceEventData {
	let info: any;

	if (trace.infoType() !== FbsConsumer.TraceInfo.NONE) {
		const accessor = trace.info.bind(trace);

		info = FbsConsumerTraceInfo.unionToTraceInfo(trace.infoType(), accessor);

		trace.info(info);
	}

	return {
		type: consumerTraceEventTypeFromFbs(trace.type()),
		timestamp: Number(trace.timestamp()),
		direction:
			trace.direction() === FbsTraceDirection.DIRECTION_IN ? 'in' : 'out',
		info: info ? info.unpack() : undefined,
	};
}

function consumerTraceEventTypeToFbs(
	eventType: ConsumerTraceEventType
): FbsConsumer.TraceEventType {
	switch (eventType) {
		case 'keyframe': {
			return FbsConsumer.TraceEventType.KEYFRAME;
		}

		case 'fir': {
			return FbsConsumer.TraceEventType.FIR;
		}

		case 'nack': {
			return FbsConsumer.TraceEventType.NACK;
		}

		case 'pli': {
			return FbsConsumer.TraceEventType.PLI;
		}

		case 'rtp': {
			return FbsConsumer.TraceEventType.RTP;
		}

		default: {
			throw new TypeError(`invalid ConsumerTraceEventType: ${eventType}`);
		}
	}
}

function consumerTraceEventTypeFromFbs(
	traceType: FbsConsumer.TraceEventType
): ConsumerTraceEventType {
	switch (traceType) {
		case FbsConsumer.TraceEventType.KEYFRAME: {
			return 'keyframe';
		}

		case FbsConsumer.TraceEventType.FIR: {
			return 'fir';
		}

		case FbsConsumer.TraceEventType.NACK: {
			return 'nack';
		}

		case FbsConsumer.TraceEventType.PLI: {
			return 'pli';
		}

		case FbsConsumer.TraceEventType.RTP: {
			return 'rtp';
		}

		default: {
			throw new TypeError(`invalid FbsConsumer.TraceEventType: ${traceType}`);
		}
	}
}

function parseConsumerLayers(data: FbsConsumer.ConsumerLayers): ConsumerLayers {
	const spatialLayer = data.spatialLayer();
	const temporalLayer =
		data.temporalLayer() !== null ? data.temporalLayer()! : undefined;

	return {
		spatialLayer,
		temporalLayer,
	};
}

function parseRtpStream(data: FbsRtpStream.Dump): RtpStreamDump {
	const params = parseRtpStreamParameters(data.params()!);

	let rtxStream: RtxStreamDump | undefined;

	if (data.rtxStream()) {
		rtxStream = parseRtxStream(data.rtxStream()!);
	}

	return {
		params,
		score: data.score(),
		rtxStream,
	};
}

function parseRtpStreamParameters(
	data: FbsRtpStream.Params
): RtpStreamParametersDump {
	return {
		encodingIdx: data.encodingIdx(),
		ssrc: data.ssrc(),
		payloadType: data.payloadType(),
		mimeType: data.mimeType()!,
		clockRate: data.clockRate(),
		rid: data.rid()!.length > 0 ? data.rid()! : undefined,
		cname: data.cname()!,
		rtxSsrc: data.rtxSsrc() !== null ? data.rtxSsrc()! : undefined,
		rtxPayloadType:
			data.rtxPayloadType() !== null ? data.rtxPayloadType()! : undefined,
		useNack: data.useNack(),
		usePli: data.usePli(),
		useFir: data.useFir(),
		useInBandFec: data.useInBandFec(),
		useDtx: data.useDtx(),
		spatialLayers: data.spatialLayers(),
		temporalLayers: data.temporalLayers(),
	};
}

function parseRtxStream(data: FbsRtxStream.RtxDump): RtxStreamDump {
	const params = parseRtxStreamParameters(data.params()!);

	return {
		params,
	};
}

function parseRtxStreamParameters(
	data: FbsRtxStream.Params
): RtxStreamParameters {
	return {
		ssrc: data.ssrc(),
		payloadType: data.payloadType(),
		mimeType: data.mimeType()!,
		clockRate: data.clockRate(),
		rrid: data.rrid()!.length > 0 ? data.rrid()! : undefined,
		cname: data.cname()!,
	};
}

function parseBaseConsumerDump(
	data: FbsConsumer.BaseConsumerDump
): BaseConsumerDump {
	return {
		id: data.id()!,
		producerId: data.producerId()!,
		kind: data.kind() === FbsRtpParameters.MediaKind.AUDIO ? 'audio' : 'video',
		rtpParameters: parseRtpParameters(data.rtpParameters()!),
		consumableRtpEncodings:
			data.consumableRtpEncodingsLength() > 0
				? utils.parseVector(
						data,
						'consumableRtpEncodings',
						parseRtpEncodingParameters
					)
				: undefined,
		traceEventTypes: utils.parseVector(
			data,
			'traceEventTypes',
			consumerTraceEventTypeFromFbs
		),
		supportedCodecPayloadTypes: utils.parseVector(
			data,
			'supportedCodecPayloadTypes'
		),
		paused: data.paused(),
		producerPaused: data.producerPaused(),
		priority: data.priority(),
	};
}

function parseSimpleConsumerDump(
	data: FbsConsumer.ConsumerDump
): SimpleConsumerDump {
	const base = parseBaseConsumerDump(data.base()!);
	const rtpStream = parseRtpStream(data.rtpStreams(0)!);

	return {
		...base,
		type: 'simple',
		rtpStream,
	};
}

function parseSimulcastConsumerDump(
	data: FbsConsumer.ConsumerDump
): SimulcastConsumerDump {
	const base = parseBaseConsumerDump(data.base()!);
	const rtpStream = parseRtpStream(data.rtpStreams(0)!);

	return {
		...base,
		type: 'simulcast',
		rtpStream,
		preferredSpatialLayer: data.preferredSpatialLayer()!,
		targetSpatialLayer: data.targetSpatialLayer()!,
		currentSpatialLayer: data.currentSpatialLayer()!,
		preferredTemporalLayer: data.preferredTemporalLayer()!,
		targetTemporalLayer: data.targetTemporalLayer()!,
		currentTemporalLayer: data.currentTemporalLayer()!,
	};
}

function parseSvcConsumerDump(data: FbsConsumer.ConsumerDump): SvcConsumerDump {
	const dump = parseSimulcastConsumerDump(data);

	dump.type = 'svc';

	return dump;
}

function parsePipeConsumerDump(
	data: FbsConsumer.ConsumerDump
): PipeConsumerDump {
	const base = parseBaseConsumerDump(data.base()!);
	const rtpStreams = utils.parseVector(data, 'rtpStreams', parseRtpStream);

	return {
		...base,
		type: 'pipe',
		rtpStreams,
	};
}

function parseConsumerDumpResponse(
	data: FbsConsumer.DumpResponse
): ConsumerDump {
	const type = data.data()!.base()!.type();

	switch (type) {
		case FbsRtpParametersType.SIMPLE: {
			const dump = new FbsConsumer.ConsumerDump();

			data.data(dump);

			return parseSimpleConsumerDump(dump);
		}

		case FbsRtpParametersType.SIMULCAST: {
			const dump = new FbsConsumer.ConsumerDump();

			data.data(dump);

			return parseSimulcastConsumerDump(dump);
		}

		case FbsRtpParametersType.SVC: {
			const dump = new FbsConsumer.ConsumerDump();

			data.data(dump);

			return parseSvcConsumerDump(dump);
		}

		case FbsRtpParametersType.PIPE: {
			const dump = new FbsConsumer.ConsumerDump();

			data.data(dump);

			return parsePipeConsumerDump(dump);
		}

		default: {
			throw new TypeError(`invalid Consumer type: ${type}`);
		}
	}
}

function parseConsumerStats(
	binary: FbsConsumer.GetStatsResponse
): (ConsumerStat | ProducerStat)[] {
	return utils.parseVector(binary, 'stats', parseRtpStreamStats);
}
