import { Logger } from './Logger';
import { EnhancedEventEmitter } from './enhancedEvents';
import {
	AudioLevelObserverInterface,
	AudioLevelObserverVolume,
	AudioLevelObserverEvents,
	AudioLevelObserverObserver,
	AudioLevelObserverObserverEvents,
} from './AudioLevelObserverTypes';
import { RtpObserverInterface } from './RtpObserverTypes';
import { RtpObserverImpl, RtpObserverConstructorOptions } from './RtpObserver';
import { ProducerInterface } from './ProducerTypes';
import { AppData } from './types';
import * as utils from './utils';
import { Event, Notification } from './fbs/notification';
import * as FbsAudioLevelObserver from './fbs/audio-level-observer';

type AudioLevelObserverConstructorOptions<AudioLevelObserverAppData> =
	RtpObserverConstructorOptions<AudioLevelObserverAppData>;

const logger = new Logger('AudioLevelObserver');

export class AudioLevelObserverImpl<
		AudioLevelObserverAppData extends AppData = AppData,
	>
	extends RtpObserverImpl<
		AudioLevelObserverAppData,
		AudioLevelObserverEvents,
		AudioLevelObserverObserver
	>
	implements RtpObserverInterface, AudioLevelObserverInterface
{
	/**
	 * @private
	 */
	constructor(
		options: AudioLevelObserverConstructorOptions<AudioLevelObserverAppData>
	) {
		const observer: AudioLevelObserverObserver =
			new EnhancedEventEmitter<AudioLevelObserverObserverEvents>();

		super(options, observer);

		this.handleWorkerNotifications();
	}

	/**
	 * Observer.
	 *
	 * @override
	 */
	get observer(): AudioLevelObserverObserver {
		return super.observer;
	}

	private handleWorkerNotifications(): void {
		this.channel.on(
			this.internal.rtpObserverId,
			(event: Event, data?: Notification) => {
				switch (event) {
					case Event.AUDIOLEVELOBSERVER_VOLUMES: {
						const notification =
							new FbsAudioLevelObserver.VolumesNotification();

						data!.body(notification);

						// Get the corresponding Producer instance and remove entries with
						// no Producer (it may have been closed in the meanwhile).
						const volumes: AudioLevelObserverVolume[] = utils
							.parseVector(notification, 'volumes', parseVolume)
							.map(
								({
									producerId,
									volume,
								}: {
									producerId: string;
									volume: number;
								}) => ({
									producer: this.getProducerById(producerId)!,
									volume,
								})
							)
							.filter(
								({ producer }: { producer: ProducerInterface }) => producer
							);

						if (volumes.length > 0) {
							this.safeEmit('volumes', volumes);

							// Emit observer event.
							this.observer.safeEmit('volumes', volumes);
						}

						break;
					}

					case Event.AUDIOLEVELOBSERVER_SILENCE: {
						this.safeEmit('silence');

						// Emit observer event.
						this.observer.safeEmit('silence');

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

function parseVolume(binary: FbsAudioLevelObserver.Volume): {
	producerId: string;
	volume: number;
} {
	return {
		producerId: binary.producerId()!,
		volume: binary.volume(),
	};
}
