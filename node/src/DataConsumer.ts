import { Logger } from './Logger';
import { EnhancedEventEmitter } from './enhancedEvents';
import { Channel } from './Channel';
import { TransportInternal } from './Transport';
import {
	SctpStreamParameters,
	parseSctpStreamParameters,
} from './SctpParameters';
import { AppData } from './types';
import * as utils from './utils';
import { Event, Notification } from './fbs/notification';
import * as FbsTransport from './fbs/transport';
import * as FbsRequest from './fbs/request';
import * as FbsDataConsumer from './fbs/data-consumer';
import * as FbsDataProducer from './fbs/data-producer';

export type DataConsumerOptions<DataConsumerAppData extends AppData = AppData> =
	{
		/**
		 * The id of the DataProducer to consume.
		 */
		dataProducerId: string;

		/**
		 * Just if consuming over SCTP.
		 * Whether data messages must be received in order. If true the messages will
		 * be sent reliably. Defaults to the value in the DataProducer if it has type
		 * 'sctp' or to true if it has type 'direct'.
		 */
		ordered?: boolean;

		/**
		 * Just if consuming over SCTP.
		 * When ordered is false indicates the time (in milliseconds) after which a
		 * SCTP packet will stop being retransmitted. Defaults to the value in the
		 * DataProducer if it has type 'sctp' or unset if it has type 'direct'.
		 */
		maxPacketLifeTime?: number;

		/**
		 * Just if consuming over SCTP.
		 * When ordered is false indicates the maximum number of times a packet will
		 * be retransmitted. Defaults to the value in the DataProducer if it has type
		 * 'sctp' or unset if it has type 'direct'.
		 */
		maxRetransmits?: number;

		/**
		 * Just if consuming over SCTP.
		 * SCTP stream id. If not provided defaults to an arbitrary available stream ID.
		 */
		sctpStreamId?: number;

		/**
		 * Whether the data consumer must start in paused mode. Default false.
		 */
		paused?: boolean;

		/**
		 * Subchannels this data consumer initially subscribes to.
		 * Only used in case this data consumer receives messages from a local data
		 * producer that specifies subchannel(s) when calling send().
		 */
		subchannels?: number[];

		/**
		 * Custom application data.
		 */
		appData?: DataConsumerAppData;
	};

export type DataConsumerStat = {
	type: string;
	timestamp: number;
	label: string;
	protocol: string;
	messagesSent: number;
	bytesSent: number;
	bufferedAmount: number;
};

/**
 * DataConsumer type.
 */
export type DataConsumerType = 'sctp' | 'direct';

export type DataConsumerEvents = {
	transportclose: [];
	dataproducerclose: [];
	dataproducerpause: [];
	dataproducerresume: [];
	message: [Buffer, number];
	sctpsendbufferfull: [];
	bufferedamountlow: [number];
	listenererror: [string, Error];
	// Private events.
	'@close': [];
	'@dataproducerclose': [];
};

export type DataConsumerObserver =
	EnhancedEventEmitter<DataConsumerObserverEvents>;

export type DataConsumerObserverEvents = {
	close: [];
	pause: [];
	resume: [];
};

type DataConsumerDump = DataConsumerData & {
	id: string;
	paused: boolean;
	dataProducerPaused: boolean;
	subchannels: number[];
};

type DataConsumerInternal = TransportInternal & {
	dataConsumerId: string;
};

type DataConsumerData = {
	dataProducerId: string;
	type: DataConsumerType;
	sctpStreamParameters?: SctpStreamParameters;
	label: string;
	protocol: string;
	bufferedAmountLowThreshold: number;
};

const logger = new Logger('DataConsumer');

export class DataConsumer<
	DataConsumerAppData extends AppData = AppData,
