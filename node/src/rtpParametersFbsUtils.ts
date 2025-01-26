import * as flatbuffers from 'flatbuffers';
import type {
	RtpParameters,
	RtpCodecParameters,
	RtcpFeedback,
	RtpEncodingParameters,
	RtpHeaderExtensionUri,
	RtpHeaderExtensionParameters,
	RtcpParameters,
} from './rtpParametersTypes';
import * as fbsUtils from './fbsUtils';
import {
	Boolean as FbsBoolean,
	Double as FbsDouble,
	Integer32 as FbsInteger32,
	Integer32Array as FbsInteger32Array,
	String as FbsString,
	Parameter as FbsParameter,
	RtcpFeedback as FbsRtcpFeedback,
	RtcpParameters as FbsRtcpParameters,
	RtpCodecParameters as FbsRtpCodecParameters,
	RtpEncodingParameters as FbsRtpEncodingParameters,
	RtpHeaderExtensionParameters as FbsRtpHeaderExtensionParameters,
	RtpHeaderExtensionUri as FbsRtpHeaderExtensionUri,
	RtpParameters as FbsRtpParameters,
	Rtx as FbsRtx,
	Value as FbsValue,
} from './fbs/rtp-parameters';

export function serializeRtpParameters(
	builder: flatbuffers.Builder,
	rtpParameters: RtpParameters
): number {
	const codecs: number[] = [];
	const headerExtensions: number[] = [];

	for (const codec of rtpParameters.codecs) {
		const mimeTypeOffset = builder.createString(codec.mimeType);
		const parameters = serializeParameters(builder, codec.parameters);
		const parametersOffset = FbsRtpCodecParameters.createParametersVector(
			builder,
			parameters
		);

		const rtcpFeedback: number[] = [];

		for (const rtcp of codec.rtcpFeedback ?? []) {
			const typeOffset = builder.createString(rtcp.type);
			const rtcpParametersOffset = builder.createString(rtcp.parameter);

			rtcpFeedback.push(
				FbsRtcpFeedback.createRtcpFeedback(
					builder,
					typeOffset,
					rtcpParametersOffset
				)
			);
		}
		const rtcpFeedbackOffset = FbsRtpCodecParameters.createRtcpFeedbackVector(
			builder,
			rtcpFeedback
		);

		codecs.push(
			FbsRtpCodecParameters.createRtpCodecParameters(
				builder,
				mimeTypeOffset,
				codec.payloadType,
				codec.clockRate,
				Number(codec.channels),
				parametersOffset,
				rtcpFeedbackOffset
			)
		);
	}
	const codecsOffset = FbsRtpParameters.createCodecsVector(builder, codecs);

	// RtpHeaderExtensionParameters.
	for (const headerExtension of rtpParameters.headerExtensions ?? []) {
		const uri = rtpHeaderExtensionUriToFbs(headerExtension.uri);
		const parameters = serializeParameters(builder, headerExtension.parameters);
		const parametersOffset = FbsRtpCodecParameters.createParametersVector(
			builder,
			parameters
		);

		headerExtensions.push(
			FbsRtpHeaderExtensionParameters.createRtpHeaderExtensionParameters(
				builder,
				uri,
				headerExtension.id,
				Boolean(headerExtension.encrypt),
				parametersOffset
			)
		);
	}
	const headerExtensionsOffset = FbsRtpParameters.createHeaderExtensionsVector(
		builder,
		headerExtensions
	);

	// RtpEncodingParameters.
	const encodingsOffset = serializeRtpEncodingParameters(
		builder,
		rtpParameters.encodings ?? []
	);

	// RtcpParameters.
	const { cname, reducedSize } = rtpParameters.rtcp ?? { reducedSize: true };
	const cnameOffset = builder.createString(cname);

	const rtcpOffset = FbsRtcpParameters.createRtcpParameters(
		builder,
		cnameOffset,
		Boolean(reducedSize)
	);

	const midOffset = builder.createString(rtpParameters.mid);

	FbsRtpParameters.startRtpParameters(builder);
	FbsRtpParameters.addMid(builder, midOffset);
	FbsRtpParameters.addCodecs(builder, codecsOffset);

	FbsRtpParameters.addHeaderExtensions(builder, headerExtensionsOffset);
	FbsRtpParameters.addEncodings(builder, encodingsOffset);
	FbsRtpParameters.addRtcp(builder, rtcpOffset);

	return FbsRtpParameters.endRtpParameters(builder);
}

