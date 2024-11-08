import { Logger } from './Logger';
import { EnhancedEventEmitter } from './enhancedEvents';
import type {
	Producer,
	ProducerType,
	ProducerScore,
	ProducerVideoOrientation,
	ProducerDump,
	ProducerStat,
	ProducerTraceEventType,
	ProducerTraceEventData,
	ProducerEvents,
	ProducerObserver,
	ProducerObserverEvents,
} from './ProducerTypes';
import { Channel } from './Channel';
import type { TransportInternal } from './Transport';
import type { MediaKind, RtpParameters } from './rtpParametersTypes';
import { parseRtpParameters } from './rtpParametersFbsUtils';
import { parseRtpStreamRecvStats } from './RtpStream';
import type { AppData } from './types';
import * as fbsUtils from './fbsUtils';
import { Event, Notification } from './fbs/notification';
import { TraceDirection as FbsTraceDirection } from './fbs/common';
import * as FbsNotification from './fbs/notification';
import * as FbsRequest from './fbs/request';
import * as FbsTransport from './fbs/transport';
import * as FbsProducer from './fbs/producer';
import * as FbsProducerTraceInfo from './fbs/producer/trace-info';
import * as FbsRtpParameters from './fbs/rtp-parameters';

type ProducerInternal = TransportInternal & {
	producerId: string;
};

const logger = new Logger('Producer');

type ProducerData = {
	kind: MediaKind;
	rtpParameters: RtpParameters;
	type: ProducerType;
	consumableRtpParameters: RtpParameters;
};

