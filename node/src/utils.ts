import { randomUUID, randomInt } from 'node:crypto';
import { ProducerType } from './ProducerTypes';
import { Type as FbsRtpParametersType } from './fbs/rtp-parameters';

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
 * Get the flatbuffers RtpParameters type for a given Producer.
 */
export function getRtpParametersType(
	producerType: ProducerType,
	pipe: boolean
): FbsRtpParametersType {
	if (pipe) {
		return FbsRtpParametersType.PIPE;
	}

	switch (producerType) {
		case 'simple': {
			return FbsRtpParametersType.SIMPLE;
		}

		case 'simulcast': {
			return FbsRtpParametersType.SIMULCAST;
		}

		case 'svc': {
			return FbsRtpParametersType.SVC;
		}
	}
}

/**
 * Parse flatbuffers vector into an array of the given type.
 */
export function parseVector<Type>(
	binary: any,
	methodName: string,
	parseFn?: (binary2: any) => Type
): Type[] {
	const array: Type[] = [];

	for (let i = 0; i < binary[`${methodName}Length`](); ++i) {
		if (parseFn) {
			array.push(parseFn(binary[methodName](i)));
		} else {
			array.push(binary[methodName](i) as Type);
		}
	}

	return array;
}

/**
 * Parse flatbuffers vector of StringString into the corresponding array.
 */
export function parseStringStringVector(
	binary: any,
	methodName: string
): { key: string; value: string }[] {
	const array: { key: string; value: string }[] = [];

	for (let i = 0; i < binary[`${methodName}Length`](); ++i) {
		const kv = binary[methodName](i)!;

		array.push({ key: kv.key(), value: kv.value() });
	}

	return array;
}

/**
 * Parse flatbuffers vector of StringUint8 into the corresponding array.
 */
export function parseStringUint8Vector(
	binary: any,
	methodName: string
): { key: string; value: number }[] {
	const array: { key: string; value: number }[] = [];

	for (let i = 0; i < binary[`${methodName}Length`](); ++i) {
		const kv = binary[methodName](i)!;

		array.push({ key: kv.key(), value: kv.value() });
	}

	return array;
}

/**
 * Parse flatbuffers vector of Uint16String into the corresponding array.
 */
export function parseUint16StringVector(
	binary: any,
	methodName: string
): { key: number; value: string }[] {
	const array: { key: number; value: string }[] = [];

	for (let i = 0; i < binary[`${methodName}Length`](); ++i) {
		const kv = binary[methodName](i)!;

		array.push({ key: kv.key(), value: kv.value() });
	}

	return array;
}

/**
 * Parse flatbuffers vector of Uint32String into the corresponding array.
 */
export function parseUint32StringVector(
	binary: any,
	methodName: string
): { key: number; value: string }[] {
	const array: { key: number; value: string }[] = [];

	for (let i = 0; i < binary[`${methodName}Length`](); ++i) {
		const kv = binary[methodName](i)!;

		array.push({ key: kv.key(), value: kv.value() });
	}

	return array;
}

/**
 * Parse flatbuffers vector of StringStringArray into the corresponding array.
 */
export function parseStringStringArrayVector(
	binary: any,
	methodName: string
): { key: string; values: string[] }[] {
	const array: { key: string; values: string[] }[] = [];

	for (let i = 0; i < binary[`${methodName}Length`](); ++i) {
		const kv = binary[methodName](i)!;
		const values: string[] = [];

		for (let i2 = 0; i2 < kv.valuesLength(); ++i2) {
			values.push(kv.values(i2)! as string);
		}

		array.push({ key: kv.key(), values });
	}

	return array;
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