export function serializeRtpEncodingParameters(
	builder: flatbuffers.Builder,
	rtpEncodingParameters: RtpEncodingParameters[] = []
): number {
	const encodings: number[] = [];

	for (const encoding of rtpEncodingParameters) {
		// Prepare Rid.
		const ridOffset = builder.createString(encoding.rid);

		// Prepare Rtx.
		let rtxOffset: number | undefined;

		if (encoding.rtx) {
			rtxOffset = FbsRtx.createRtx(builder, encoding.rtx.ssrc);
		}

		// Prepare scalability mode.
		let scalabilityModeOffset: number | undefined;

		if (encoding.scalabilityMode) {
			scalabilityModeOffset = builder.createString(encoding.scalabilityMode);
		}

		// Start serialization.
		FbsRtpEncodingParameters.startRtpEncodingParameters(builder);

		// Add SSRC.
		if (encoding.ssrc) {
			FbsRtpEncodingParameters.addSsrc(builder, encoding.ssrc);
		}

		// Add Rid.
		FbsRtpEncodingParameters.addRid(builder, ridOffset);

		// Add payload type.
		if (encoding.codecPayloadType) {
			FbsRtpEncodingParameters.addCodecPayloadType(
				builder,
				encoding.codecPayloadType
			);
		}

		// Add RTX.
		if (rtxOffset) {
			FbsRtpEncodingParameters.addRtx(builder, rtxOffset);
		}

		// Add DTX.
		if (encoding.dtx !== undefined) {
			FbsRtpEncodingParameters.addDtx(builder, encoding.dtx);
		}

		// Add scalability ode.
		if (scalabilityModeOffset) {
			FbsRtpEncodingParameters.addScalabilityMode(
				builder,
				scalabilityModeOffset
			);
		}

		// Add max bitrate.
		if (encoding.maxBitrate !== undefined) {
			FbsRtpEncodingParameters.addMaxBitrate(builder, encoding.maxBitrate);
		}

		// End serialization.
		encodings.push(FbsRtpEncodingParameters.endRtpEncodingParameters(builder));
	}

	return FbsRtpParameters.createEncodingsVector(builder, encodings);
}

export function serializeParameters(
	builder: flatbuffers.Builder,
	parameters: any
): number[] {
	const fbsParameters: number[] = [];

	for (const key of Object.keys(parameters)) {
		const value = parameters[key];
		const keyOffset = builder.createString(key);
		let parameterOffset: number;

		if (typeof value === 'boolean') {
			parameterOffset = FbsParameter.createParameter(
				builder,
				keyOffset,
				FbsValue.Boolean,
				value === true ? 1 : 0
			);
		} else if (typeof value === 'number') {
			// Integer.
			if (value % 1 === 0) {
				const valueOffset = FbsInteger32.createInteger32(builder, value);

				parameterOffset = FbsParameter.createParameter(
					builder,
					keyOffset,
					FbsValue.Integer32,
					valueOffset
				);
			}
			// Float.
			else {
				const valueOffset = FbsDouble.createDouble(builder, value);

				parameterOffset = FbsParameter.createParameter(
					builder,
					keyOffset,
					FbsValue.Double,
					valueOffset
				);
			}
		} else if (typeof value === 'string') {
			const valueOffset = FbsString.createString(
				builder,
				builder.createString(value)
			);

			parameterOffset = FbsParameter.createParameter(
				builder,
				keyOffset,
				FbsValue.String,
				valueOffset
			);
		} else if (Array.isArray(value)) {
			const valueOffset = FbsInteger32Array.createValueVector(builder, value);

			parameterOffset = FbsParameter.createParameter(
				builder,
				keyOffset,
				FbsValue.Integer32Array,
				valueOffset
			);
		} else {
			throw new Error(`invalid parameter type [key:'${key}', value:${value}]`);
		}

		fbsParameters.push(parameterOffset);
	}

	return fbsParameters;
}

