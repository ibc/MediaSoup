import { randomUUID, randomInt } from 'node:crypto';

/**
 * Clones the given value.
 */
export function clone<T>(value: T): T {
	if (value === undefined) {
		return undefined as unknown as T;
	} else if (Number.isNaN(value)) {
		return NaN as unknown as T;
	} else if (typeof structuredClone === 'function') {
		// Available in Node >= 18.
		return structuredClone(value);
	} else {
		return JSON.parse(JSON.stringify(value));
	}
}

/**
 * Generates a random UUID v4.
 */
export function generateUUIDv4(): string {
	return randomUUID();
}

/**
 * Generates a random positive integer.
 */
export function generateRandomNumber(): number {
	return randomInt(100_000_000, 999_999_999);
}

/**
 * Make an object or array recursively immutable.
 * https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/freeze.
 */
export function deepFreeze<T>(object: T): T {
	// Retrieve the property names defined on object.
	const propNames = Reflect.ownKeys(object as any);

	// Freeze properties before freezing self.
	for (const name of propNames) {
		const value = (object as any)[name];

		if ((value && typeof value === 'object') || typeof value === 'function') {
			deepFreeze(value);
		}
	}

	return Object.freeze(object);
}