export class ProducerImpl<ProducerAppData extends AppData = AppData>
	extends EnhancedEventEmitter<ProducerEvents>
	implements Producer
{
	// Internal data.
	readonly #internal: ProducerInternal;

	// Producer data.
	readonly #data: ProducerData;

	// Channel instance.
	readonly #channel: Channel;

	// Closed flag.
	#closed = false;

	// Paused flag.
	#paused = false;

	// Custom app data.
	#appData: ProducerAppData;

	// Current score.
	#score: ProducerScore[] = [];

	// Observer instance.
	readonly #observer: ProducerObserver =
		new EnhancedEventEmitter<ProducerObserverEvents>();

	constructor({
		internal,
		data,
		channel,
		appData,
		paused,
	}: {
		internal: ProducerInternal;
		data: ProducerData;
		channel: Channel;
		appData?: ProducerAppData;
		paused: boolean;
	}) {
		super();

		logger.debug('constructor()');

		this.#internal = internal;
		this.#data = data;
		this.#channel = channel;
		this.#paused = paused;
		this.#appData = appData ?? ({} as ProducerAppData);

		this.handleWorkerNotifications();
	}

	get id(): string {
		return this.#internal.producerId;
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

	get type(): ProducerType {
		return this.#data.type;
	}

	get consumableRtpParameters(): RtpParameters {
		return this.#data.consumableRtpParameters;
	}

	get paused(): boolean {
		return this.#paused;
	}

	get score(): ProducerScore[] {
		return this.#score;
	}

	get appData(): ProducerAppData {
		return this.#appData;
	}

	set appData(appData: ProducerAppData) {
		this.#appData = appData;
	}

	get observer(): ProducerObserver {
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
		this.#channel.removeAllListeners(this.#internal.producerId);

		/* Build Request. */
		const requestOffset = new FbsTransport.CloseProducerRequestT(
			this.#internal.producerId
		).pack(this.#channel.bufferBuilder);

		this.#channel
			.request(
				FbsRequest.Method.TRANSPORT_CLOSE_PRODUCER,
				FbsRequest.Body.Transport_CloseProducerRequest,
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
		this.#channel.removeAllListeners(this.#internal.producerId);

		this.safeEmit('transportclose');

		// Emit observer event.
		this.#observer.safeEmit('close');
	}

	async dump(): Promise<ProducerDump> {
		logger.debug('dump()');

		const response = await this.#channel.request(
			FbsRequest.Method.PRODUCER_DUMP,
			undefined,
			undefined,
			this.#internal.producerId
		);

		/* Decode Response. */
		const dumpResponse = new FbsProducer.DumpResponse();

		response.body(dumpResponse);

		return parseProducerDump(dumpResponse);
	}

	async getStats(): Promise<ProducerStat[]> {
		logger.debug('getStats()');

		const response = await this.#channel.request(
			FbsRequest.Method.PRODUCER_GET_STATS,
			undefined,
			undefined,
			this.#internal.producerId
		);

		/* Decode Response. */
		const data = new FbsProducer.GetStatsResponse();

		response.body(data);

		return parseProducerStats(data);
	}

	async pause(): Promise<void> {
		logger.debug('pause()');

		await this.#channel.request(
			FbsRequest.Method.PRODUCER_PAUSE,
			undefined,
			undefined,
			this.#internal.producerId
		);

		const wasPaused = this.#paused;

		this.#paused = true;

		// Emit observer event.
		if (!wasPaused) {
			this.#observer.safeEmit('pause');
		}
	}

	async resume(): Promise<void> {
		logger.debug('resume()');

		await this.#channel.request(
			FbsRequest.Method.PRODUCER_RESUME,
			undefined,
			undefined,
			this.#internal.producerId
		);

		const wasPaused = this.#paused;

		this.#paused = false;

		// Emit observer event.
		if (wasPaused) {
			this.#observer.safeEmit('resume');
		}
	}

	async enableTraceEvent(types: ProducerTraceEventType[] = []): Promise<void> {
		logger.debug('enableTraceEvent()');

		if (!Array.isArray(types)) {
			throw new TypeError('types must be an array');
		}
		if (types.find(type => typeof type !== 'string')) {
			throw new TypeError('every type must be a string');
		}

		// Convert event types.
		const fbsEventTypes: FbsProducer.TraceEventType[] = [];

		for (const eventType of types) {
			try {
				fbsEventTypes.push(producerTraceEventTypeToFbs(eventType));
			} catch (error) {
				logger.warn('enableTraceEvent() | [error:${error}]');
			}
		}

		/* Build Request. */
		const requestOffset = new FbsProducer.EnableTraceEventRequestT(
			fbsEventTypes
		).pack(this.#channel.bufferBuilder);

		await this.#channel.request(
			FbsRequest.Method.PRODUCER_ENABLE_TRACE_EVENT,
			FbsRequest.Body.Producer_EnableTraceEventRequest,
			requestOffset,
			this.#internal.producerId
		);
	}

	send(rtpPacket: Buffer): void {
		if (!Buffer.isBuffer(rtpPacket)) {
			throw new TypeError('rtpPacket must be a Buffer');
		}

		const builder = this.#channel.bufferBuilder;
		const dataOffset = FbsProducer.SendNotification.createDataVector(
			builder,
			rtpPacket
		);
		const notificationOffset =
			FbsProducer.SendNotification.createSendNotification(builder, dataOffset);

		this.#channel.notify(
			FbsNotification.Event.PRODUCER_SEND,
			FbsNotification.Body.Producer_SendNotification,
			notificationOffset,
			this.#internal.producerId
		);
	}

	private handleWorkerNotifications(): void {
		this.#channel.on(
			this.#internal.producerId,
			(event: Event, data?: Notification) => {
				switch (event) {
					case Event.PRODUCER_SCORE: {
						const notification = new FbsProducer.ScoreNotification();

						data!.body(notification);

						const score: ProducerScore[] = fbsUtils.parseVector(
							notification,
							'scores',
							parseProducerScore
						);

						this.#score = score;

						this.safeEmit('score', score);

						// Emit observer event.
						this.#observer.safeEmit('score', score);

						break;
					}

					case Event.PRODUCER_VIDEO_ORIENTATION_CHANGE: {
						const notification =
							new FbsProducer.VideoOrientationChangeNotification();

						data!.body(notification);

						const videoOrientation: ProducerVideoOrientation =
							notification.unpack();

						this.safeEmit('videoorientationchange', videoOrientation);

						// Emit observer event.
						this.#observer.safeEmit('videoorientationchange', videoOrientation);

						break;
					}

					case Event.PRODUCER_TRACE: {
						const notification = new FbsProducer.TraceNotification();

						data!.body(notification);

						const trace: ProducerTraceEventData =
							parseTraceEventData(notification);

						this.safeEmit('trace', trace);

						// Emit observer event.
						this.#observer.safeEmit('trace', trace);

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

export function producerTypeFromFbs(type: FbsRtpParameters.Type): ProducerType {
	switch (type) {
		case FbsRtpParameters.Type.SIMPLE: {
			return 'simple';
		}

		case FbsRtpParameters.Type.SIMULCAST: {
			return 'simulcast';
		}

		case FbsRtpParameters.Type.SVC: {
			return 'svc';
		}

		default: {
			throw new TypeError(`invalid FbsRtpParameters.Type: ${type}`);
		}
	}
}

export function producerTypeToFbs(type: ProducerType): FbsRtpParameters.Type {
	switch (type) {
		case 'simple': {
			return FbsRtpParameters.Type.SIMPLE;
		}

		case 'simulcast': {
			return FbsRtpParameters.Type.SIMULCAST;
		}

		case 'svc': {
			return FbsRtpParameters.Type.SVC;
		}

		default: {
			throw new TypeError(`invalid ProducerType: ${type}`);
		}
	}
}

function producerTraceEventTypeToFbs(
	eventType: ProducerTraceEventType
): FbsProducer.TraceEventType {
	switch (eventType) {
		case 'keyframe': {
			return FbsProducer.TraceEventType.KEYFRAME;
		}

		case 'fir': {
			return FbsProducer.TraceEventType.FIR;
		}

		case 'nack': {
			return FbsProducer.TraceEventType.NACK;
		}

		case 'pli': {
			return FbsProducer.TraceEventType.PLI;
		}

		case 'rtp': {
			return FbsProducer.TraceEventType.RTP;
		}

		case 'sr': {
			return FbsProducer.TraceEventType.SR;
		}

		default: {
			throw new TypeError(`invalid ProducerTraceEventType: ${eventType}`);
		}
	}
}

function producerTraceEventTypeFromFbs(
	eventType: FbsProducer.TraceEventType
): ProducerTraceEventType {
	switch (eventType) {
		case FbsProducer.TraceEventType.KEYFRAME: {
			return 'keyframe';
		}

		case FbsProducer.TraceEventType.FIR: {
			return 'fir';
		}

		case FbsProducer.TraceEventType.NACK: {
			return 'nack';
		}

		case FbsProducer.TraceEventType.PLI: {
			return 'pli';
		}

		case FbsProducer.TraceEventType.RTP: {
			return 'rtp';
		}

		case FbsProducer.TraceEventType.SR: {
			return 'sr';
		}
	}
}

export function parseProducerDump(
	data: FbsProducer.DumpResponse
): ProducerDump {
	return {
		id: data.id()!,
		kind: data.kind() === FbsRtpParameters.MediaKind.AUDIO ? 'audio' : 'video',
		type: producerTypeFromFbs(data.type()),
		rtpParameters: parseRtpParameters(data.rtpParameters()!),
		// NOTE: optional values are represented with null instead of undefined.
		// TODO: Make flatbuffers TS return undefined instead of null.
		rtpMapping: data.rtpMapping() ? data.rtpMapping()!.unpack() : undefined,
		// NOTE: optional values are represented with null instead of undefined.
		// TODO: Make flatbuffers TS return undefined instead of null.
		rtpStreams:
			data.rtpStreamsLength() > 0
				? fbsUtils.parseVector(data, 'rtpStreams', (rtpStream: any) =>
						rtpStream.unpack()
					)
				: undefined,
		traceEventTypes: fbsUtils.parseVector<ProducerTraceEventType>(
			data,
			'traceEventTypes',
			producerTraceEventTypeFromFbs
		),
		paused: data.paused(),
	};
}

function parseProducerStats(
	binary: FbsProducer.GetStatsResponse
): ProducerStat[] {
	return fbsUtils.parseVector(binary, 'stats', parseRtpStreamRecvStats);
}

function parseProducerScore(binary: FbsProducer.Score): ProducerScore {
	return {
		encodingIdx: binary.encodingIdx(),
		ssrc: binary.ssrc(),
		rid: binary.rid() ?? undefined,
		score: binary.score(),
	};
}

function parseTraceEventData(
	trace: FbsProducer.TraceNotification
): ProducerTraceEventData {
	let info: any;

	if (trace.infoType() !== FbsProducer.TraceInfo.NONE) {
		const accessor = trace.info.bind(trace);

		info = FbsProducerTraceInfo.unionToTraceInfo(trace.infoType(), accessor);

		trace.info(info);
	}

	return {
		type: producerTraceEventTypeFromFbs(trace.type()),
		timestamp: Number(trace.timestamp()),
		direction:
			trace.direction() === FbsTraceDirection.DIRECTION_IN ? 'in' : 'out',
		info: info ? info.unpack() : undefined,
	};
}