> extends EnhancedEventEmitter<DataConsumerEvents> {
	// Internal data.
	readonly #internal: DataConsumerInternal;

	// DataConsumer data.
	readonly #data: DataConsumerData;

	// Channel instance.
	readonly #channel: Channel;

	// Closed flag.
	#closed = false;

	// Paused flag.
	#paused = false;

	// Associated DataProducer paused flag.
	#dataProducerPaused = false;

	// Subchannels subscribed to.
	#subchannels: number[];

	// Custom app data.
	#appData: DataConsumerAppData;

	// Observer instance.
	readonly #observer: DataConsumerObserver =
		new EnhancedEventEmitter<DataConsumerObserverEvents>();

	/**
	 * @private
	 */
	constructor({
		internal,
		data,
		channel,
		paused,
		dataProducerPaused,
		subchannels,
		appData,
	}: {
		internal: DataConsumerInternal;
		data: DataConsumerData;
		channel: Channel;
		paused: boolean;
		dataProducerPaused: boolean;
		subchannels: number[];
		appData?: DataConsumerAppData;
	}) {
		super();

		logger.debug('constructor()');

		this.#internal = internal;
		this.#data = data;
		this.#channel = channel;
		this.#paused = paused;
		this.#dataProducerPaused = dataProducerPaused;
		this.#subchannels = subchannels;
		this.#appData = appData ?? ({} as DataConsumerAppData);

		this.handleWorkerNotifications();
	}

	/**
	 * DataConsumer id.
	 */
	get id(): string {
		return this.#internal.dataConsumerId;
	}

	/**
	 * Associated DataProducer id.
	 */
	get dataProducerId(): string {
		return this.#data.dataProducerId;
	}

	/**
	 * Whether the DataConsumer is closed.
	 */
	get closed(): boolean {
		return this.#closed;
	}

	/**
	 * DataConsumer type.
	 */
	get type(): DataConsumerType {
		return this.#data.type;
	}

	/**
	 * SCTP stream parameters.
	 */
	get sctpStreamParameters(): SctpStreamParameters | undefined {
		return this.#data.sctpStreamParameters;
	}

	/**
	 * DataChannel label.
	 */
	get label(): string {
		return this.#data.label;
	}

	/**
	 * DataChannel protocol.
	 */
	get protocol(): string {
		return this.#data.protocol;
	}

	/**
	 * Whether the DataConsumer is paused.
	 */
	get paused(): boolean {
		return this.#paused;
	}

	/**
	 * Whether the associate DataProducer is paused.
	 */
	get dataProducerPaused(): boolean {
		return this.#dataProducerPaused;
	}

	/**
	 * Get current subchannels this data consumer is subscribed to.
	 */
	get subchannels(): number[] {
		return Array.from(this.#subchannels);
	}

	/**
	 * App custom data.
	 */
	get appData(): DataConsumerAppData {
		return this.#appData;
	}

	/**
	 * App custom data setter.
	 */
	set appData(appData: DataConsumerAppData) {
		this.#appData = appData;
	}

	/**
	 * Observer.
	 */
	get observer(): DataConsumerObserver {
		return this.#observer;
	}

	/**
	 * Close the DataConsumer.
	 */
	close(): void {
		if (this.#closed) {
			return;
		}

		logger.debug('close()');

		this.#closed = true;

		// Remove notification subscriptions.
		this.#channel.removeAllListeners(this.#internal.dataConsumerId);

		/* Build Request. */
		const requestOffset = new FbsTransport.CloseDataConsumerRequestT(
			this.#internal.dataConsumerId
		).pack(this.#channel.bufferBuilder);

		this.#channel
			.request(
				FbsRequest.Method.TRANSPORT_CLOSE_DATACONSUMER,
				FbsRequest.Body.Transport_CloseDataConsumerRequest,
				requestOffset,
				this.#internal.transportId
			)
			.catch(() => {});

		this.emit('@close');

		// Emit observer event.
		this.#observer.safeEmit('close');
	}

	/**
	 * Transport was closed.
	 *
	 * @private
	 */
	transportClosed(): void {
		if (this.#closed) {
			return;
		}

		logger.debug('transportClosed()');

		this.#closed = true;

		// Remove notification subscriptions.
		this.#channel.removeAllListeners(this.#internal.dataConsumerId);

		this.safeEmit('transportclose');

		// Emit observer event.
		this.#observer.safeEmit('close');
	}

	/**
	 * Dump DataConsumer.
	 */
	async dump(): Promise<DataConsumerDump> {
		logger.debug('dump()');

		const response = await this.#channel.request(
			FbsRequest.Method.DATACONSUMER_DUMP,
			undefined,
			undefined,
			this.#internal.dataConsumerId
		);

		/* Decode Response. */
		const dumpResponse = new FbsDataConsumer.DumpResponse();

		response.body(dumpResponse);

		return parseDataConsumerDumpResponse(dumpResponse);
	}

	/**
	 * Get DataConsumer stats.
	 */
	async getStats(): Promise<DataConsumerStat[]> {
		logger.debug('getStats()');

		const response = await this.#channel.request(
			FbsRequest.Method.DATACONSUMER_GET_STATS,
			undefined,
			undefined,
			this.#internal.dataConsumerId
		);

		/* Decode Response. */
		const data = new FbsDataConsumer.GetStatsResponse();

		response.body(data);

		return [parseDataConsumerStats(data)];
	}

	/**
	 * Pause the DataConsumer.
	 */
	async pause(): Promise<void> {
		logger.debug('pause()');

		await this.#channel.request(
			FbsRequest.Method.DATACONSUMER_PAUSE,
			undefined,
			undefined,
			this.#internal.dataConsumerId
		);

		const wasPaused = this.#paused;

		this.#paused = true;

		// Emit observer event.
		if (!wasPaused && !this.#dataProducerPaused) {
			this.#observer.safeEmit('pause');
		}
	}

	/**
	 * Resume the DataConsumer.
	 */
	async resume(): Promise<void> {
		logger.debug('resume()');

		await this.#channel.request(
			FbsRequest.Method.DATACONSUMER_RESUME,
			undefined,
			undefined,
			this.#internal.dataConsumerId
		);

		const wasPaused = this.#paused;

		this.#paused = false;

		// Emit observer event.
		if (wasPaused && !this.#dataProducerPaused) {
			this.#observer.safeEmit('resume');
		}
	}

	/**
	 * Set buffered amount low threshold.
	 */
	async setBufferedAmountLowThreshold(threshold: number): Promise<void> {
		logger.debug(`setBufferedAmountLowThreshold() [threshold:${threshold}]`);

		/* Build Request. */
		const requestOffset =
			FbsDataConsumer.SetBufferedAmountLowThresholdRequest.createSetBufferedAmountLowThresholdRequest(
				this.#channel.bufferBuilder,
				threshold
			);

		await this.#channel.request(
			FbsRequest.Method.DATACONSUMER_SET_BUFFERED_AMOUNT_LOW_THRESHOLD,
			FbsRequest.Body.DataConsumer_SetBufferedAmountLowThresholdRequest,
			requestOffset,
			this.#internal.dataConsumerId
		);
	}

	/**
	 * Send data.
	 */
	async send(message: string | Buffer, ppid?: number): Promise<void> {
		if (typeof message !== 'string' && !Buffer.isBuffer(message)) {
			throw new TypeError('message must be a string or a Buffer');
		}

		/*
		 * +-------------------------------+----------+
		 * | Value                         | SCTP     |
		 * |                               | PPID     |
		 * +-------------------------------+----------+
		 * | WebRTC String                 | 51       |
		 * | WebRTC Binary Partial         | 52       |
		 * | (Deprecated)                  |          |
		 * | WebRTC Binary                 | 53       |
		 * | WebRTC String Partial         | 54       |
		 * | (Deprecated)                  |          |
		 * | WebRTC String Empty           | 56       |
		 * | WebRTC Binary Empty           | 57       |
		 * +-------------------------------+----------+
		 */

		if (typeof ppid !== 'number') {
			ppid =
				typeof message === 'string'
					? message.length > 0
						? 51
						: 56
					: message.length > 0
						? 53
						: 57;
		}

		// Ensure we honor PPIDs.
		if (ppid === 56) {
			message = ' ';
		} else if (ppid === 57) {
			message = Buffer.alloc(1);
		}

		const builder = this.#channel.bufferBuilder;

		let dataOffset = 0;

		if (typeof message === 'string') {
			message = Buffer.from(message);
		}

		dataOffset = FbsDataConsumer.SendRequest.createDataVector(builder, message);

		const requestOffset = FbsDataConsumer.SendRequest.createSendRequest(
			builder,
			ppid,
			dataOffset
		);

		await this.#channel.request(
			FbsRequest.Method.DATACONSUMER_SEND,
			FbsRequest.Body.DataConsumer_SendRequest,
			requestOffset,
			this.#internal.dataConsumerId
		);
	}

	/**
	 * Get buffered amount size.
	 */
	async getBufferedAmount(): Promise<number> {
		logger.debug('getBufferedAmount()');

		const response = await this.#channel.request(
			FbsRequest.Method.DATACONSUMER_GET_BUFFERED_AMOUNT,
			undefined,
			undefined,
			this.#internal.dataConsumerId
		);

		const data = new FbsDataConsumer.GetBufferedAmountResponse();

		response.body(data);

		return data.bufferedAmount();
	}

	/**
	 * Set subchannels.
	 */
	async setSubchannels(subchannels: number[]): Promise<void> {
		logger.debug('setSubchannels()');

		/* Build Request. */
		const requestOffset = new FbsDataConsumer.SetSubchannelsRequestT(
			subchannels
		).pack(this.#channel.bufferBuilder);

		const response = await this.#channel.request(
			FbsRequest.Method.DATACONSUMER_SET_SUBCHANNELS,
			FbsRequest.Body.DataConsumer_SetSubchannelsRequest,
			requestOffset,
			this.#internal.dataConsumerId
		);

		/* Decode Response. */
		const data = new FbsDataConsumer.SetSubchannelsResponse();

		response.body(data);

		// Update subchannels.
		this.#subchannels = utils.parseVector(data, 'subchannels');
	}

	/**
	 * Add a subchannel.
	 */
	async addSubchannel(subchannel: number): Promise<void> {
		logger.debug('addSubchannel()');

		/* Build Request. */
		const requestOffset =
			FbsDataConsumer.AddSubchannelRequest.createAddSubchannelRequest(
				this.#channel.bufferBuilder,
				subchannel
			);

		const response = await this.#channel.request(
			FbsRequest.Method.DATACONSUMER_ADD_SUBCHANNEL,
			FbsRequest.Body.DataConsumer_AddSubchannelRequest,
			requestOffset,
			this.#internal.dataConsumerId
		);

		/* Decode Response. */
		const data = new FbsDataConsumer.AddSubchannelResponse();

		response.body(data);

		// Update subchannels.
		this.#subchannels = utils.parseVector(data, 'subchannels');
	}

	/**
	 * Remove a subchannel.
	 */
	async removeSubchannel(subchannel: number): Promise<void> {
		logger.debug('removeSubchannel()');

		/* Build Request. */
		const requestOffset =
			FbsDataConsumer.RemoveSubchannelRequest.createRemoveSubchannelRequest(
				this.#channel.bufferBuilder,
				subchannel
			);

		const response = await this.#channel.request(
			FbsRequest.Method.DATACONSUMER_REMOVE_SUBCHANNEL,
			FbsRequest.Body.DataConsumer_RemoveSubchannelRequest,
			requestOffset,
			this.#internal.dataConsumerId
		);

		/* Decode Response. */
		const data = new FbsDataConsumer.RemoveSubchannelResponse();

		response.body(data);

		// Update subchannels.
		this.#subchannels = utils.parseVector(data, 'subchannels');
	}

	private handleWorkerNotifications(): void {
		this.#channel.on(
			this.#internal.dataConsumerId,
			(event: Event, data?: Notification) => {
				switch (event) {
					case Event.DATACONSUMER_DATAPRODUCER_CLOSE: {
						if (this.#closed) {
							break;
						}

						this.#closed = true;

						// Remove notification subscriptions.
						this.#channel.removeAllListeners(this.#internal.dataConsumerId);

						this.emit('@dataproducerclose');
						this.safeEmit('dataproducerclose');

						// Emit observer event.
						this.#observer.safeEmit('close');

						break;
					}

					case Event.DATACONSUMER_DATAPRODUCER_PAUSE: {
						if (this.#dataProducerPaused) {
							break;
						}

						this.#dataProducerPaused = true;

						this.safeEmit('dataproducerpause');

						// Emit observer event.
						if (!this.#paused) {
							this.#observer.safeEmit('pause');
						}

						break;
					}

					case Event.DATACONSUMER_DATAPRODUCER_RESUME: {
						if (!this.#dataProducerPaused) {
							break;
						}

						this.#dataProducerPaused = false;

						this.safeEmit('dataproducerresume');

						// Emit observer event.
						if (!this.#paused) {
							this.#observer.safeEmit('resume');
						}

						break;
					}

					case Event.DATACONSUMER_SCTP_SENDBUFFER_FULL: {
						this.safeEmit('sctpsendbufferfull');

						break;
					}

					case Event.DATACONSUMER_BUFFERED_AMOUNT_LOW: {
						const notification =
							new FbsDataConsumer.BufferedAmountLowNotification();

						data!.body(notification);

						const bufferedAmount = notification.bufferedAmount();

						this.safeEmit('bufferedamountlow', bufferedAmount);

						break;
					}

					case Event.DATACONSUMER_MESSAGE: {
						if (this.#closed) {
							break;
						}

						const notification = new FbsDataConsumer.MessageNotification();

						data!.body(notification);

						this.safeEmit(
							'message',
							Buffer.from(notification.dataArray()!),
							notification.ppid()
						);

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

export function dataConsumerTypeToFbs(
	type: DataConsumerType
): FbsDataProducer.Type {
	switch (type) {
		case 'sctp': {
			return FbsDataProducer.Type.SCTP;
		}

		case 'direct': {
			return FbsDataProducer.Type.DIRECT;
		}

		default: {
			throw new TypeError('invalid DataConsumerType: ${type}');
		}
	}
}

export function dataConsumerTypeFromFbs(
	type: FbsDataProducer.Type
): DataConsumerType {
	switch (type) {
		case FbsDataProducer.Type.SCTP: {
			return 'sctp';
		}

		case FbsDataProducer.Type.DIRECT: {
			return 'direct';
		}
	}
}

export function parseDataConsumerDumpResponse(
	data: FbsDataConsumer.DumpResponse
): DataConsumerDump {
	return {
		id: data.id()!,
		dataProducerId: data.dataProducerId()!,
		type: dataConsumerTypeFromFbs(data.type()),
		sctpStreamParameters:
			data.sctpStreamParameters() !== null
				? parseSctpStreamParameters(data.sctpStreamParameters()!)
				: undefined,
		label: data.label()!,
		protocol: data.protocol()!,
		bufferedAmountLowThreshold: data.bufferedAmountLowThreshold(),
		paused: data.paused(),
		dataProducerPaused: data.dataProducerPaused(),
		subchannels: utils.parseVector(data, 'subchannels'),
	};
}

function parseDataConsumerStats(
	binary: FbsDataConsumer.GetStatsResponse
): DataConsumerStat {
	return {
		type: 'data-consumer',
		timestamp: Number(binary.timestamp()),
		label: binary.label()!,
		protocol: binary.protocol()!,
		messagesSent: Number(binary.messagesSent()),
		bytesSent: Number(binary.bytesSent()),
		bufferedAmount: binary.bufferedAmount(),
	};
}
