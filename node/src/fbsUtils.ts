import { Type as FbsRtpParametersType } from './fbs/rtp-parameters';

/**
 * Parse flatbuffers vector into an array of the given T.
 */
export function parseVector<T>(
	binary: any,
	methodName: string,
	parseFn?: (binary2: any) => T
): T[] {
	const array: T[] = [];

	for (let i = 0; i < binary[`${methodName}Length`](); ++i) {
		if (parseFn) {
			array.push(parseFn(binary[methodName](i)));
		} else {
			array.push(binary[methodName](i) as T);
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
