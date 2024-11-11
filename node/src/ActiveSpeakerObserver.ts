import { Logger } from './Logger';
import { EnhancedEventEmitter } from './enhancedEvents';
import type {
	ActiveSpeakerObserver,
	ActiveSpeakerObserverDominantSpeaker,
	ActiveSpeakerObserverEvents,
	ActiveSpeakerObserverObserver,
	ActiveSpeakerObserverObserverEvents,
} from './ActiveSpeakerObserverTypes';
import type { RtpObserver } from './RtpObserverTypes';
import { RtpObserverImpl, RtpObserverConstructorOptions } from './RtpObserver';
import type { AppData } from './types';
import { Event, Notification } from './fbs/notification';
import * as FbsActiveSpeakerObserver from './fbs/active-speaker-observer';

type RtpObserverObserverConstructorOptions<ActiveSpeakerObserverAppData> =
	RtpObserverConstructorOptions<ActiveSpeakerObserverAppData>;

const logger = new Logger('ActiveSpeakerObserver');

export class ActiveSpeakerObserverImpl<
		ActiveSpeakerObserverAppData extends AppData = AppData,
	>
	extends RtpObserverImpl<
		ActiveSpeakerObserverAppData,
		ActiveSpeakerObserverEvents,
		ActiveSpeakerObserverObserver
	>
	implements RtpObserver, ActiveSpeakerObserver
{
	constructor(
		options: RtpObserverObserverConstructorOptions<ActiveSpeakerObserverAppData>
	) {
		const observer: ActiveSpeakerObserverObserver =
			new EnhancedEventEmitter<ActiveSpeakerObserverObserverEvents>();

		super(options, observer);

		this.handleWorkerNotifications();
		this.handleListenerError();
	}

	get type(): 'activespeaker' {
		return 'activespeaker';
	}

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

	private handleListenerError(): void {
		this.on('listenererror', (eventName, error) => {
			logger.error(
				`event listener threw an error [eventName:${eventName}]:`,
				error
			);
		});
	}
}
