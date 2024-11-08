import { EnhancedEventEmitter } from './enhancedEvents';
import {
	RtpObserverInterface,
	RtpObserverEvents,
	RtpObserverObserverEvents,
} from './RtpObserverTypes';
import { ProducerInterface } from './ProducerTypes';
import { AppData } from './types';

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
	producer: ProducerInterface;
};

export type ActiveSpeakerObserverEvents = RtpObserverEvents & {
	dominantspeaker: [ActiveSpeakerObserverDominantSpeaker];
};

export type ActiveSpeakerObserverObserver =
	EnhancedEventEmitter<ActiveSpeakerObserverObserverEvents>;

export type ActiveSpeakerObserverObserverEvents = RtpObserverObserverEvents & {
	dominantspeaker: [ActiveSpeakerObserverDominantSpeaker];
};

export interface ActiveSpeakerObserverInterface<
	ActiveSpeakerObserverAppData extends AppData = AppData,
> extends RtpObserverInterface<
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
