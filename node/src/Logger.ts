import debug from 'debug';
import type { EnhancedEventEmitter } from './enhancedEvents';

const APP_NAME = 'mediasoup';

export type LoggerEmitterEvents = {
	debuglog: [string, string];
	warnlog: [string, string];
	errorlog: [string, string, Error?];
};

export type LoggerEmitter = EnhancedEventEmitter<LoggerEmitterEvents>;

export class Logger {
	private static debugLogEmitter?: LoggerEmitter;
	private static warnLogEmitter?: LoggerEmitter;
	private static errorLogEmitter?: LoggerEmitter;

	readonly #debug: debug.Debugger;
	readonly #warn: debug.Debugger;
	readonly #error: debug.Debugger;

	static setEmitters(
		debugLogEmitter?: LoggerEmitter,
		warnLogEmitter?: LoggerEmitter,
		errorLogEmitter?: LoggerEmitter
	): void {
		Logger.debugLogEmitter = debugLogEmitter;
		Logger.warnLogEmitter = warnLogEmitter;
		Logger.errorLogEmitter = errorLogEmitter;
	}

	constructor(prefix?: string) {
		if (prefix) {
			this.#debug = debug(`${APP_NAME}:${prefix}`);
			this.#warn = debug(`${APP_NAME}:WARN:${prefix}`);
			this.#error = debug(`${APP_NAME}:ERROR:${prefix}`);
		} else {
			this.#debug = debug(APP_NAME);
			this.#warn = debug(`${APP_NAME}:WARN`);
			this.#error = debug(`${APP_NAME}:ERROR`);
		}

		/* eslint-disable no-console */
		this.#debug.log = console.info.bind(console);
		this.#warn.log = console.warn.bind(console);
		this.#error.log = console.error.bind(console);
		/* eslint-enable no-console */
	}

	debug(log: string): void {
		this.#debug(log);

		Logger.debugLogEmitter?.safeEmit('debuglog', this.#debug.namespace, log);
	}

	warn(log: string): void {
		this.#warn(log);

		Logger.warnLogEmitter?.safeEmit('warnlog', this.#warn.namespace, log);
	}

	error(log: string, error?: Error): void {
		this.#error(log, error);

		Logger.errorLogEmitter?.safeEmit(
			'errorlog',
			this.#error.namespace,
			log,
			error
		);
	}
}
