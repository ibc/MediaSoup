export type { Observer, ObserverEvents } from './index';
export type * from './WorkerTypes';
export type * from './WebRtcServerTypes';
export type * from './RouterTypes';
export type * from './TransportTypes';
export type * from './WebRtcTransportTypes';
export type * from './PlainTransportTypes';
export type * from './PipeTransportTypes';
export type * from './DirectTransportTypes';
export type * from './ProducerTypes';
export type * from './ConsumerTypes';
export type * from './DataProducerTypes';
export type * from './DataConsumerTypes';
export type * from './RtpObserverTypes';
export type * from './ActiveSpeakerObserverTypes';
export type * from './AudioLevelObserverTypes';
export type * from './RtpParameters';
export type * from './SctpParameters';
export type * from './SrtpParameters';
export type * from './scalabilityModes';

// TODO: Here we are exporting real classes rather than types. This should
// be exported somehow else rather than in mediasoup.types namespace.
export * from './errors';

type Only<T, U> = {
	[P in keyof T]: T[P];
} & {
	[P in keyof U]?: never;
};

export type Either<T, U> = Only<T, U> | Only<U, T>;

export type AppData = {
	[key: string]: unknown;
};

/**
 * Event listeners for mediasoup generated logs.
 */
export type LogEventListeners = {
	ondebug?: (namespace: string, log: string) => void;
	onwarn?: (namespace: string, log: string) => void;
	onerror?: (namespace: string, log: string, error?: Error) => void;
};
