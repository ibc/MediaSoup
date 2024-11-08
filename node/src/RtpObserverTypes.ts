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
	/**
	 * RtpObserver id.
	 */
	get id(): string;

	/**
	 * Whether the RtpObserver is closed.
	 */
	get closed(): boolean;

	/**
	 * Whether the RtpObserver is paused.
	 */
	get paused(): boolean;

	/**
	 * App custom data.
	 */
	get appData(): RtpObserverAppData;

	/**
	 * App custom data setter.
	 */
	set appData(appData: RtpObserverAppData);

	/**
	 * Observer.
	 *
	 * @virtual
	 */
	get observer(): Observer;

	/**
	 * Close the RtpObserver.
	 */
	close(): void;

	/**
	 * Router was closed.
	 *
	 * @private
	 */
	routerClosed(): void;

	/**
	 * Pause the RtpObserver.
	 */
	pause(): Promise<void>;

	/**
	 * Resume the RtpObserver.
	 */
	resume(): Promise<void>;

	/**
	 * Add a Producer to the RtpObserver.
	 */
	addProducer({ producerId }: { producerId: string }): Promise<void>;

	/**
	 * Remove a Producer from the RtpObserver.
	 */
	removeProducer({ producerId }: { producerId: string }): Promise<void>;
}
