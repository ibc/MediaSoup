import { EnhancedEventEmitter } from './enhancedEvents';
import { SctpStreamParameters } from './SctpParameters';
import { AppData } from './types';

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
	listenererror: [string, Error];
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

export interface DataProducerInterface<
	DataProducerAppData extends AppData = AppData,
> extends EnhancedEventEmitter<DataProducerEvents> {
	get id(): string;

	get closed(): boolean;

	get type(): DataProducerType;

	get sctpStreamParameters(): SctpStreamParameters | undefined;

	get label(): string;

	get protocol(): string;

	get paused(): boolean;

	get appData(): DataProducerAppData;

	set appData(appData: DataProducerAppData);

	get observer(): DataProducerObserver;

	close(): void;

	/**
	 * Transport was closed.
	 *
	 * @private
	 */
	transportClosed(): void;

	dump(): Promise<DataProducerDump>;

	getStats(): Promise<DataProducerStat[]>;

	pause(): Promise<void>;

	resume(): Promise<void>;

	send(
		message: string | Buffer,
		ppid?: number,
		subchannels?: number[],
		requiredSubchannel?: number
	): void;
}
