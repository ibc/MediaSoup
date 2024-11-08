import { EnhancedEventEmitter } from './enhancedEvents';
import { MediaKind, RtpParameters } from './RtpParameters';
import { RtpStreamRecvStats } from './RtpStream';
import { AppData } from './types';

export type ProducerOptions<ProducerAppData extends AppData = AppData> = {
	/**
	 * Producer id (just for Router.pipeToRouter() method).
	 */
	id?: string;

	/**
	 * Media kind ('audio' or 'video').
	 */
	kind: MediaKind;

	/**
	 * RTP parameters defining what the endpoint is sending.
	 */
	rtpParameters: RtpParameters;

	/**
	 * Whether the producer must start in paused mode. Default false.
	 */
	paused?: boolean;

	/**
	 * Just for video. Time (in ms) before asking the sender for a new key frame
	 * after having asked a previous one. Default 0.
	 */
	keyFrameRequestDelay?: number;

	/**
	 * Custom application data.
	 */
	appData?: ProducerAppData;
};

/**
 * Producer type.
 */
export type ProducerType = 'simple' | 'simulcast' | 'svc';

export type ProducerScore = {
	/**
	 * Index of the RTP stream in the rtpParameters.encodings array.
	 */
	encodingIdx: number;

	/**
	 * SSRC of the RTP stream.
	 */
	ssrc: number;

	/**
	 * RID of the RTP stream.
	 */
	rid?: string;

	/**
	 * The score of the RTP stream.
	 */
	score: number;
};

export type ProducerVideoOrientation = {
	/**
	 * Whether the source is a video camera.
	 */
	camera: boolean;

	/**
	 * Whether the video source is flipped.
	 */
	flip: boolean;

	/**
	 * Rotation degrees (0, 90, 180 or 270).
	 */
	rotation: number;
};

export type ProducerDump = {
	id: string;
	kind: string;
	type: ProducerType;
	rtpParameters: RtpParameters;
	rtpMapping: any;
	rtpStreams: any;
	traceEventTypes: string[];
	paused: boolean;
};

export type ProducerStat = RtpStreamRecvStats;

/**
 * Valid types for 'trace' event.
 */
export type ProducerTraceEventType =
	| 'rtp'
	| 'keyframe'
	| 'nack'
	| 'pli'
	| 'fir'
	| 'sr';

/**
 * 'trace' event data.
 */
export type ProducerTraceEventData = {
	/**
	 * Trace type.
	 */
	type: ProducerTraceEventType;

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

export type ProducerEvents = {
	transportclose: [];
	score: [ProducerScore[]];
	videoorientationchange: [ProducerVideoOrientation];
	trace: [ProducerTraceEventData];
	listenererror: [string, Error];
	// Private events.
	'@close': [];
};

export type ProducerObserver = EnhancedEventEmitter<ProducerObserverEvents>;

export type ProducerObserverEvents = {
	close: [];
	pause: [];
	resume: [];
	score: [ProducerScore[]];
	videoorientationchange: [ProducerVideoOrientation];
	trace: [ProducerTraceEventData];
};

export interface ProducerInterface<ProducerAppData extends AppData = AppData>
	extends EnhancedEventEmitter<ProducerEvents> {
	get id(): string;

	get closed(): boolean;

	get kind(): MediaKind;

	get rtpParameters(): RtpParameters;

	get type(): ProducerType;

	/**
	 * Consumable RTP parameters.
	 *
	 * @private
	 */
	get consumableRtpParameters(): RtpParameters;

	get paused(): boolean;

	get score(): ProducerScore[];

	get appData(): ProducerAppData;

	set appData(appData: ProducerAppData);

	get observer(): ProducerObserver;

	close(): void;

	/**
	 * Transport was closed.
	 *
	 * @private
	 */
	transportClosed(): void;

	dump(): Promise<ProducerDump>;

	getStats(): Promise<ProducerStat[]>;

	pause(): Promise<void>;

	resume(): Promise<void>;

	enableTraceEvent(types?: ProducerTraceEventType[]): Promise<void>;

	send(rtpPacket: Buffer): void;
}
