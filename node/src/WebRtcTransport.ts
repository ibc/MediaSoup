import * as flatbuffers from 'flatbuffers';
import { Logger } from './Logger';
import { EnhancedEventEmitter } from './enhancedEvents';
import type {
	WebRtcTransport,
	IceParameters,
	IceCandidate,
	DtlsParameters,
	FingerprintAlgorithm,
	DtlsFingerprint,
	IceRole,
	IceState,
	IceCandidateType,
	IceCandidateTcpType,
	DtlsRole,
	DtlsState,
	WebRtcTransportDump,
	WebRtcTransportStat,
	WebRtcTransportEvents,
	WebRtcTransportObserver,
	WebRtcTransportObserverEvents,
} from './WebRtcTransportTypes';
import type { Transport, TransportTuple, SctpState } from './TransportTypes';
import {
	TransportImpl,
	TransportConstructorOptions,
	parseSctpState,
	parseBaseTransportDump,
	parseBaseTransportStats,
	parseProtocol,
	parseTransportTraceEventData,
	parseTuple,
} from './Transport';
import type { SctpParameters } from './sctpParametersTypes';
import type { AppData } from './types';
import * as fbsUtils from './fbsUtils';
import { Event, Notification } from './fbs/notification';
import * as FbsRequest from './fbs/request';
import * as FbsTransport from './fbs/transport';
import * as FbsWebRtcTransport from './fbs/web-rtc-transport';
import { DtlsState as FbsDtlsState } from './fbs/web-rtc-transport/dtls-state';
import { DtlsRole as FbsDtlsRole } from './fbs/web-rtc-transport/dtls-role';
import { FingerprintAlgorithm as FbsFingerprintAlgorithm } from './fbs/web-rtc-transport/fingerprint-algorithm';
import { IceState as FbsIceState } from './fbs/web-rtc-transport/ice-state';
import { IceRole as FbsIceRole } from './fbs/web-rtc-transport/ice-role';
import { IceCandidateType as FbsIceCandidateType } from './fbs/web-rtc-transport/ice-candidate-type';
import { IceCandidateTcpType as FbsIceCandidateTcpType } from './fbs/web-rtc-transport/ice-candidate-tcp-type';

type WebRtcTransportConstructorOptions<WebRtcTransportAppData> =
	TransportConstructorOptions<WebRtcTransportAppData> & {
		data: WebRtcTransportData;
	};

export type WebRtcTransportData = {
	iceRole: 'controlled';
	iceParameters: IceParameters;
	iceCandidates: IceCandidate[];
	iceState: IceState;
	iceSelectedTuple?: TransportTuple;
	dtlsParameters: DtlsParameters;
	dtlsState: DtlsState;
	dtlsRemoteCert?: string;
	sctpParameters?: SctpParameters;
	sctpState?: SctpState;
};

const logger = new Logger('WebRtcTransport');

