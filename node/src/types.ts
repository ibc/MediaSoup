export type { Observer, ObserverEvents, LogEventListeners } from './index';
export type * from './RtpParameters';
export type * from './SctpParameters';
export type * from './SrtpParameters';
export type * from './scalabilityModes';

export * from './WorkerInterface';
export * from './WebRtcServerInterface';
export * from './RouterInterface';
export * from './TransportInterface';
export * from './WebRtcTransportInterface';
export * from './PlainTransportInterface';
export * from './PipeTransportInterface';
export * from './DirectTransportInterface';
export * from './ProducerInterface';
export * from './ConsumerInterface';
export * from './DataProducerInterface';
export * from './DataConsumerInterface';
export * from './RtpObserverInterface';
export * from './ActiveSpeakerObserverInterface';
export * from './AudioLevelObserverInterface';
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
