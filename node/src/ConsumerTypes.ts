import type { EnhancedEventEmitter } from './enhancedEvents';
import type { ProducerStat } from './ProducerTypes';
import type {
	MediaKind,
	RtpCapabilities,
	RtpEncodingParameters,
	RtpParameters,
} from './rtpParametersTypes';
import type { RtpStreamSendStats } from './RtpStream';
import type { AppData } from './types';

export type ConsumerOptions<ConsumerAppData extends AppData = AppData> = {
	/**
	 * The id of the Producer to consume.
	 */
	producerId: string;

	/**
	 * RTP capabilities of the consuming endpoint.
	 */
	rtpCapabilities: RtpCapabilities;

	/**
	 * Whether the consumer must start in paused mode. Default false.
	 *
	 * When creating a video Consumer, it's recommended to set paused to true,
	 * then transmit the Consumer parameters to the consuming endpoint and, once
	 * the consuming endpoint has created its local side Consumer, unpause the
	 * server side Consumer using the resume() method. This is an optimization
	 * to make it possible for the consuming endpoint to render the video as far
	 * as possible. If the server side Consumer was created with paused: false,
	 * mediasoup will immediately request a key frame to the remote Producer and
	 * suych a key frame may reach the consuming endpoint even before it's ready
	 * to consume it, generating “black” video until the device requests a keyframe
	 * by itself.
	 */
	paused?: boolean;

	/**
	 * The MID for the Consumer. If not specified, a sequentially growing
	 * number will be assigned.
	 */
	mid?: string;

	/**
	 * Preferred spatial and temporal layer for simulcast or SVC media sources.
	 * If unset, the highest ones are selected.
	 */
	preferredLayers?: ConsumerLayers;

	/**
	 * Whether this Consumer should enable RTP retransmissions, storing sent RTP
	 * and processing the incoming RTCP NACK from the remote Consumer. If not set
	 * it's true by default for video codecs and false for audio codecs. If set
	 * to true, NACK will be enabled if both endpoints (mediasoup and the remote
	 * Consumer) support NACK for this codec. When it comes to audio codecs, just
	 * OPUS supports NACK.
	 */
	enableRtx?: boolean;

	/**
	 * Whether this Consumer should ignore DTX packets (only valid for Opus codec).
	 * If set, DTX packets are not forwarded to the remote Consumer.
	 */
	ignoreDtx?: boolean;

	/**
	 * Whether this Consumer should consume all RTP streams generated by the
	 * Producer.
	 */
	pipe?: boolean;

	/**
	 * Custom application data.
	 */
	appData?: ConsumerAppData;
};

/**
 * Consumer type.
 */
export type ConsumerType = 'simple' | 'simulcast' | 'svc' | 'pipe';

export type ConsumerScore = {
	/**
	 * The score of the RTP stream of the consumer.
	 */
	score: number;

	/**
	 * The score of the currently selected RTP stream of the producer.
	 */
	producerScore: number;

	/**
	 * The scores of all RTP streams in the producer ordered by encoding (just
	 * useful when the producer uses simulcast).
	 */
	producerScores: number[];
};

export type ConsumerLayers = {
	/**
	 * The spatial layer index (from 0 to N).
	 */
	spatialLayer: number;

	/**
	 * The temporal layer index (from 0 to N).
	 */
	temporalLayer?: number;
};

export type ConsumerDump =
	| SimpleConsumerDump
	| SimulcastConsumerDump
	| SvcConsumerDump
	| PipeConsumerDump;

export type SimpleConsumerDump = BaseConsumerDump & {
	type: string;
	rtpStream: RtpStreamDump;
};

export type SimulcastConsumerDump = BaseConsumerDump & {
	type: string;
	rtpStream: RtpStreamDump;
	preferredSpatialLayer: number;
	targetSpatialLayer: number;
	currentSpatialLayer: number;
	preferredTemporalLayer: number;
	targetTemporalLayer: number;
	currentTemporalLayer: number;
};

export type SvcConsumerDump = SimulcastConsumerDump;

export type PipeConsumerDump = BaseConsumerDump & {
	type: string;
	rtpStreams: RtpStreamDump[];
};

export type BaseConsumerDump = {
	id: string;
	producerId: string;
	kind: MediaKind;
	rtpParameters: RtpParameters;
	consumableRtpEncodings?: RtpEncodingParameters[];
	supportedCodecPayloadTypes: number[];
	traceEventTypes: string[];
	paused: boolean;
	producerPaused: boolean;
	priority: number;
};

export type RtpStreamDump = {
	params: RtpStreamParametersDump;
	score: number;
	rtxStream?: RtxStreamDump;
};