export class WebRtcTransportImpl<
		WebRtcTransportAppData extends AppData = AppData,
	>
	extends TransportImpl<
		WebRtcTransportAppData,
		WebRtcTransportEvents,
		WebRtcTransportObserver
	>
	implements Transport, WebRtcTransport
{
	// WebRtcTransport data.
	readonly #data: WebRtcTransportData;

	/**
	 * @private
	 */
	constructor(
		options: WebRtcTransportConstructorOptions<WebRtcTransportAppData>
	) {
		const observer: WebRtcTransportObserver =
			new EnhancedEventEmitter<WebRtcTransportObserverEvents>();

		super(options, observer);

		logger.debug('constructor()');

		const { data } = options;

		this.#data = {
			iceRole: data.iceRole,
			iceParameters: data.iceParameters,
			iceCandidates: data.iceCandidates,
			iceState: data.iceState,
			iceSelectedTuple: data.iceSelectedTuple,
			dtlsParameters: data.dtlsParameters,
			dtlsState: data.dtlsState,
			dtlsRemoteCert: data.dtlsRemoteCert,
			sctpParameters: data.sctpParameters,
			sctpState: data.sctpState,
		};

		this.handleWorkerNotifications();
		this.handleListenerError();
	}

	get observer(): WebRtcTransportObserver {
		return super.observer;
	}

	get iceRole(): 'controlled' {
		return this.#data.iceRole;
	}

	get iceParameters(): IceParameters {
		return this.#data.iceParameters;
	}

	get iceCandidates(): IceCandidate[] {
		return this.#data.iceCandidates;
	}

	get iceState(): IceState {
		return this.#data.iceState;
	}

	get iceSelectedTuple(): TransportTuple | undefined {
		return this.#data.iceSelectedTuple;
	}

	get dtlsParameters(): DtlsParameters {
		return this.#data.dtlsParameters;
	}

	get dtlsState(): DtlsState {
		return this.#data.dtlsState;
	}

	get dtlsRemoteCert(): string | undefined {
		return this.#data.dtlsRemoteCert;
	}

	get sctpParameters(): SctpParameters | undefined {
		return this.#data.sctpParameters;
	}

	get sctpState(): SctpState | undefined {
		return this.#data.sctpState;
	}

	close(): void {
		if (this.closed) {
			return;
		}

		this.#data.iceState = 'closed';
		this.#data.iceSelectedTuple = undefined;
		this.#data.dtlsState = 'closed';

		if (this.#data.sctpState) {
			this.#data.sctpState = 'closed';
		}

		super.close();
	}

	routerClosed(): void {
		if (this.closed) {
			return;
		}

		this.#data.iceState = 'closed';
		this.#data.iceSelectedTuple = undefined;
		this.#data.dtlsState = 'closed';

		if (this.#data.sctpState) {
			this.#data.sctpState = 'closed';
		}

		super.routerClosed();
	}

	listenServerClosed(): void {
		if (this.closed) {
			return;
		}

		this.#data.iceState = 'closed';
		this.#data.iceSelectedTuple = undefined;
		this.#data.dtlsState = 'closed';

		if (this.#data.sctpState) {
			this.#data.sctpState = 'closed';
		}

		super.listenServerClosed();
	}

	async dump(): Promise<WebRtcTransportDump> {
		logger.debug('dump()');

		const response = await this.channel.request(
			FbsRequest.Method.TRANSPORT_DUMP,
			undefined,
			undefined,
			this.internal.transportId
		);

		/* Decode Response. */
		const data = new FbsWebRtcTransport.DumpResponse();

		response.body(data);

		return parseWebRtcTransportDumpResponse(data);
	}

	async getStats(): Promise<WebRtcTransportStat[]> {
		logger.debug('getStats()');

		const response = await this.channel.request(
			FbsRequest.Method.TRANSPORT_GET_STATS,
			undefined,
			undefined,
			this.internal.transportId
		);

		/* Decode Response. */
		const data = new FbsWebRtcTransport.GetStatsResponse();

		response.body(data);

		return [parseGetStatsResponse(data)];
	}

	async connect({
		dtlsParameters,
	}: {
		dtlsParameters: DtlsParameters;
	}): Promise<void> {
		logger.debug('connect()');

		const requestOffset = createConnectRequest({
			builder: this.channel.bufferBuilder,
			dtlsParameters,
		});

		// Wait for response.
		const response = await this.channel.request(
			FbsRequest.Method.WEBRTCTRANSPORT_CONNECT,
			FbsRequest.Body.WebRtcTransport_ConnectRequest,
			requestOffset,
			this.internal.transportId
		);

		/* Decode Response. */
		const data = new FbsWebRtcTransport.ConnectResponse();

		response.body(data);

		// Update data.
		this.#data.dtlsParameters.role = dtlsRoleFromFbs(data.dtlsLocalRole());
	}

	async restartIce(): Promise<IceParameters> {
		logger.debug('restartIce()');

		const response = await this.channel.request(
			FbsRequest.Method.TRANSPORT_RESTART_ICE,
			undefined,
			undefined,
			this.internal.transportId
		);

		/* Decode Response. */
		const restartIceResponse = new FbsTransport.RestartIceResponse();

		response.body(restartIceResponse);

		const iceParameters = {
			usernameFragment: restartIceResponse.usernameFragment()!,
			password: restartIceResponse.password()!,
			iceLite: restartIceResponse.iceLite(),
		};

		this.#data.iceParameters = iceParameters;

		return iceParameters;
	}

	private handleWorkerNotifications(): void {
		this.channel.on(
			this.internal.transportId,
			(event: Event, data?: Notification) => {
				switch (event) {
					case Event.WEBRTCTRANSPORT_ICE_STATE_CHANGE: {
						const notification =
							new FbsWebRtcTransport.IceStateChangeNotification();

						data!.body(notification);

						const iceState = iceStateFromFbs(notification.iceState());

						this.#data.iceState = iceState;

						this.safeEmit('icestatechange', iceState);

						// Emit observer event.
						this.observer.safeEmit('icestatechange', iceState);

						break;
					}

					case Event.WEBRTCTRANSPORT_ICE_SELECTED_TUPLE_CHANGE: {
						const notification =
							new FbsWebRtcTransport.IceSelectedTupleChangeNotification();

						data!.body(notification);

						const iceSelectedTuple = parseTuple(notification.tuple()!);

						this.#data.iceSelectedTuple = iceSelectedTuple;

						this.safeEmit('iceselectedtuplechange', iceSelectedTuple);

						// Emit observer event.
						this.observer.safeEmit('iceselectedtuplechange', iceSelectedTuple);

						break;
					}

					case Event.WEBRTCTRANSPORT_DTLS_STATE_CHANGE: {
						const notification =
							new FbsWebRtcTransport.DtlsStateChangeNotification();

						data!.body(notification);

						const dtlsState = dtlsStateFromFbs(notification.dtlsState());

						this.#data.dtlsState = dtlsState;

						if (dtlsState === 'connected') {
							this.#data.dtlsRemoteCert = notification.remoteCert()!;
						}

						this.safeEmit('dtlsstatechange', dtlsState);

						// Emit observer event.
						this.observer.safeEmit('dtlsstatechange', dtlsState);

						break;
					}

					case Event.TRANSPORT_SCTP_STATE_CHANGE: {
						const notification = new FbsTransport.SctpStateChangeNotification();

						data!.body(notification);

						const sctpState = parseSctpState(notification.sctpState());

						this.#data.sctpState = sctpState;

						this.safeEmit('sctpstatechange', sctpState);

						// Emit observer event.
						this.observer.safeEmit('sctpstatechange', sctpState);

						break;
					}

					case Event.TRANSPORT_TRACE: {
						const notification = new FbsTransport.TraceNotification();

						data!.body(notification);

						const trace = parseTransportTraceEventData(notification);

						this.safeEmit('trace', trace);

						// Emit observer event.
						this.observer.safeEmit('trace', trace);

						break;
					}

					default: {
						logger.error(`ignoring unknown event "${event}"`);
					}
				}
			}
		);
	}

	private handleListenerError(): void {
		this.on('listenererror', (eventName, error) => {
			logger.error(
				`event listener threw an error [eventName:${eventName}]:`,
				error
			);
		});
	}
}