export function parseRtcpFeedback(data: FbsRtcpFeedback): RtcpFeedback {
	return {
		type: data.type()!,
		parameter: data.parameter() ?? undefined,
	};
}

export function parseParameters(data: any): any {
	const parameters: any = {};

	for (let i = 0; i < data.parametersLength(); i++) {
		const fbsParameter = data.parameters(i)!;

		switch (fbsParameter.valueType()) {
			case FbsValue.Boolean: {
				const value = new FbsBoolean();

				fbsParameter.value(value);
				parameters[String(fbsParameter.name())] = value.value();

				break;
			}

			case FbsValue.Integer32: {
				const value = new FbsInteger32();

				fbsParameter.value(value);
				parameters[String(fbsParameter.name())] = value.value();

				break;
			}

			case FbsValue.Double: {
				const value = new FbsDouble();

				fbsParameter.value(value);
				parameters[String(fbsParameter.name())] = value.value();

				break;
			}

			case FbsValue.String: {
				const value = new FbsString();

				fbsParameter.value(value);
				parameters[String(fbsParameter.name())] = value.value();

				break;
			}

			case FbsValue.Integer32Array: {
				const value = new FbsInteger32Array();

				fbsParameter.value(value);
				parameters[String(fbsParameter.name())] = value.valueArray();

				break;
			}
		}
	}

	return parameters;
}

export function parseRtpCodecParameters(
	data: FbsRtpCodecParameters
): RtpCodecParameters {
	const parameters = parseParameters(data);

	let rtcpFeedback: RtcpFeedback[] = [];

	if (data.rtcpFeedbackLength() > 0) {
		rtcpFeedback = fbsUtils.parseVector(
			data,
			'rtcpFeedback',
			parseRtcpFeedback
		);
	}

	return {
		mimeType: data.mimeType()!,
		payloadType: data.payloadType(),
		clockRate: data.clockRate(),
		channels: data.channels() ?? undefined,
		parameters,
		rtcpFeedback,
	};
}

export function rtpHeaderExtensionUriFromFbs(
	uri: FbsRtpHeaderExtensionUri
): RtpHeaderExtensionUri {
	switch (uri) {
		case FbsRtpHeaderExtensionUri.Mid: {
			return 'urn:ietf:params:rtp-hdrext:sdes:mid';
		}

		case FbsRtpHeaderExtensionUri.RtpStreamId: {
			return 'urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id';
		}

		case FbsRtpHeaderExtensionUri.RepairRtpStreamId: {
			return 'urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id';
		}

		case FbsRtpHeaderExtensionUri.FrameMarkingDraft07: {
			return 'http://tools.ietf.org/html/draft-ietf-avtext-framemarking-07';
		}

		case FbsRtpHeaderExtensionUri.FrameMarking: {
			return 'urn:ietf:params:rtp-hdrext:framemarking';
		}

		case FbsRtpHeaderExtensionUri.AudioLevel: {
			return 'urn:ietf:params:rtp-hdrext:ssrc-audio-level';
		}

		case FbsRtpHeaderExtensionUri.VideoOrientation: {
			return 'urn:3gpp:video-orientation';
		}

		case FbsRtpHeaderExtensionUri.TimeOffset: {
			return 'urn:ietf:params:rtp-hdrext:toffset';
		}

		case FbsRtpHeaderExtensionUri.TransportWideCcDraft01: {
			return 'http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01';
		}

		case FbsRtpHeaderExtensionUri.AbsSendTime: {
			return 'http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time';
		}

		case FbsRtpHeaderExtensionUri.AbsCaptureTime: {
			return 'http://www.webrtc.org/experiments/rtp-hdrext/abs-capture-time';
		}

		case FbsRtpHeaderExtensionUri.PlayoutDelay: {
			return 'http://www.webrtc.org/experiments/rtp-hdrext/playout-delay';
		}

		case FbsRtpHeaderExtensionUri.DependencyDescriptor: {
			return 'https://aomediacodec.github.io/av1-rtp-spec/#dependency-descriptor-rtp-header-extension';
		}
	}
}

