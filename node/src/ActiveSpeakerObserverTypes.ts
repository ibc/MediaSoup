import type { EnhancedEventEmitter } from './enhancedEvents';
import type {
	RtpObserver,
	RtpObserverEvents,
	RtpObserverObserverEvents,
} from './RtpObserverTypes';
import type { Producer } from './ProducerTypes';
import type { AppData } from './types';

export type ActiveSpeakerObserverOptions<
	ActiveSpeakerObserverAppData extends AppData = AppData,
> = {
	interval?: number;

	/**
	 * Custom application data.
	 */
	appData?: ActiveSpeakerObserverAppData;
};

export type ActiveSpeakerObserverDominantSpeaker = {
	/**
	 * The audio Producer instance.
	 */
	producer: Producer;
};

export type ActiveSpeakerObserverEvents = RtpObserverEvents & {
	dominantspeaker: [ActiveSpeakerObserverDominantSpeaker];
};

export type ActiveSpeakerObserverObserver =
	EnhancedEventEmitter<ActiveSpeakerObserverObserverEvents>;

export type ActiveSpeakerObserverObserverEvents = RtpObserverObserverEvents & {
	dominantspeaker: [ActiveSpeakerObserverDominantSpeaker];
};

export interface ActiveSpeakerObserver<
	ActiveSpeakerObserverAppData extends AppData = AppData,
> extends RtpObserver<
		ActiveSpeakerObserverAppData,
		ActiveSpeakerObserverEvents,
		ActiveSpeakerObserverObserver
	> {
	/**
	 * Observer.
	 *
	 * @override
	 */
	get observer(): ActiveSpeakerObserverObserver;
}