function iceStateFromFbs(fbsIceState: FbsIceState): IceState {
	switch (fbsIceState) {
		case FbsIceState.NEW: {
			return 'new';
		}

		case FbsIceState.CONNECTED: {
			return 'connected';
		}

		case FbsIceState.COMPLETED: {
			return 'completed';
		}

		case FbsIceState.DISCONNECTED: {
			return 'disconnected';
		}
	}
}

function iceRoleFromFbs(role: FbsIceRole): IceRole {
	switch (role) {
		case FbsIceRole.CONTROLLED: {
			return 'controlled';
		}

		case FbsIceRole.CONTROLLING: {
			return 'controlling';
		}
	}
}

function iceCandidateTypeFromFbs(type: FbsIceCandidateType): IceCandidateType {
	switch (type) {
		case FbsIceCandidateType.HOST: {
			return 'host';
		}
	}
}

function iceCandidateTcpTypeFromFbs(
	type: FbsIceCandidateTcpType
): IceCandidateTcpType {
	switch (type) {
		case FbsIceCandidateTcpType.PASSIVE: {
			return 'passive';
		}
	}
}

function dtlsStateFromFbs(fbsDtlsState: FbsDtlsState): DtlsState {
	switch (fbsDtlsState) {
		case FbsDtlsState.NEW: {
			return 'new';
		}

		case FbsDtlsState.CONNECTING: {
			return 'connecting';
		}

		case FbsDtlsState.CONNECTED: {
			return 'connected';
		}

		case FbsDtlsState.FAILED: {
			return 'failed';
		}

		case FbsDtlsState.CLOSED: {
			return 'closed';
		}
	}
}

