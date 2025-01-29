export type * from './indexTypes';
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
export type * from './rtpParametersTypes';
export type * from './rtpStreamStatsTypes';
export type * from './sctpParametersTypes';
export type * from './srtpParametersTypes';
export type * from './scalabilityModesTypes';
export type * from './errors';

type Only<T, U> = {
	[P in keyof T]: T[P];
} & {
	[P in keyof U]?: never;
};

export type Either<T, U> = Only<T, U> | Only<U, T>;

export type AppData = {
	[key: string]: unknown;
};
