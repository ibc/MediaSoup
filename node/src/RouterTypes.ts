import { EnhancedEventEmitter } from './enhancedEvents';
import {
	TransportInterface,
	TransportListenInfo,
	TransportListenIp,
} from './TransportTypes';
import {
	WebRtcTransportInterface,
	WebRtcTransportOptions,
} from './WebRtcTransportTypes';
import {
	PlainTransportInterface,
	PlainTransportOptions,
} from './PlainTransportTypes';
import {
	PipeTransportInterface,
	PipeTransportOptions,
} from './PipeTransportTypes';
import {
	DirectTransportInterface,
	DirectTransportOptions,
} from './DirectTransportTypes';
import { ProducerInterface } from './ProducerTypes';
import { ConsumerInterface } from './ConsumerTypes';
import { DataProducerInterface } from './DataProducerTypes';
import { DataConsumerInterface } from './DataConsumerTypes';
import { RtpObserverInterface } from './RtpObserverTypes';
import {
	ActiveSpeakerObserverInterface,
	ActiveSpeakerObserverOptions,
} from './ActiveSpeakerObserverTypes';
import {
	AudioLevelObserverInterface,
	AudioLevelObserverOptions,
} from './AudioLevelObserverTypes';
import { RtpCapabilities, RtpCodecCapability } from './RtpParameters';
import { NumSctpStreams } from './SctpParameters';
import { Either, AppData } from './types';

export type RouterOptions<RouterAppData extends AppData = AppData> = {
	/**
	 * Router media codecs.
	 */
	mediaCodecs?: RtpCodecCapability[];

	/**
	 * Custom application data.
	 */
	appData?: RouterAppData;
};

export type PipeToRouterOptions = {
	/**
	 * The id of the Producer to consume.
	 */
	producerId?: string;

	/**
	 * The id of the DataProducer to consume.
	 */
	dataProducerId?: string;

	/**
	 * Target Router instance.
	 */
	router: RouterInterface;

	/**
	 * Create a SCTP association. Default true.
	 */
	enableSctp?: boolean;

	/**
	 * SCTP streams number.
	 */
	numSctpStreams?: NumSctpStreams;

	/**
	 * Enable RTX and NACK for RTP retransmission.
	 */
	enableRtx?: boolean;

	/**
	 * Enable SRTP.
	 */
	enableSrtp?: boolean;
} & PipeToRouterListen;

type PipeToRouterListen = Either<PipeToRouterListenInfo, PipeToRouterListenIp>;

type PipeToRouterListenInfo = {
	listenInfo: TransportListenInfo;
};

type PipeToRouterListenIp = {
	/**
	 * IP used in the PipeTransport pair. Default '127.0.0.1'.
	 */
	listenIp?: TransportListenIp | string;
};

export type PipeToRouterResult = {
	/**
	 * The Consumer created in the current Router.
	 */
	pipeConsumer?: ConsumerInterface;

	/**
	 * The Producer created in the target Router.
	 */
	pipeProducer?: ProducerInterface;

	/**
	 * The DataConsumer created in the current Router.
	 */
	pipeDataConsumer?: DataConsumerInterface;

	/**
	 * The DataProducer created in the target Router.
	 */
	pipeDataProducer?: DataProducerInterface;
};

export type PipeTransportPair = {
	[key: string]: PipeTransportInterface;
};

export type RouterDump = {
	/**
	 * The Router id.
	 */
	id: string;
	/**
	 * Id of Transports.
	 */
	transportIds: string[];
	/**
	 * Id of RtpObservers.
	 */
	rtpObserverIds: string[];
	/**
	 * Array of Producer id and its respective Consumer ids.
	 */
	mapProducerIdConsumerIds: { key: string; values: string[] }[];
	/**
	 * Array of Consumer id and its Producer id.
	 */
	mapConsumerIdProducerId: { key: string; value: string }[];
	/**
	 * Array of Producer id and its respective Observer ids.
	 */
	mapProducerIdObserverIds: { key: string; values: string[] }[];
	/**
	 * Array of Producer id and its respective DataConsumer ids.
	 */
	mapDataProducerIdDataConsumerIds: { key: string; values: string[] }[];
	/**
	 * Array of DataConsumer id and its DataProducer id.
	 */
	mapDataConsumerIdDataProducerId: { key: string; value: string }[];
};

