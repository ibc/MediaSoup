import { EventEmitter, once } from 'node:events';

type Events = Record<string, any[]>;

export class EnhancedEventEmitter<
	E extends Events = Events,
> extends EventEmitter {
	constructor() {
		super();

		this.setMaxListeners(Infinity);
	}

	emit<K extends keyof E & string>(eventName: K, ...args: E[K]): boolean {
		return super.emit(eventName, ...args);
	}

	/**
	 * Special addition to the EventEmitter API.
	 */
	safeEmit<K extends keyof E & string>(eventName: K, ...args: E[K]): boolean {
		try {
			return super.emit(eventName, ...args);
		} catch (error) {
			try {
				super.emit('listenererror', eventName, error);
			} catch (error2) {
				// Ignore it.
			}

			return Boolean(super.listenerCount(eventName));
		}
	}

	on<K extends keyof E & string>(
		eventName: K,
		listener: (...args: E[K]) => void
	): this {
		super.on(eventName, listener as (...args: any[]) => void);

		return this;
	}

	off<K extends keyof E & string>(
		eventName: K,
		listener: (...args: E[K]) => void
	): this {
		super.off(eventName, listener as (...args: any[]) => void);

		return this;
	}

	addListener<K extends keyof E & string>(
		eventName: K,
		listener: (...args: E[K]) => void
	): this {
		super.on(eventName, listener as (...args: any[]) => void);

		return this;
	}

	prependListener<K extends keyof E & string>(
		eventName: K,
		listener: (...args: E[K]) => void
	): this {
		super.prependListener(eventName, listener as (...args: any[]) => void);

		return this;
	}

	once<K extends keyof E & string>(
		eventName: K,
		listener: (...args: E[K]) => void
	): this {
		super.once(eventName, listener as (...args: any[]) => void);

		return this;
	}

	prependOnceListener<K extends keyof E & string>(
		eventName: K,
		listener: (...args: E[K]) => void
	): this {
		super.prependOnceListener(eventName, listener as (...args: any[]) => void);

		return this;
	}

	removeListener<K extends keyof E & string>(
		eventName: K,
		listener: (...args: E[K]) => void
	): this {
		super.off(eventName, listener as (...args: any[]) => void);

		return this;
	}

	removeAllListeners<K extends keyof E & string>(eventName?: K): this {
		super.removeAllListeners(eventName);

		return this;
	}

	listenerCount<K extends keyof E & string>(eventName: K): number {
		return super.listenerCount(eventName);
	}

	// eslint-disable-next-line @typescript-eslint/no-unsafe-function-type
	listeners<K extends keyof E & string>(eventName: K): Function[] {
		return super.listeners(eventName);
	}

	// eslint-disable-next-line @typescript-eslint/no-unsafe-function-type
	rawListeners<K extends keyof E & string>(eventName: K): Function[] {
		return super.rawListeners(eventName);
	}
}

/**
 * TypeScript version of events.once():
 *   https://nodejs.org/api/events.html#eventsonceemitter-name-options
 *
 * Usage example:
 * ```ts
 * await enhancedOnce<ConsumerEvents>(videoConsumer, 'producerpause');
 * ````
 */
export async function enhancedOnce<E extends Events = Events>(
	emmiter: EnhancedEventEmitter<E>,
	eventName: keyof E & string,
	options?: any
): Promise<any[]> {
	return once(emmiter, eventName, options);
}
