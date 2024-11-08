import { EnhancedEventEmitter } from './enhancedEvents';
import {
	Transport,
	TransportListenInfo,
	TransportListenIp,
} from './TransportTypes';
import {
	WebRtcTransport,
	WebRtcTransportOptions,
} from './WebRtcTransportTypes';
import { PlainTransport, PlainTransportOptions } from './PlainTransportTypes';
import { PipeTransport, PipeTransportOptions } from './PipeTransportTypes';
import {
	DirectTransport,
	DirectTransportOptions,
} from './DirectTransportTypes';
import { Producer } from './ProducerTypes';
import { Consumer } from './ConsumerTypes';
import { DataProducer } from './DataProducerTypes';
import { DataConsumer } from './DataConsumerTypes';
import { RtpObserver } from './RtpObserverTypes';
import {
	ActiveSpeakerObserver,
	ActiveSpeakerObserverOptions,
} from './ActiveSpeakerObserverTypes';
import {
	AudioLevelObserver,
	AudioLevelObserverOptions,
} from './AudioLevelObserverTypes';
import { RtpCapabilities, RtpCodecCapability } from './rtpParametersTypes';
import { NumSctpStreams } from './sctpParametersTypes';
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
	router: Router;

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
	pipeConsumer?: Consumer;

	/**
	 * The Producer created in the target Router.
	 */
	pipeProducer?: Producer;

	/**
	 * The DataConsumer created in the current Router.
	 */
	pipeDataConsumer?: DataConsumer;

	/**
	 * The DataProducer created in the target Router.
	 */
	pipeDataProducer?: DataProducer;
};

export type PipeTransportPair = {
	[key: string]: PipeTransport;
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
	newtransport: [Transport];
	newrtpobserver: [RtpObserver];
};

export interface Router<RouterAppData extends AppData = AppData>
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
	): Promise<WebRtcTransport<WebRtcTransportAppData>>;

	/**
	 * Create a PlainTransport.
	 */
	createPlainTransport<PlainTransportAppData extends AppData = AppData>(
		options: PlainTransportOptions<PlainTransportAppData>
	): Promise<PlainTransport<PlainTransportAppData>>;

	/**
	 * Create a PipeTransport.
	 */
	createPipeTransport<PipeTransportAppData extends AppData = AppData>(
		options: PipeTransportOptions<PipeTransportAppData>
	): Promise<PipeTransport<PipeTransportAppData>>;

	/**
	 * Create a DirectTransport.
	 */
	createDirectTransport<DirectTransportAppData extends AppData = AppData>(
		options?: DirectTransportOptions<DirectTransportAppData>
	): Promise<DirectTransport<DirectTransportAppData>>;

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
	): Promise<ActiveSpeakerObserver<ActiveSpeakerObserverAppData>>;

	/**
	 * Create an AudioLevelObserver.
	 */
	createAudioLevelObserver<AudioLevelObserverAppData extends AppData = AppData>(
		options?: AudioLevelObserverOptions<AudioLevelObserverAppData>
	): Promise<AudioLevelObserver<AudioLevelObserverAppData>>;

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
