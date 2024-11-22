import {
	RtpStreamRecvStats,
	RtpStreamSendStats,
	BaseRtpStreamStats,
	BitrateByLayer,
} from './rtpStreamStatsTypes';
import * as FbsRtpStream from './fbs/rtp-stream';
import * as FbsRtpParameters from './fbs/rtp-parameters';

export function parseRtpStreamStats(
	binary: FbsRtpStream.Stats
): RtpStreamRecvStats | RtpStreamSendStats {
	if (binary.dataType() === FbsRtpStream.StatsData.RecvStats) {
		return parseRtpStreamRecvStats(binary);
	} else {
		return parseSendStreamStats(binary);
	}
}

export function parseRtpStreamRecvStats(
	binary: FbsRtpStream.Stats
): RtpStreamRecvStats {
	const recvStats = new FbsRtpStream.RecvStats();
	const baseStats = new FbsRtpStream.BaseStats();

	binary.data(recvStats);
	recvStats.base()!.data(baseStats);

	const base = parseBaseStreamStats(baseStats);

	return {
		...base,
		type: 'inbound-rtp',
		jitter: recvStats.jitter(),
		byteCount: Number(recvStats.byteCount()),
		packetCount: Number(recvStats.packetCount()),
		bitrate: Number(recvStats.bitrate()),
		bitrateByLayer: parseBitrateByLayer(recvStats),
	};
}

export function parseSendStreamStats(
	binary: FbsRtpStream.Stats
): RtpStreamSendStats {
	const sendStats = new FbsRtpStream.SendStats();
	const baseStats = new FbsRtpStream.BaseStats();

	binary.data(sendStats);
	sendStats.base()!.data(baseStats);

	const base = parseBaseStreamStats(baseStats);

	return {
		...base,
		type: 'outbound-rtp',
		byteCount: Number(sendStats.byteCount()),
		packetCount: Number(sendStats.packetCount()),
		bitrate: Number(sendStats.bitrate()),
	};
}

function parseBaseStreamStats(
	binary: FbsRtpStream.BaseStats
): BaseRtpStreamStats {
	return {
		timestamp: Number(binary.timestamp()),
		ssrc: binary.ssrc(),
		rtxSsrc: binary.rtxSsrc() ?? undefined,
		rid: binary.rid() ?? undefined,
		kind:
			binary.kind() === FbsRtpParameters.MediaKind.AUDIO ? 'audio' : 'video',
		mimeType: binary.mimeType()!,
		packetsLost: Number(binary.packetsLost()),
		fractionLost: Number(binary.fractionLost()),
		packetsDiscarded: Number(binary.packetsDiscarded()),
		packetsRetransmitted: Number(binary.packetsRetransmitted()),
		packetsRepaired: Number(binary.packetsRepaired()),
		nackCount: Number(binary.nackCount()),
		nackPacketCount: Number(binary.nackPacketCount()),
		pliCount: Number(binary.pliCount()),
		firCount: Number(binary.firCount()),
		score: binary.score(),
		roundTripTime: binary.roundTripTime(),
		rtxPacketsDiscarded: binary.rtxPacketsDiscarded()
			? Number(binary.rtxPacketsDiscarded())
			: undefined,
	};
}

function parseBitrateByLayer(binary: FbsRtpStream.RecvStats): BitrateByLayer {
	if (binary.bitrateByLayerLength() === 0) {
		return {};
	}

	const bitRateByLayer: { [key: string]: number } = {};

	for (let i = 0; i < binary.bitrateByLayerLength(); ++i) {
		const layer: string = binary.bitrateByLayer(i)!.layer()!;
		const bitrate = binary.bitrateByLayer(i)!.bitrate();

		bitRateByLayer[layer] = Number(bitrate);
	}

	return bitRateByLayer;
}