export function rtpHeaderExtensionUriToFbs(
	uri: RtpHeaderExtensionUri
): FbsRtpHeaderExtensionUri {
	switch (uri) {
		case 'urn:ietf:params:rtp-hdrext:sdes:mid': {
			return FbsRtpHeaderExtensionUri.Mid;
		}

		case 'urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id': {
			return FbsRtpHeaderExtensionUri.RtpStreamId;
		}

		case 'urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id': {
			return FbsRtpHeaderExtensionUri.RepairRtpStreamId;
		}

		case 'http://tools.ietf.org/html/draft-ietf-avtext-framemarking-07': {
			return FbsRtpHeaderExtensionUri.FrameMarkingDraft07;
		}

		case 'urn:ietf:params:rtp-hdrext:framemarking': {
			return FbsRtpHeaderExtensionUri.FrameMarking;
		}

		case 'urn:ietf:params:rtp-hdrext:ssrc-audio-level': {
			return FbsRtpHeaderExtensionUri.AudioLevel;
		}

		case 'urn:3gpp:video-orientation': {
			return FbsRtpHeaderExtensionUri.VideoOrientation;
		}

		case 'urn:ietf:params:rtp-hdrext:toffset': {
			return FbsRtpHeaderExtensionUri.TimeOffset;
		}

		case 'http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01': {
			return FbsRtpHeaderExtensionUri.TransportWideCcDraft01;
		}

		case 'http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time': {
			return FbsRtpHeaderExtensionUri.AbsSendTime;
		}

		case 'http://www.webrtc.org/experiments/rtp-hdrext/abs-capture-time': {
			return FbsRtpHeaderExtensionUri.AbsCaptureTime;
		}

		case 'http://www.webrtc.org/experiments/rtp-hdrext/playout-delay': {
			return FbsRtpHeaderExtensionUri.PlayoutDelay;
		}

		case 'https://aomediacodec.github.io/av1-rtp-spec/#dependency-descriptor-rtp-header-extension': {
			return FbsRtpHeaderExtensionUri.DependencyDescriptor;
		}

		default: {
			throw new TypeError(`invalid RtpHeaderExtensionUri: ${uri}`);
		}
	}
}

export function parseRtpHeaderExtensionParameters(
	data: FbsRtpHeaderExtensionParameters
): RtpHeaderExtensionParameters {
	return {
		uri: rtpHeaderExtensionUriFromFbs(data.uri()),
		id: data.id(),
		encrypt: data.encrypt(),
		parameters: parseParameters(data),
	};
}

export function parseRtpEncodingParameters(
	data: FbsRtpEncodingParameters
): RtpEncodingParameters {
	return {
		ssrc: data.ssrc() ?? undefined,
		rid: data.rid() ?? undefined,
		codecPayloadType:
			data.codecPayloadType() !== null ? data.codecPayloadType()! : undefined,
		rtx: data.rtx() ? { ssrc: data.rtx()!.ssrc() } : undefined,
		dtx: data.dtx(),
		scalabilityMode: data.scalabilityMode() ?? undefined,
		maxBitrate: data.maxBitrate() !== null ? data.maxBitrate()! : undefined,
	};
}

export function parseRtpParameters(data: FbsRtpParameters): RtpParameters {
	const codecs = fbsUtils.parseVector(data, 'codecs', parseRtpCodecParameters);

	let headerExtensions: RtpHeaderExtensionParameters[] = [];

	if (data.headerExtensionsLength() > 0) {
		headerExtensions = fbsUtils.parseVector(
			data,
			'headerExtensions',
			parseRtpHeaderExtensionParameters
		);
	}

	let encodings: RtpEncodingParameters[] = [];

	if (data.encodingsLength() > 0) {
		encodings = fbsUtils.parseVector(
			data,
			'encodings',
			parseRtpEncodingParameters
		);
	}

	let rtcp: RtcpParameters | undefined;

	if (data.rtcp()) {
		const fbsRtcp = data.rtcp()!;

		rtcp = {
			cname: fbsRtcp.cname() ?? undefined,
			reducedSize: fbsRtcp.reducedSize(),
		};
	}

	return {
		mid: data.mid() ?? undefined,
		codecs,
		headerExtensions,
		encodings,
		rtcp,
	};
}
