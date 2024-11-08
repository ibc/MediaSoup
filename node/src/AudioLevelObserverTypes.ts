import { EnhancedEventEmitter } from './enhancedEvents';
import {
	RtpObserver,
	RtpObserverEvents,
	RtpObserverObserverEvents,
} from './RtpObserverTypes';
import { Producer } from './ProducerTypes';
import { AppData } from './types';

export type AudioLevelObserverOptions<
	AudioLevelObserverAppData extends AppData = AppData,
> = {
	/**
	 * Maximum number of entries in the 'volumes”' event. Default 1.
	 */
	maxEntries?: number;

	/**
	 * Minimum average volume (in dBvo from -127 to 0) for entries in the
	 * 'volumes' event.	Default -80.
	 */
	threshold?: number;

	/**
	 * Interval in ms for checking audio volumes. Default 1000.
	 */
	interval?: number;

	/**
	 * Custom application data.
	 */
	appData?: AudioLevelObserverAppData;
};

export type AudioLevelObserverVolume = {
	/**
	 * The audio Producer instance.
	 */
	producer: Producer;

	/**
	 * The average volume (in dBvo from -127 to 0) of the audio Producer in the
	 * last interval.
	 */
	volume: number;
};

export type AudioLevelObserverEvents = RtpObserverEvents & {
	volumes: [AudioLevelObserverVolume[]];
	silence: [];
};

export type AudioLevelObserverObserver =
	EnhancedEventEmitter<AudioLevelObserverObserverEvents>;

export type AudioLevelObserverObserverEvents = RtpObserverObserverEvents & {
	volumes: [AudioLevelObserverVolume[]];
	silence: [];
};

export interface AudioLevelObserver<
	AudioLevelObserverAppData extends AppData = AppData,
> extends RtpObserver<
		AudioLevelObserverAppData,
		AudioLevelObserverEvents,
		AudioLevelObserverObserver
	> {
	/**
	 * Observer.
	 *
	 * @override
	 */
	get observer(): AudioLevelObserverObserver;
}
