import { EnhancedEventEmitter } from './enhancedEvents';
import { ProducerInterface } from './ProducerTypes';
import { AppData } from './types';

export type RtpObserverEvents = {
	routerclose: [];
	listenererror: [string, Error];
	// Private events.
	'@close': [];
};

export type RtpObserverObserver =
	EnhancedEventEmitter<RtpObserverObserverEvents>;

export type RtpObserverObserverEvents = {
	close: [];
	pause: [];
	resume: [];
	addproducer: [ProducerInterface];
	removeproducer: [ProducerInterface];
};

export interface RtpObserverInterface<
	RtpObserverAppData extends AppData = AppData,
	Events extends RtpObserverEvents = RtpObserverEvents,
	Observer extends RtpObserverObserver = RtpObserverObserver,
> extends EnhancedEventEmitter<Events> {
	get id(): string;

	get closed(): boolean;

	get paused(): boolean;

	get appData(): RtpObserverAppData;

	set appData(appData: RtpObserverAppData);

	get observer(): Observer;

	close(): void;

	/**
	 * Router was closed.
	 *
	 * @private
	 */
	routerClosed(): void;

	pause(): Promise<void>;

	resume(): Promise<void>;

	addProducer({ producerId }: { producerId: string }): Promise<void>;

	removeProducer({ producerId }: { producerId: string }): Promise<void>;
}
