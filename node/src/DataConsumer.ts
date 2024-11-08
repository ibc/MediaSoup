import { Logger } from './Logger';
import { EnhancedEventEmitter } from './enhancedEvents';
import type {
	DataConsumer,
	DataConsumerType,
	DataConsumerDump,
	DataConsumerStat,
	DataConsumerEvents,
	DataConsumerObserver,
	DataConsumerObserverEvents,
} from './DataConsumerTypes';
import { Channel } from './Channel';
import type { TransportInternal } from './Transport';
import type { SctpStreamParameters } from './sctpParametersTypes';
import { parseSctpStreamParameters } from './sctpParametersFbsUtils';
import type { AppData } from './types';
import * as fbsUtils from './fbsUtils';
import { Event, Notification } from './fbs/notification';
import * as FbsTransport from './fbs/transport';
import * as FbsRequest from './fbs/request';
import * as FbsDataConsumer from './fbs/data-consumer';
import * as FbsDataProducer from './fbs/data-producer';

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

export class DataConsumerImpl<DataConsumerAppData extends AppData = AppData>
	extends EnhancedEventEmitter<DataConsumerEvents>
	implements DataConsumer
{
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

	get id(): string {
		return this.#internal.dataConsumerId;
	}

	get dataProducerId(): string {
		return this.#data.dataProducerId;
	}

	get closed(): boolean {
		return this.#closed;
	}

	get type(): DataConsumerType {
		return this.#data.type;
	}

	get sctpStreamParameters(): SctpStreamParameters | undefined {
		return this.#data.sctpStreamParameters;
	}

	get label(): string {
		return this.#data.label;
	}

	get protocol(): string {
		return this.#data.protocol;
	}

	get paused(): boolean {
		return this.#paused;
	}

	get dataProducerPaused(): boolean {
		return this.#dataProducerPaused;
	}

	get subchannels(): number[] {
		return Array.from(this.#subchannels);
	}

	get appData(): DataConsumerAppData {
		return this.#appData;
	}

	set appData(appData: DataConsumerAppData) {
		this.#appData = appData;
	}

	get observer(): DataConsumerObserver {
		return this.#observer;
	}

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
		this.#subchannels = fbsUtils.parseVector(data, 'subchannels');
	}

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
		this.#subchannels = fbsUtils.parseVector(data, 'subchannels');
	}

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
		this.#subchannels = fbsUtils.parseVector(data, 'subchannels');
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
		subchannels: fbsUtils.parseVector(data, 'subchannels'),
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
