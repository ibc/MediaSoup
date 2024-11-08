import { Logger } from './Logger';
import { EnhancedEventEmitter } from './enhancedEvents';
import {
	DataProducerInterface,
	DataProducerType,
	DataProducerDump,
	DataProducerStat,
	DataProducerEvents,
	DataProducerObserver,
	DataProducerObserverEvents,
} from './DataProducerTypes';
import { Channel } from './Channel';
import { TransportInternal } from './Transport';
import {
	SctpStreamParameters,
	parseSctpStreamParameters,
} from './SctpParameters';
import { AppData } from './types';
import * as FbsTransport from './fbs/transport';
import * as FbsNotification from './fbs/notification';
import * as FbsRequest from './fbs/request';
import * as FbsDataProducer from './fbs/data-producer';

type DataProducerInternal = TransportInternal & {
	dataProducerId: string;
};

type DataProducerData = {
	type: DataProducerType;
	sctpStreamParameters?: SctpStreamParameters;
	label: string;
	protocol: string;
};

const logger = new Logger('DataProducer');

export class DataProducerImpl<DataProducerAppData extends AppData = AppData>
	extends EnhancedEventEmitter<DataProducerEvents>
	implements DataProducerInterface
{
	// Internal data.
	readonly #internal: DataProducerInternal;

	// DataProducer data.
	readonly #data: DataProducerData;

	// Channel instance.
	readonly #channel: Channel;

	// Closed flag.
	#closed = false;

	// Paused flag.
	#paused = false;

	// Custom app data.
	#appData: DataProducerAppData;

	// Observer instance.
	readonly #observer: DataProducerObserver =
		new EnhancedEventEmitter<DataProducerObserverEvents>();

	constructor({
		internal,
		data,
		channel,
		paused,
		appData,
	}: {
		internal: DataProducerInternal;
		data: DataProducerData;
		channel: Channel;
		paused: boolean;
		appData?: DataProducerAppData;
	}) {
		super();

		logger.debug('constructor()');

		this.#internal = internal;
		this.#data = data;
		this.#channel = channel;
		this.#paused = paused;
		this.#appData = appData ?? ({} as DataProducerAppData);

		this.handleWorkerNotifications();
	}

	get id(): string {
		return this.#internal.dataProducerId;
	}

	get closed(): boolean {
		return this.#closed;
	}

	get type(): DataProducerType {
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

	get appData(): DataProducerAppData {
		return this.#appData;
	}

	set appData(appData: DataProducerAppData) {
		this.#appData = appData;
	}

	get observer(): DataProducerObserver {
		return this.#observer;
	}

	close(): void {
		if (this.#closed) {
			return;
		}

		logger.debug('close()');

		this.#closed = true;

		// Remove notification subscriptions.
		this.#channel.removeAllListeners(this.#internal.dataProducerId);

		/* Build Request. */
		const requestOffset = new FbsTransport.CloseDataProducerRequestT(
			this.#internal.dataProducerId
		).pack(this.#channel.bufferBuilder);

		this.#channel
			.request(
				FbsRequest.Method.TRANSPORT_CLOSE_DATAPRODUCER,
				FbsRequest.Body.Transport_CloseDataProducerRequest,
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
		this.#channel.removeAllListeners(this.#internal.dataProducerId);

		this.safeEmit('transportclose');

		// Emit observer event.
		this.#observer.safeEmit('close');
	}

	async dump(): Promise<DataProducerDump> {
		logger.debug('dump()');

		const response = await this.#channel.request(
			FbsRequest.Method.DATAPRODUCER_DUMP,
			undefined,
			undefined,
			this.#internal.dataProducerId
		);

		/* Decode Response. */
		const produceResponse = new FbsDataProducer.DumpResponse();

		response.body(produceResponse);

		return parseDataProducerDumpResponse(produceResponse);
	}

	async getStats(): Promise<DataProducerStat[]> {
		logger.debug('getStats()');

		const response = await this.#channel.request(
			FbsRequest.Method.DATAPRODUCER_GET_STATS,
			undefined,
			undefined,
			this.#internal.dataProducerId
		);

		/* Decode Response. */
		const data = new FbsDataProducer.GetStatsResponse();

		response.body(data);

		return [parseDataProducerStats(data)];
	}

	async pause(): Promise<void> {
		logger.debug('pause()');

		await this.#channel.request(
			FbsRequest.Method.DATAPRODUCER_PAUSE,
			undefined,
			undefined,
			this.#internal.dataProducerId
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
			FbsRequest.Method.DATAPRODUCER_RESUME,
			undefined,
			undefined,
			this.#internal.dataProducerId
		);

		const wasPaused = this.#paused;

		this.#paused = false;

		// Emit observer event.
		if (wasPaused) {
			this.#observer.safeEmit('resume');
		}
	}

	send(
		message: string | Buffer,
		ppid?: number,
		subchannels?: number[],
		requiredSubchannel?: number
	): void {
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

		const subchannelsOffset =
			FbsDataProducer.SendNotification.createSubchannelsVector(
				builder,
				subchannels ?? []
			);

		if (typeof message === 'string') {
			message = Buffer.from(message);
		}

		dataOffset = FbsDataProducer.SendNotification.createDataVector(
			builder,
			message
		);

		const notificationOffset =
			FbsDataProducer.SendNotification.createSendNotification(
				builder,
				ppid,
				dataOffset,
				subchannelsOffset,
				requiredSubchannel ?? null
			);

		this.#channel.notify(
			FbsNotification.Event.DATAPRODUCER_SEND,
			FbsNotification.Body.DataProducer_SendNotification,
			notificationOffset,
			this.#internal.dataProducerId
		);
	}

	private handleWorkerNotifications(): void {
		// No need to subscribe to any event.
	}
}

export function dataProducerTypeToFbs(
	type: DataProducerType
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

export function dataProducerTypeFromFbs(
	type: FbsDataProducer.Type
): DataProducerType {
	switch (type) {
		case FbsDataProducer.Type.SCTP: {
			return 'sctp';
		}

		case FbsDataProducer.Type.DIRECT: {
			return 'direct';
		}
	}
}

export function parseDataProducerDumpResponse(
	data: FbsDataProducer.DumpResponse
): DataProducerDump {
	return {
		id: data.id()!,
		type: dataProducerTypeFromFbs(data.type()),
		sctpStreamParameters:
			data.sctpStreamParameters() !== null
				? parseSctpStreamParameters(data.sctpStreamParameters()!)
				: undefined,
		label: data.label()!,
		protocol: data.protocol()!,
		paused: data.paused(),
	};
}

function parseDataProducerStats(
	binary: FbsDataProducer.GetStatsResponse
): DataProducerStat {
	return {
		type: 'data-producer',
		timestamp: Number(binary.timestamp()),
		label: binary.label()!,
		protocol: binary.protocol()!,
		messagesReceived: Number(binary.messagesReceived()),
		bytesReceived: Number(binary.bytesReceived()),
	};
}
