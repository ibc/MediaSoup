import { Logger } from './Logger';
import { EnhancedEventEmitter } from './enhancedEvents';
import {
	ActiveSpeakerObserverInterface,
	ActiveSpeakerObserverDominantSpeaker,
	ActiveSpeakerObserverEvents,
	ActiveSpeakerObserverObserver,
	ActiveSpeakerObserverObserverEvents,
} from './ActiveSpeakerObserverInterface';
import { RtpObserverInterface } from './RtpObserverInterface';
import { RtpObserver, RtpObserverConstructorOptions } from './RtpObserver';
import { AppData } from './types';
import { Event, Notification } from './fbs/notification';
import * as FbsActiveSpeakerObserver from './fbs/active-speaker-observer';

type RtpObserverObserverConstructorOptions<ActiveSpeakerObserverAppData> =
	RtpObserverConstructorOptions<ActiveSpeakerObserverAppData>;

const logger = new Logger('ActiveSpeakerObserver');

export class ActiveSpeakerObserver<
		ActiveSpeakerObserverAppData extends AppData = AppData,
	>
	extends RtpObserver<
		ActiveSpeakerObserverAppData,
		ActiveSpeakerObserverEvents,
		ActiveSpeakerObserverObserver
	>
	implements RtpObserverInterface, ActiveSpeakerObserverInterface
{
	/**
	 * @private
	 */
	constructor(
		options: RtpObserverObserverConstructorOptions<ActiveSpeakerObserverAppData>
	) {
		const observer: ActiveSpeakerObserverObserver =
			new EnhancedEventEmitter<ActiveSpeakerObserverObserverEvents>();

		super(options, observer);

		this.handleWorkerNotifications();
	}

	/**
	 * Observer.
	 *
	 * @override
	 */
	get observer(): ActiveSpeakerObserverObserver {
		return super.observer;
	}

	private handleWorkerNotifications(): void {
		this.channel.on(
			this.internal.rtpObserverId,
			(event: Event, data?: Notification) => {
				switch (event) {
					case Event.ACTIVESPEAKEROBSERVER_DOMINANT_SPEAKER: {
						const notification =
							new FbsActiveSpeakerObserver.DominantSpeakerNotification();

						data!.body(notification);

						const producer = this.getProducerById(notification.producerId()!);

						if (!producer) {
							break;
						}

						const dominantSpeaker: ActiveSpeakerObserverDominantSpeaker = {
							producer,
						};

						this.safeEmit('dominantspeaker', dominantSpeaker);
						this.observer.safeEmit('dominantspeaker', dominantSpeaker);

						break;
					}

					default: {
						logger.error(`ignoring unknown event "${event}"`);
					}
				}
			}
		);
	}
}