export type RouterEvents = {
	workerclose: [];
	listenererror: [string, Error];
	// Private events.
	'@close': [];
};

export type RouterObserver = EnhancedEventEmitter<RouterObserverEvents>;

export type RouterObserverEvents = {
	close: [];
	newtransport: [TransportInterface];
	newrtpobserver: [RtpObserverInterface];
};

export interface RouterInterface<RouterAppData extends AppData = AppData>
	extends EnhancedEventEmitter<RouterEvents> {
	/**
	 * Router id.
	 */
	get id(): string;

	/**
	 * Whether the Router is closed.
	 */
	get closed(): boolean;

	/**
	 * RTP capabilities of the Router.
	 */
	get rtpCapabilities(): RtpCapabilities;

	/**
	 * App custom data.
	 */
	get appData(): RouterAppData;

	/**
	 * App custom data setter.
	 */
	set appData(appData: RouterAppData);

	/**
	 * Observer.
	 */
	get observer(): RouterObserver;

	/**
	 * Close the Router.
	 */
	close(): void;

	/**
	 * Worker was closed.
	 *
	 * @private
	 */
	workerClosed(): void;

	/**
	 * Dump Router.
	 */
	dump(): Promise<RouterDump>;

	/**
	 * Create a WebRtcTransport.
	 */
	createWebRtcTransport<WebRtcTransportAppData extends AppData = AppData>(
		options: WebRtcTransportOptions<WebRtcTransportAppData>
	): Promise<WebRtcTransportInterface<WebRtcTransportAppData>>;

	/**
	 * Create a PlainTransport.
	 */
	createPlainTransport<PlainTransportAppData extends AppData = AppData>(
		options: PlainTransportOptions<PlainTransportAppData>
	): Promise<PlainTransportInterface<PlainTransportAppData>>;

	/**
	 * Create a PipeTransport.
	 */
	createPipeTransport<PipeTransportAppData extends AppData = AppData>(
		options: PipeTransportOptions<PipeTransportAppData>
	): Promise<PipeTransportInterface<PipeTransportAppData>>;

	/**
	 * Create a DirectTransport.
	 */
	createDirectTransport<DirectTransportAppData extends AppData = AppData>(
		options?: DirectTransportOptions<DirectTransportAppData>
	): Promise<DirectTransportInterface<DirectTransportAppData>>;

	/**
	 * Pipes the given Producer or DataProducer into another Router in same host.
	 */
	pipeToRouter(options: PipeToRouterOptions): Promise<PipeToRouterResult>;

	/**
	 * @private
	 */
	addPipeTransportPair(
		pipeTransportPairKey: string,
		pipeTransportPairPromise: Promise<PipeTransportPair>
	): void;

	/**
	 * Create an ActiveSpeakerObserver
	 */
	createActiveSpeakerObserver<
		ActiveSpeakerObserverAppData extends AppData = AppData,
	>(
		options?: ActiveSpeakerObserverOptions<ActiveSpeakerObserverAppData>
	): Promise<ActiveSpeakerObserverInterface<ActiveSpeakerObserverAppData>>;

	/**
	 * Create an AudioLevelObserver.
	 */
	createAudioLevelObserver<AudioLevelObserverAppData extends AppData = AppData>(
		options?: AudioLevelObserverOptions<AudioLevelObserverAppData>
	): Promise<AudioLevelObserverInterface<AudioLevelObserverAppData>>;

	/**
	 * Check whether the given RTP capabilities can consume the given Producer.
	 */
	canConsume({
		producerId,
		rtpCapabilities,
	}: {
		producerId: string;
		rtpCapabilities: RtpCapabilities;
	}): boolean;
}
