import type { EnhancedEventEmitter } from './enhancedEvents';
import type { SctpStreamParameters } from './sctpParametersTypes';
import type { AppData } from './types';

export type DataProducerOptions<DataProducerAppData extends AppData = AppData> =
	{
		/**
		 * DataProducer id (just for Router.pipeToRouter() method).
		 */
		id?: string;

		/**
		 * SCTP parameters defining how the endpoint is sending the data.
		 * Just if messages are sent over SCTP.
		 */
		sctpStreamParameters?: SctpStreamParameters;

		/**
		 * A label which can be used to distinguish this DataChannel from others.
		 */
		label?: string;

		/**
		 * Name of the sub-protocol used by this DataChannel.
		 */
		protocol?: string;

		/**
		 * Whether the data producer must start in paused mode. Default false.
		 */
		paused?: boolean;

		/**
		 * Custom application data.
		 */
		appData?: DataProducerAppData;
	};

/**
 * DataProducer type.
 */
export type DataProducerType = 'sctp' | 'direct';

export type DataProducerDump = {
	id: string;
	paused: boolean;
	type: DataProducerType;
	sctpStreamParameters?: SctpStreamParameters;
	label: string;
	protocol: string;
};

export type DataProducerStat = {
	type: string;
	timestamp: number;
	label: string;
	protocol: string;
	messagesReceived: number;
	bytesReceived: number;
};

export type DataProducerEvents = {
	transportclose: [];
	// Private events.
	'@close': [];
};

export type DataProducerObserver =
	EnhancedEventEmitter<DataProducerObserverEvents>;

export type DataProducerObserverEvents = {
	close: [];
	pause: [];
	resume: [];
};

export interface DataProducer<DataProducerAppData extends AppData = AppData>
	extends EnhancedEventEmitter<DataProducerEvents> {
	/**
	 * DataProducer id.
	 */
	get id(): string;

	/**
	 * Whether the DataProducer is closed.
	 */
	get closed(): boolean;

	/**
	 * DataProducer type.
	 */
	get type(): DataProducerType;

	/**
	 * SCTP stream parameters.
	 */
	get sctpStreamParameters(): SctpStreamParameters | undefined;

	/**
	 * DataChannel label.
	 */
	get label(): string;

	/**
	 * DataChannel protocol.
	 */
	get protocol(): string;

	/**
	 * Whether the DataProducer is paused.
	 */
	get paused(): boolean;

	/**
	 * App custom data.
	 */
	get appData(): DataProducerAppData;

	/**
	 * App custom data setter.
	 */
	set appData(appData: DataProducerAppData);

	/**
	 * Observer.
	 */
	get observer(): DataProducerObserver;

	/**
	 * Close the DataProducer.
	 */
	close(): void;

	/**
	 * Transport was closed.
	 *
	 * @private
	 */
	transportClosed(): void;

	/**
	 * Dump DataProducer.
	 */
	dump(): Promise<DataProducerDump>;

	/**
	 * Get DataProducer stats.
	 */
	getStats(): Promise<DataProducerStat[]>;

	/**
	 * Pause the DataProducer.
	 */
	pause(): Promise<void>;

	/**
	 * Resume the DataProducer.
	 */
	resume(): Promise<void>;

	/**
	 * Send data (just valid for DataProducers created on a DirectTransport).
	 */
	send(
		message: string | Buffer,
		ppid?: number,
		subchannels?: number[],
		requiredSubchannel?: number
	): void;
}
