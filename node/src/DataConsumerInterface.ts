import { EnhancedEventEmitter } from './enhancedEvents';
import { SctpStreamParameters } from './SctpParameters';
import { AppData } from './types';

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

/**
 * DataConsumer type.
 */
export type DataConsumerType = 'sctp' | 'direct';

export type DataConsumerDump = {
	id: string;
	paused: boolean;
	dataProducerPaused: boolean;
	subchannels: number[];
	dataProducerId: string;
	type: DataConsumerType;
	sctpStreamParameters?: SctpStreamParameters;
	label: string;
	protocol: string;
	bufferedAmountLowThreshold: number;
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

export interface DataConsumerInterface<
	DataConsumerAppData extends AppData = AppData,
> extends EnhancedEventEmitter<DataConsumerEvents> {
	get id(): string;

	get dataProducerId(): string;

	get closed(): boolean;

	get type(): DataConsumerType;

	get sctpStreamParameters(): SctpStreamParameters | undefined;

	get label(): string;

	get protocol(): string;

	get paused(): boolean;

	get dataProducerPaused(): boolean;

	get subchannels(): number[];

	get appData(): DataConsumerAppData;

	set appData(appData: DataConsumerAppData);

	get observer(): DataConsumerObserver;

	close(): void;

	/**
	 * Transport was closed.
	 *
	 * @private
	 */
	transportClosed(): void;

	dump(): Promise<DataConsumerDump>;

	getStats(): Promise<DataConsumerStat[]>;

	pause(): Promise<void>;

	resume(): Promise<void>;

	setBufferedAmountLowThreshold(threshold: number): Promise<void>;

	getBufferedAmount(): Promise<number>;

	send(message: string | Buffer, ppid?: number): Promise<void>;

	setSubchannels(subchannels: number[]): Promise<void>;

	addSubchannel(subchannel: number): Promise<void>;

	removeSubchannel(subchannel: number): Promise<void>;
}