function dtlsRoleFromFbs(role: FbsDtlsRole): DtlsRole {
	switch (role) {
		case FbsDtlsRole.AUTO: {
			return 'auto';
		}

		case FbsDtlsRole.CLIENT: {
			return 'client';
		}

		case FbsDtlsRole.SERVER: {
			return 'server';
		}
	}
}

function fingerprintAlgorithmsFromFbs(
	algorithm: FbsFingerprintAlgorithm
): FingerprintAlgorithm {
	switch (algorithm) {
		case FbsFingerprintAlgorithm.SHA1: {
			return 'sha-1';
		}

		case FbsFingerprintAlgorithm.SHA224: {
			return 'sha-224';
		}

		case FbsFingerprintAlgorithm.SHA256: {
			return 'sha-256';
		}

		case FbsFingerprintAlgorithm.SHA384: {
			return 'sha-384';
		}

		case FbsFingerprintAlgorithm.SHA512: {
			return 'sha-512';
		}
	}
}

function fingerprintAlgorithmToFbs(
	algorithm: FingerprintAlgorithm
): FbsFingerprintAlgorithm {
	switch (algorithm) {
		case 'sha-1': {
			return FbsFingerprintAlgorithm.SHA1;
		}

		case 'sha-224': {
			return FbsFingerprintAlgorithm.SHA224;
		}

		case 'sha-256': {
			return FbsFingerprintAlgorithm.SHA256;
		}

		case 'sha-384': {
			return FbsFingerprintAlgorithm.SHA384;
		}

		case 'sha-512': {
			return FbsFingerprintAlgorithm.SHA512;
		}

		default: {
			throw new TypeError(`invalid FingerprintAlgorithm: ${algorithm}`);
		}
	}
}

function dtlsRoleToFbs(role: DtlsRole): FbsDtlsRole {
	switch (role) {
		case 'auto': {
			return FbsDtlsRole.AUTO;
		}

		case 'client': {
			return FbsDtlsRole.CLIENT;
		}

		case 'server': {
			return FbsDtlsRole.SERVER;
		}

		default: {
			throw new TypeError(`invalid DtlsRole: ${role}`);
		}
	}
}

export function parseWebRtcTransportDumpResponse(
	binary: FbsWebRtcTransport.DumpResponse
): WebRtcTransportDump {
	// Retrieve BaseTransportDump.
	const baseTransportDump = parseBaseTransportDump(binary.base()!);
	// Retrieve ICE candidates.
	const iceCandidates = fbsUtils.parseVector<IceCandidate>(
		binary,
		'iceCandidates',
		parseIceCandidate
	);
	// Retrieve ICE parameters.
	const iceParameters = parseIceParameters(binary.iceParameters()!);
	// Retrieve DTLS parameters.
	const dtlsParameters = parseDtlsParameters(binary.dtlsParameters()!);

	return {
		...baseTransportDump,
		sctpParameters: baseTransportDump.sctpParameters,
		sctpState: baseTransportDump.sctpState,
		iceRole: 'controlled',
		iceParameters: iceParameters,
		iceCandidates: iceCandidates,
		iceState: iceStateFromFbs(binary.iceState()),
		dtlsParameters: dtlsParameters,
		dtlsState: dtlsStateFromFbs(binary.dtlsState()),
	};
}

