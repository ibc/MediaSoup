import { Logger } from './Logger';
import { EnhancedEventEmitter } from './enhancedEvents';
import { RtpObserverEvents, RtpObserverObserver } from './RtpObserverTypes';
import { Channel } from './Channel';
import { RouterInternal } from './Router';
import { ProducerInterface } from './ProducerTypes';
import { AppData } from './types';
import * as FbsRequest from './fbs/request';
import * as FbsRouter from './fbs/router';
import * as FbsRtpObserver from './fbs/rtp-observer';

export type RtpObserverConstructorOptions<RtpObserverAppData> = {
	internal: RtpObserverObserverInternal;
	channel: Channel;
	appData?: RtpObserverAppData;
	getProducerById: (producerId: string) => ProducerInterface | undefined;
};

type RtpObserverObserverInternal = RouterInternal & {
	rtpObserverId: string;
};

const logger = new Logger('RtpObserver');

export abstract class RtpObserverImpl<
	RtpObserverAppData extends AppData = AppData,
	Events extends RtpObserverEvents = RtpObserverEvents,
	Observer extends RtpObserverObserver = RtpObserverObserver,
> extends EnhancedEventEmitter<Events> {
	// Internal data.
	protected readonly internal: RtpObserverObserverInternal;

	// Channel instance.
	protected readonly channel: Channel;

	// Closed flag.
	#closed = false;

	// Paused flag.
	#paused = false;

	// Custom app data.
	#appData: RtpObserverAppData;

	// Method to retrieve a Producer.
	protected readonly getProducerById: (
		producerId: string
	) => ProducerInterface | undefined;

	// Observer instance.
	readonly #observer: Observer;

	/**
	 * @private
	 * @interface
	 */
	constructor(
		{
			internal,
			channel,
			appData,
			getProducerById,
		}: RtpObserverConstructorOptions<RtpObserverAppData>,
		observer: Observer
	) {
		super();

		logger.debug('constructor()');

		this.internal = internal;
		this.channel = channel;
		this.#appData = appData ?? ({} as RtpObserverAppData);
		this.getProducerById = getProducerById;
		this.#observer = observer;
	}

	/**
	 * RtpObserver id.
	 */
	get id(): string {
		return this.internal.rtpObserverId;
	}

	/**
	 * Whether the RtpObserver is closed.
	 */
	get closed(): boolean {
		return this.#closed;
	}

	/**
	 * Whether the RtpObserver is paused.
	 */
	get paused(): boolean {
		return this.#paused;
	}

	/**
	 * App custom data.
	 */
	get appData(): RtpObserverAppData {
		return this.#appData;
	}

	/**
	 * App custom data setter.
	 */
	set appData(appData: RtpObserverAppData) {
		this.#appData = appData;
	}

	/**
	 * Observer.
	 */
	get observer(): Observer {
		return this.#observer;
	}

	/**
	 * Close the RtpObserver.
	 */
	close(): void {
		if (this.#closed) {
			return;
		}

		logger.debug('close()');

		this.#closed = true;

		// Remove notification subscriptions.
		this.channel.removeAllListeners(this.internal.rtpObserverId);

		/* Build Request. */
		const requestOffset = new FbsRouter.CloseRtpObserverRequestT(
			this.internal.rtpObserverId
		).pack(this.channel.bufferBuilder);

		this.channel
			.request(
				FbsRequest.Method.ROUTER_CLOSE_RTPOBSERVER,
				FbsRequest.Body.Router_CloseRtpObserverRequest,
				requestOffset,
				this.internal.routerId
			)
			.catch(() => {});

		this.emit('@close');

		// Emit observer event.
		this.#observer.safeEmit('close');
	}

	/**
	 * Router was closed.
	 *
	 * @private
	 */
	routerClosed(): void {
		if (this.#closed) {
			return;
		}

		logger.debug('routerClosed()');

		this.#closed = true;

		// Remove notification subscriptions.
		this.channel.removeAllListeners(this.internal.rtpObserverId);

		this.safeEmit('routerclose');

		// Emit observer event.
		this.#observer.safeEmit('close');
	}

	/**
	 * Pause the RtpObserver.
	 */
	async pause(): Promise<void> {
		logger.debug('pause()');

		const wasPaused = this.#paused;

		await this.channel.request(
			FbsRequest.Method.RTPOBSERVER_PAUSE,
			undefined,
			undefined,
			this.internal.rtpObserverId
		);

		this.#paused = true;

		// Emit observer event.
		if (!wasPaused) {
			this.#observer.safeEmit('pause');
		}
	}

	/**
	 * Resume the RtpObserver.
	 */
	async resume(): Promise<void> {
		logger.debug('resume()');

		const wasPaused = this.#paused;

		await this.channel.request(
			FbsRequest.Method.RTPOBSERVER_RESUME,
			undefined,
			undefined,
			this.internal.rtpObserverId
		);

		this.#paused = false;

		// Emit observer event.
		if (wasPaused) {
			this.#observer.safeEmit('resume');
		}
	}

	/**
	 * Add a Producer to the RtpObserver.
	 */
	async addProducer({ producerId }: { producerId: string }): Promise<void> {
		logger.debug('addProducer()');

		const producer = this.getProducerById(producerId);

		if (!producer) {
			throw Error(`Producer with id "${producerId}" not found`);
		}

		const requestOffset = new FbsRtpObserver.AddProducerRequestT(
			producerId
		).pack(this.channel.bufferBuilder);

		await this.channel.request(
			FbsRequest.Method.RTPOBSERVER_ADD_PRODUCER,
			FbsRequest.Body.RtpObserver_AddProducerRequest,
			requestOffset,
			this.internal.rtpObserverId
		);

		// Emit observer event.
		this.#observer.safeEmit('addproducer', producer);
	}

	/**
	 * Remove a Producer from the RtpObserver.
	 */
	async removeProducer({ producerId }: { producerId: string }): Promise<void> {
		logger.debug('removeProducer()');

		const producer = this.getProducerById(producerId);

		if (!producer) {
			throw Error(`Producer with id "${producerId}" not found`);
		}

		const requestOffset = new FbsRtpObserver.RemoveProducerRequestT(
			producerId
		).pack(this.channel.bufferBuilder);

		await this.channel.request(
			FbsRequest.Method.RTPOBSERVER_REMOVE_PRODUCER,
			FbsRequest.Body.RtpObserver_RemoveProducerRequest,
			requestOffset,
			this.internal.rtpObserverId
		);

		// Emit observer event.
		this.#observer.safeEmit('removeproducer', producer);
	}
}
