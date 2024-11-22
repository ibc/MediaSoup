/**
 * Error indicating not support for something.
 */
export class UnsupportedError extends Error {
	constructor(message: string) {
		super(message);

		this.name = 'UnsupportedError';

		if (Error.hasOwnProperty('captureStackTrace')) {
			// Just in V8.
			Error.captureStackTrace(this, UnsupportedError);
		} else {
			this.stack = new Error(message).stack;
		}
	}
}

/**
 * Error produced when calling a method in an invalid state.
 */
export class InvalidStateError extends Error {
	constructor(message: string) {
		super(message);

		this.name = 'InvalidStateError';

		if (Error.hasOwnProperty('captureStackTrace')) {
			// Just in V8.
			Error.captureStackTrace(this, InvalidStateError);
		} else {
			this.stack = new Error(message).stack;
		}
	}
}