export type RtpStreamParametersDump = {
	encodingIdx: number;
	ssrc: number;
	payloadType: number;
	mimeType: string;
	clockRate: number;
	rid?: string;
	cname: string;
	rtxSsrc?: number;
	rtxPayloadType?: number;
	useNack: boolean;
	usePli: boolean;
	useFir: boolean;
	useInBandFec: boolean;
	useDtx: boolean;
	spatialLayers: number;
	temporalLayers: number;
};

export type RtxStreamDump = {
	params: RtxStreamParameters;
};

export type RtxStreamParameters = {
	ssrc: number;
	payloadType: number;
	mimeType: string;
	clockRate: number;
	rrid?: string;
	cname: string;
};

export type ConsumerStat = RtpStreamSendStats;

/**
 * Valid types for 'trace' event.
 */
export type ConsumerTraceEventType =
	| 'rtp'
	| 'keyframe'
	| 'nack'
	| 'pli'
	| 'fir';

/**
 * 'trace' event data.
 */
export type ConsumerTraceEventData = {
	/**
	 * Trace type.
	 */
	type: ConsumerTraceEventType;

	/**
	 * Event timestamp.
	 */
	timestamp: number;

	/**
	 * Event direction.
	 */
	direction: 'in' | 'out';

	/**
	 * Per type information.
	 */
	info: any;
};

export type ConsumerEvents = {
	transportclose: [];
	producerclose: [];
	producerpause: [];
	producerresume: [];
	score: [ConsumerScore];
	layerschange: [ConsumerLayers?];
	trace: [ConsumerTraceEventData];
	rtp: [Buffer];
	listenererror: [string, Error];
	// Private events.
	'@close': [];
	'@producerclose': [];
};

export type ConsumerObserver = EnhancedEventEmitter<ConsumerObserverEvents>;

export type ConsumerObserverEvents = {
	close: [];
	pause: [];
	resume: [];
	score: [ConsumerScore];
	layerschange: [ConsumerLayers?];
	trace: [ConsumerTraceEventData];
};

export interface Consumer<ConsumerAppData extends AppData = AppData>
	extends EnhancedEventEmitter<ConsumerEvents> {
	/**
	 * Consumer id.
	 */
	get id(): string;

	/**
	 * Associated Producer id.
	 */
	get producerId(): string;

	/**
	 * Whether the Consumer is closed.
	 */
	get closed(): boolean;

	/**
	 * Media kind.
	 */
	get kind(): MediaKind;

	/**
	 * RTP parameters.
	 */
	get rtpParameters(): RtpParameters;

	/**
	 * Consumer type.
	 */
	get type(): ConsumerType;

	/**
	 * Whether the Consumer is paused.
	 */
	get paused(): boolean;

	/**
	 * Whether the associate Producer is paused.
	 */
	get producerPaused(): boolean;

	/**
	 * Current priority.
	 */
	get priority(): number;

	/**
	 * Consumer score.
	 */
	get score(): ConsumerScore;

	/**
	 * Preferred video layers.
	 */
	get preferredLayers(): ConsumerLayers | undefined;

	/**
	 * Current video layers.
	 */
	get currentLayers(): ConsumerLayers | undefined;

	/**
	 * App custom data.
	 */
	get appData(): ConsumerAppData;

	/**
	 * App custom data setter.
	 */
	set appData(appData: ConsumerAppData);

	/**
	 * Observer.
	 */
	get observer(): ConsumerObserver;

	/**
	 * Close the Consumer.
	 */
	close(): void;

	/**
	 * Transport was closed.
	 *
	 * @private
	 */
	transportClosed(): void;

	/**
	 * Dump Consumer.
	 */
	dump(): Promise<ConsumerDump>;

	/**
	 * Get Consumer stats.
	 */
	getStats(): Promise<(ConsumerStat | ProducerStat)[]>;

	/**
	 * Pause the Consumer.
	 */
	pause(): Promise<void>;

	/**
	 * Resume the Consumer.
	 */
	resume(): Promise<void>;

	/**
	 * Set preferred video layers.
	 */
	setPreferredLayers({
		spatialLayer,
		temporalLayer,
	}: ConsumerLayers): Promise<void>;

	/**
	 * Set priority.
	 */
	setPriority(priority: number): Promise<void>;

	/**
	 * Unset priority.
	 */
	unsetPriority(): Promise<void>;

	/**
	 * Request a key frame to the Producer.
	 */
	requestKeyFrame(): Promise<void>;

	/**
	 * Enable 'trace' event.
	 */
	enableTraceEvent(types?: ConsumerTraceEventType[]): Promise<void>;
}
