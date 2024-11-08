import * as flatbuffers from 'flatbuffers';
import {
	SctpStreamParameters,
	SctpParametersDump,
} from './sctpParametersTypes';
import * as FbsSctpParameters from './fbs/sctp-parameters';

export function parseSctpParametersDump(
	binary: FbsSctpParameters.SctpParameters
): SctpParametersDump {
	return {
		port: binary.port(),
		OS: binary.os(),
		MIS: binary.mis(),
		maxMessageSize: binary.maxMessageSize(),
		sendBufferSize: binary.sendBufferSize(),
		sctpBufferedAmount: binary.sctpBufferedAmount(),
		isDataChannel: binary.isDataChannel(),
	};
}

export function serializeSctpStreamParameters(
	builder: flatbuffers.Builder,
	parameters: SctpStreamParameters
): number {
	return FbsSctpParameters.SctpStreamParameters.createSctpStreamParameters(
		builder,
		parameters.streamId,
		parameters.ordered!,
		typeof parameters.maxPacketLifeTime === 'number'
			? parameters.maxPacketLifeTime
			: null,
		typeof parameters.maxRetransmits === 'number'
			? parameters.maxRetransmits
			: null
	);
}

export function parseSctpStreamParameters(
	parameters: FbsSctpParameters.SctpStreamParameters
): SctpStreamParameters {
	return {
		streamId: parameters.streamId(),
		ordered: parameters.ordered()!,
		maxPacketLifeTime:
			parameters.maxPacketLifeTime() !== null
				? parameters.maxPacketLifeTime()!
				: undefined,
		maxRetransmits:
			parameters.maxRetransmits() !== null
				? parameters.maxRetransmits()!
				: undefined,
	};
}