function createConnectRequest({
	builder,
	dtlsParameters,
}: {
	builder: flatbuffers.Builder;
	dtlsParameters: DtlsParameters;
}): number {
	// Serialize DtlsParameters. This can throw.
	const dtlsParametersOffset = serializeDtlsParameters(builder, dtlsParameters);

	return FbsWebRtcTransport.ConnectRequest.createConnectRequest(
		builder,
		dtlsParametersOffset
	);
}

function parseGetStatsResponse(
	binary: FbsWebRtcTransport.GetStatsResponse
): WebRtcTransportStat {
	const base = parseBaseTransportStats(binary.base()!);

	return {
		...base,
		type: 'webrtc-transport',
		iceRole: iceRoleFromFbs(binary.iceRole()),
		iceState: iceStateFromFbs(binary.iceState()),
		iceSelectedTuple: binary.iceSelectedTuple()
			? parseTuple(binary.iceSelectedTuple()!)
			: undefined,
		dtlsState: dtlsStateFromFbs(binary.dtlsState()),
	};
}

function parseIceCandidate(
	binary: FbsWebRtcTransport.IceCandidate
): IceCandidate {
	return {
		foundation: binary.foundation()!,
		priority: binary.priority(),
		ip: binary.address()!,
		address: binary.address()!,
		protocol: parseProtocol(binary.protocol()),
		port: binary.port(),
		type: iceCandidateTypeFromFbs(binary.type()),
		tcpType:
			binary.tcpType() === null
				? undefined
				: iceCandidateTcpTypeFromFbs(binary.tcpType()!),
	};
}

function parseIceParameters(
	binary: FbsWebRtcTransport.IceParameters
): IceParameters {
	return {
		usernameFragment: binary.usernameFragment()!,
		password: binary.password()!,
		iceLite: binary.iceLite(),
	};
}

function parseDtlsParameters(
	binary: FbsWebRtcTransport.DtlsParameters
): DtlsParameters {
	const fingerprints: DtlsFingerprint[] = [];

	for (let i = 0; i < binary.fingerprintsLength(); ++i) {
		const fbsFingerprint = binary.fingerprints(i)!;
		const fingerPrint: DtlsFingerprint = {
			algorithm: fingerprintAlgorithmsFromFbs(fbsFingerprint.algorithm()),
			value: fbsFingerprint.value()!,
		};

		fingerprints.push(fingerPrint);
	}

	return {
		fingerprints: fingerprints,
		role: binary.role() === null ? undefined : dtlsRoleFromFbs(binary.role()),
	};
}

function serializeDtlsParameters(
	builder: flatbuffers.Builder,
	dtlsParameters: DtlsParameters
): number {
	const fingerprints: number[] = [];

	for (const fingerprint of dtlsParameters.fingerprints) {
		const algorithm = fingerprintAlgorithmToFbs(fingerprint.algorithm);
		const valueOffset = builder.createString(fingerprint.value);
		const fingerprintOffset = FbsWebRtcTransport.Fingerprint.createFingerprint(
			builder,
			algorithm,
			valueOffset
		);

		fingerprints.push(fingerprintOffset);
	}

	const fingerprintsOffset =
		FbsWebRtcTransport.DtlsParameters.createFingerprintsVector(
			builder,
			fingerprints
		);

	const role =
		dtlsParameters.role !== undefined
			? dtlsRoleToFbs(dtlsParameters.role)
			: FbsWebRtcTransport.DtlsRole.AUTO;

	return FbsWebRtcTransport.DtlsParameters.createDtlsParameters(
		builder,
		fingerprintsOffset,
		role
	);
}
