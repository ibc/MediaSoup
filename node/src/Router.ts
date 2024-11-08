import { Logger } from './Logger';
import { EnhancedEventEmitter } from './enhancedEvents';
import * as ortc from './ortc';
import { InvalidStateError } from './errors';
import { Channel } from './Channel';
import {
	RouterInterface,
	PipeToRouterOptions,
	PipeToRouterResult,
	PipeTransportPair,
	RouterDump,
	RouterEvents,
	RouterObserver,
	RouterObserverEvents,
} from './RouterInterface';
import {
	TransportInterface,
	TransportListenIp,
	TransportProtocol,
} from './TransportInterface';
import { portRangeToFbs, socketFlagsToFbs } from './Transport';
import {
	WebRtcTransportInterface,
	WebRtcTransportOptions,
} from './WebRtcTransportInterface';
import {
	WebRtcTransport,
	parseWebRtcTransportDumpResponse,
} from './WebRtcTransport';
import {
	PlainTransportInterface,
	PlainTransportOptions,
} from './PlainTransportInterface';
import {
	PlainTransport,
	parsePlainTransportDumpResponse,
} from './PlainTransport';
import {
	PipeTransportInterface,
	PipeTransportOptions,
} from './PipeTransportInterface';
import { PipeTransport, parsePipeTransportDumpResponse } from './PipeTransport';
import {
	DirectTransportInterface,
	DirectTransportOptions,
} from './DirectTransportInterface';
import {
	DirectTransport,
	parseDirectTransportDumpResponse,
} from './DirectTransport';
import { ProducerInterface } from './ProducerInterface';
import { ConsumerInterface } from './ConsumerInterface';
import { DataProducerInterface } from './DataProducerInterface';
import { DataConsumerInterface } from './DataConsumerInterface';
import { RtpObserverInterface } from './RtpObserverInterface';
import {
	ActiveSpeakerObserverInterface,
	ActiveSpeakerObserverOptions,
} from './ActiveSpeakerObserverInterface';
import { ActiveSpeakerObserver } from './ActiveSpeakerObserver';
import {
	AudioLevelObserverInterface,
	AudioLevelObserverOptions,
} from './AudioLevelObserverInterface';
import { AudioLevelObserver } from './AudioLevelObserver';
import { RtpCapabilities } from './RtpParameters';
import { cryptoSuiteToFbs } from './SrtpParameters';
import { AppData } from './types';
import {
	clone,
	generateUUIDv4,
	parseVector,
	parseStringStringVector,
	parseStringStringArrayVector,
} from './utils';
import * as FbsActiveSpeakerObserver from './fbs/active-speaker-observer';
import * as FbsAudioLevelObserver from './fbs/audio-level-observer';
import * as FbsRequest from './fbs/request';
import * as FbsWorker from './fbs/worker';
import * as FbsRouter from './fbs/router';
import * as FbsTransport from './fbs/transport';
import { Protocol as FbsTransportProtocol } from './fbs/transport/protocol';
import * as FbsWebRtcTransport from './fbs/web-rtc-transport';
import * as FbsPlainTransport from './fbs/plain-transport';
import * as FbsPipeTransport from './fbs/pipe-transport';
import * as FbsDirectTransport from './fbs/direct-transport';
import * as FbsSctpParameters from './fbs/sctp-parameters';

export type RouterInternal = {
	routerId: string;
};

type RouterData = {
	rtpCapabilities: RtpCapabilities;
};

const logger = new Logger('Router');

export class Router<RouterAppData extends AppData = AppData>
	extends EnhancedEventEmitter<RouterEvents>
	implements RouterInterface
{
	// Internal data.
	readonly #internal: RouterInternal;

	// Router data.
	readonly #data: RouterData;

	// Channel instance.
	readonly #channel: Channel;

	// Closed flag.
	#closed = false;

	// Custom app data.
	#appData: RouterAppData;

	// Transports map.
	readonly #transports: Map<string, TransportInterface> = new Map();

	// Producers map.
	readonly #producers: Map<string, ProducerInterface> = new Map();

	// RtpObservers map.
	readonly #rtpObservers: Map<string, RtpObserverInterface> = new Map();

	// DataProducers map.
	readonly #dataProducers: Map<string, DataProducerInterface> = new Map();

	// Map of PipeTransport pair Promises indexed by the id of the Router in
	// which pipeToRouter() was called.
	readonly #mapRouterPairPipeTransportPairPromise: Map<
		string,
		Promise<PipeTransportPair>
	> = new Map();

	// Observer instance.
	readonly #observer: RouterObserver =
		new EnhancedEventEmitter<RouterObserverEvents>();

	/**
	 * @private
	 */
	constructor({
		internal,
		data,
		channel,
		appData,
	}: {
		internal: RouterInternal;
		data: RouterData;
		channel: Channel;
		appData?: RouterAppData;
	}) {
		super();

		logger.debug('constructor()');

		this.#internal = internal;
		this.#data = data;
		this.#channel = channel;
		this.#appData = appData ?? ({} as RouterAppData);
	}

	/**
	 * Router id.
	 */
	get id(): string {
		return this.#internal.routerId;
	}

	/**
	 * Whether the Router is closed.
	 */
	get closed(): boolean {
		return this.#closed;
	}

	/**
	 * RTP capabilities of the Router.
	 */
	get rtpCapabilities(): RtpCapabilities {
		return this.#data.rtpCapabilities;
	}

	/**
	 * App custom data.
	 */
	get appData(): RouterAppData {
		return this.#appData;
	}

	/**
	 * App custom data setter.
	 */
	set appData(appData: RouterAppData) {
		this.#appData = appData;
	}

	/**
	 * Observer.
	 */
	get observer(): RouterObserver {
		return this.#observer;
	}

	/**
	 * @private
	 * Just for testing purposes.
	 */
	get transportsForTesting(): Map<string, TransportInterface> {
		return this.#transports;
	}

	/**
	 * Close the Router.
	 */
	close(): void {
		if (this.#closed) {
			return;
		}

		logger.debug('close()');

		this.#closed = true;

		const requestOffset = new FbsWorker.CloseRouterRequestT(
			this.#internal.routerId
		).pack(this.#channel.bufferBuilder);

		this.#channel
			.request(
				FbsRequest.Method.WORKER_CLOSE_ROUTER,
				FbsRequest.Body.Worker_CloseRouterRequest,
				requestOffset
			)
			.catch(() => {});

		// Close every Transport.
		for (const transport of this.#transports.values()) {
			transport.routerClosed();
		}
		this.#transports.clear();

		// Clear the Producers map.
		this.#producers.clear();

		// Close every RtpObserver.
		for (const rtpObserver of this.#rtpObservers.values()) {
			rtpObserver.routerClosed();
		}
		this.#rtpObservers.clear();

		// Clear the DataProducers map.
		this.#dataProducers.clear();

		this.emit('@close');

		// Emit observer event.
		this.#observer.safeEmit('close');
	}

	/**
	 * Worker was closed.
	 *
	 * @private
	 */
	workerClosed(): void {
		if (this.#closed) {
			return;
		}

		logger.debug('workerClosed()');

		this.#closed = true;

		// Close every Transport.
		for (const transport of this.#transports.values()) {
			transport.routerClosed();
		}
		this.#transports.clear();

		// Clear the Producers map.
		this.#producers.clear();

		// Close every RtpObserver.
		for (const rtpObserver of this.#rtpObservers.values()) {
			rtpObserver.routerClosed();
		}
		this.#rtpObservers.clear();

		// Clear the DataProducers map.
		this.#dataProducers.clear();

		this.safeEmit('workerclose');

		// Emit observer event.
		this.#observer.safeEmit('close');
	}

	/**
	 * Dump Router.
	 */
	async dump(): Promise<RouterDump> {
		logger.debug('dump()');

		// Send the request and wait for the response.
		const response = await this.#channel.request(
			FbsRequest.Method.ROUTER_DUMP,
			undefined,
			undefined,
			this.#internal.routerId
		);

		/* Decode Response. */
		const dump = new FbsRouter.DumpResponse();

		response.body(dump);

		return parseRouterDumpResponse(dump);
	}

	/**
	 * Create a WebRtcTransport.
	 */
	async createWebRtcTransport<
		WebRtcTransportAppData extends AppData = AppData,
	>({
		webRtcServer,
		listenInfos,
		listenIps,
		port,
		enableUdp,
		enableTcp,
		preferUdp = false,
		preferTcp = false,
		initialAvailableOutgoingBitrate = 600000,
		enableSctp = false,
		numSctpStreams = { OS: 1024, MIS: 1024 },
		maxSctpMessageSize = 262144,
		sctpSendBufferSize = 262144,
		iceConsentTimeout = 30,
		appData,
	}: WebRtcTransportOptions<WebRtcTransportAppData>): Promise<
		WebRtcTransportInterface<WebRtcTransportAppData>
	> {
		logger.debug('createWebRtcTransport()');

		if (
			!webRtcServer &&
			!Array.isArray(listenInfos) &&
			!Array.isArray(listenIps)
		) {
			throw new TypeError(
				'missing webRtcServer, listenInfos and listenIps (one of them is mandatory)'
			);
		} else if (webRtcServer && listenInfos && listenIps) {
			throw new TypeError(
				'only one of webRtcServer, listenInfos and listenIps must be given'
			);
		} else if (
			numSctpStreams &&
			(typeof numSctpStreams.OS !== 'number' ||
				typeof numSctpStreams.MIS !== 'number')
		) {
			throw new TypeError('if given, numSctpStreams must contain OS and MIS');
		} else if (appData && typeof appData !== 'object') {
			throw new TypeError('if given, appData must be an object');
		}

		// If webRtcServer is given, then do not force default values for enableUdp
		// and enableTcp. Otherwise set them if unset.
		if (webRtcServer) {
			enableUdp ??= true;
			enableTcp ??= true;
		} else {
			enableUdp ??= true;
			enableTcp ??= false;
		}

		// Convert deprecated TransportListenIps to TransportListenInfos.
		if (listenIps) {
			// Normalize IP strings to TransportListenIp objects.
			listenIps = listenIps.map(listenIp => {
				if (typeof listenIp === 'string') {
					return { ip: listenIp };
				} else {
					return listenIp;
				}
			});

			listenInfos = [];

			const orderedProtocols: TransportProtocol[] = [];

			if (enableUdp && (preferUdp || !enableTcp || !preferTcp)) {
				orderedProtocols.push('udp');

				if (enableTcp) {
					orderedProtocols.push('tcp');
				}
			} else if (enableTcp && ((preferTcp && !preferUdp) || !enableUdp)) {
				orderedProtocols.push('tcp');

				if (enableUdp) {
					orderedProtocols.push('udp');
				}
			}

			for (const listenIp of listenIps as TransportListenIp[]) {
				for (const protocol of orderedProtocols) {
					listenInfos.push({
						protocol: protocol,
						ip: listenIp.ip,
						announcedAddress: listenIp.announcedIp,
						port: port,
					});
				}
			}
		}

		const transportId = generateUUIDv4();

		/* Build Request. */
		let webRtcTransportListenServer:
			| FbsWebRtcTransport.ListenServerT
			| undefined;
		let webRtcTransportListenIndividual:
			| FbsWebRtcTransport.ListenIndividualT
			| undefined;

		if (webRtcServer) {
			webRtcTransportListenServer = new FbsWebRtcTransport.ListenServerT(
				webRtcServer.id
			);
		} else {
			const fbsListenInfos: FbsTransport.ListenInfoT[] = [];

			for (const listenInfo of listenInfos!) {
				fbsListenInfos.push(
					new FbsTransport.ListenInfoT(
						listenInfo.protocol === 'udp'
							? FbsTransportProtocol.UDP
							: FbsTransportProtocol.TCP,
						listenInfo.ip,
						listenInfo.announcedAddress ?? listenInfo.announcedIp,
						listenInfo.port,
						portRangeToFbs(listenInfo.portRange),
						socketFlagsToFbs(listenInfo.flags),
						listenInfo.sendBufferSize,
						listenInfo.recvBufferSize
					)
				);
			}

			webRtcTransportListenIndividual =
				new FbsWebRtcTransport.ListenIndividualT(fbsListenInfos);
		}

		const baseTransportOptions = new FbsTransport.OptionsT(
			undefined /* direct */,
			undefined /* maxMessageSize */,
			initialAvailableOutgoingBitrate,
			enableSctp,
			new FbsSctpParameters.NumSctpStreamsT(
				numSctpStreams.OS,
				numSctpStreams.MIS
			),
			maxSctpMessageSize,
			sctpSendBufferSize,
			true /* isDataChannel */
		);

		const webRtcTransportOptions =
			new FbsWebRtcTransport.WebRtcTransportOptionsT(
				baseTransportOptions,
				webRtcServer
					? FbsWebRtcTransport.Listen.ListenServer
					: FbsWebRtcTransport.Listen.ListenIndividual,
				webRtcServer
					? webRtcTransportListenServer
					: webRtcTransportListenIndividual,
				enableUdp,
				enableTcp,
				preferUdp,
				preferTcp,
				iceConsentTimeout
			);

		const requestOffset = new FbsRouter.CreateWebRtcTransportRequestT(
			transportId,
			webRtcTransportOptions
		).pack(this.#channel.bufferBuilder);

		const response = await this.#channel.request(
			webRtcServer
				? FbsRequest.Method.ROUTER_CREATE_WEBRTCTRANSPORT_WITH_SERVER
				: FbsRequest.Method.ROUTER_CREATE_WEBRTCTRANSPORT,
			FbsRequest.Body.Router_CreateWebRtcTransportRequest,
			requestOffset,
			this.#internal.routerId
		);

		/* Decode Response. */
		const data = new FbsWebRtcTransport.DumpResponse();

		response.body(data);

		const webRtcTransportData = parseWebRtcTransportDumpResponse(data);

		const transport: WebRtcTransportInterface<WebRtcTransportAppData> =
			new WebRtcTransport({
				internal: {
					...this.#internal,
					transportId: transportId,
				},
				data: webRtcTransportData,
				channel: this.#channel,
				appData,
				getRouterRtpCapabilities: (): RtpCapabilities =>
					this.#data.rtpCapabilities,
				getProducerById: (producerId: string): ProducerInterface | undefined =>
					this.#producers.get(producerId),
				getDataProducerById: (
					dataProducerId: string
				): DataProducerInterface | undefined =>
					this.#dataProducers.get(dataProducerId),
			});

		this.#transports.set(transport.id, transport);
		transport.on('@close', () => this.#transports.delete(transport.id));
		transport.on('@listenserverclose', () =>
			this.#transports.delete(transport.id)
		);
		transport.on('@newproducer', (producer: ProducerInterface) =>
			this.#producers.set(producer.id, producer)
		);
		transport.on('@producerclose', (producer: ProducerInterface) =>
			this.#producers.delete(producer.id)
		);
		transport.on('@newdataproducer', (dataProducer: DataProducerInterface) =>
			this.#dataProducers.set(dataProducer.id, dataProducer)
		);
		transport.on('@dataproducerclose', (dataProducer: DataProducerInterface) =>
			this.#dataProducers.delete(dataProducer.id)
		);

		// Emit observer event.
		this.#observer.safeEmit('newtransport', transport);

		if (webRtcServer) {
			webRtcServer.handleWebRtcTransport(transport);
		}

		return transport;
	}

	/**
	 * Create a PlainTransport.
	 */
	async createPlainTransport<PlainTransportAppData extends AppData = AppData>({
		listenInfo,
		rtcpListenInfo,
		listenIp,
		port,
		rtcpMux = true,
		comedia = false,
		enableSctp = false,
		numSctpStreams = { OS: 1024, MIS: 1024 },
		maxSctpMessageSize = 262144,
		sctpSendBufferSize = 262144,
		enableSrtp = false,
		srtpCryptoSuite = 'AES_CM_128_HMAC_SHA1_80',
		appData,
	}: PlainTransportOptions<PlainTransportAppData>): Promise<
		PlainTransportInterface<PlainTransportAppData>
	> {
		logger.debug('createPlainTransport()');

		if (!listenInfo && !listenIp) {
			throw new TypeError(
				'missing listenInfo and listenIp (one of them is mandatory)'
			);
		} else if (listenInfo && listenIp) {
			throw new TypeError('only one of listenInfo and listenIp must be given');
		} else if (appData && typeof appData !== 'object') {
			throw new TypeError('if given, appData must be an object');
		}

		// If rtcpMux is enabled, ignore rtcpListenInfo.
		if (rtcpMux && rtcpListenInfo) {
			logger.warn(
				'createPlainTransport() | ignoring rtcpMux since rtcpListenInfo is given'
			);

			rtcpMux = false;
		}

		// Convert deprecated TransportListenIps to TransportListenInfos.
		if (listenIp) {
			// Normalize IP string to TransportListenIp object.
			if (typeof listenIp === 'string') {
				listenIp = { ip: listenIp };
			}

			listenInfo = {
				protocol: 'udp',
				ip: listenIp.ip,
				announcedAddress: listenIp.announcedIp,
				port: port,
			};
		}

		const transportId = generateUUIDv4();

		/* Build Request. */
		const baseTransportOptions = new FbsTransport.OptionsT(
			undefined /* direct */,
			undefined /* maxMessageSize */,
			undefined /* initialAvailableOutgoingBitrate */,
			enableSctp,
			new FbsSctpParameters.NumSctpStreamsT(
				numSctpStreams.OS,
				numSctpStreams.MIS
			),
			maxSctpMessageSize,
			sctpSendBufferSize,
			false /* isDataChannel */
		);

		const plainTransportOptions = new FbsPlainTransport.PlainTransportOptionsT(
			baseTransportOptions,
			new FbsTransport.ListenInfoT(
				listenInfo!.protocol === 'udp'
					? FbsTransportProtocol.UDP
					: FbsTransportProtocol.TCP,
				listenInfo!.ip,
				listenInfo!.announcedAddress ?? listenInfo!.announcedIp,
				listenInfo!.port,
				portRangeToFbs(listenInfo!.portRange),
				socketFlagsToFbs(listenInfo!.flags),
				listenInfo!.sendBufferSize,
				listenInfo!.recvBufferSize
			),
			rtcpListenInfo
				? new FbsTransport.ListenInfoT(
						rtcpListenInfo.protocol === 'udp'
							? FbsTransportProtocol.UDP
							: FbsTransportProtocol.TCP,
						rtcpListenInfo.ip,
						rtcpListenInfo.announcedAddress ?? rtcpListenInfo.announcedIp,
						rtcpListenInfo.port,
						portRangeToFbs(rtcpListenInfo.portRange),
						socketFlagsToFbs(rtcpListenInfo.flags),
						rtcpListenInfo.sendBufferSize,
						rtcpListenInfo.recvBufferSize
					)
				: undefined,
			rtcpMux,
			comedia,
			enableSrtp,
			cryptoSuiteToFbs(srtpCryptoSuite)
		);

		const requestOffset = new FbsRouter.CreatePlainTransportRequestT(
			transportId,
			plainTransportOptions
		).pack(this.#channel.bufferBuilder);

		const response = await this.#channel.request(
			FbsRequest.Method.ROUTER_CREATE_PLAINTRANSPORT,
			FbsRequest.Body.Router_CreatePlainTransportRequest,
			requestOffset,
			this.#internal.routerId
		);

		/* Decode Response. */
		const data = new FbsPlainTransport.DumpResponse();

		response.body(data);

		const plainTransportData = parsePlainTransportDumpResponse(data);

		const transport: PlainTransportInterface<PlainTransportAppData> =
			new PlainTransport({
				internal: {
					...this.#internal,
					transportId: transportId,
				},
				data: plainTransportData,
				channel: this.#channel,
				appData,
				getRouterRtpCapabilities: (): RtpCapabilities =>
					this.#data.rtpCapabilities,
				getProducerById: (producerId: string): ProducerInterface | undefined =>
					this.#producers.get(producerId),
				getDataProducerById: (
					dataProducerId: string
				): DataProducerInterface | undefined =>
					this.#dataProducers.get(dataProducerId),
			});

		this.#transports.set(transport.id, transport);
		transport.on('@close', () => this.#transports.delete(transport.id));
		transport.on('@listenserverclose', () =>
			this.#transports.delete(transport.id)
		);
		transport.on('@newproducer', (producer: ProducerInterface) =>
			this.#producers.set(producer.id, producer)
		);
		transport.on('@producerclose', (producer: ProducerInterface) =>
			this.#producers.delete(producer.id)
		);
		transport.on('@newdataproducer', (dataProducer: DataProducerInterface) =>
			this.#dataProducers.set(dataProducer.id, dataProducer)
		);
		transport.on('@dataproducerclose', (dataProducer: DataProducerInterface) =>
			this.#dataProducers.delete(dataProducer.id)
		);

		// Emit observer event.
		this.#observer.safeEmit('newtransport', transport);

		return transport;
	}

	/**
	 * Create a PipeTransport.
	 */
	async createPipeTransport<PipeTransportAppData extends AppData = AppData>({
		listenInfo,
		listenIp,
		port,
		enableSctp = false,
		numSctpStreams = { OS: 1024, MIS: 1024 },
		maxSctpMessageSize = 268435456,
		sctpSendBufferSize = 268435456,
		enableRtx = false,
		enableSrtp = false,
		appData,
	}: PipeTransportOptions<PipeTransportAppData>): Promise<
		PipeTransportInterface<PipeTransportAppData>
	> {
		logger.debug('createPipeTransport()');

		if (!listenInfo && !listenIp) {
			throw new TypeError(
				'missing listenInfo and listenIp (one of them is mandatory)'
			);
		} else if (listenInfo && listenIp) {
			throw new TypeError('only one of listenInfo and listenIp must be given');
		} else if (appData && typeof appData !== 'object') {
			throw new TypeError('if given, appData must be an object');
		}

		// Convert deprecated TransportListenIps to TransportListenInfos.
		if (listenIp) {
			// Normalize IP string to TransportListenIp object.
			if (typeof listenIp === 'string') {
				listenIp = { ip: listenIp };
			}

			listenInfo = {
				protocol: 'udp',
				ip: listenIp.ip,
				announcedAddress: listenIp.announcedIp,
				port: port,
			};
		}

		const transportId = generateUUIDv4();

		/* Build Request. */
		const baseTransportOptions = new FbsTransport.OptionsT(
			undefined /* direct */,
			undefined /* maxMessageSize */,
			undefined /* initialAvailableOutgoingBitrate */,
			enableSctp,
			new FbsSctpParameters.NumSctpStreamsT(
				numSctpStreams.OS,
				numSctpStreams.MIS
			),
			maxSctpMessageSize,
			sctpSendBufferSize,
			false /* isDataChannel */
		);

		const pipeTransportOptions = new FbsPipeTransport.PipeTransportOptionsT(
			baseTransportOptions,
			new FbsTransport.ListenInfoT(
				listenInfo!.protocol === 'udp'
					? FbsTransportProtocol.UDP
					: FbsTransportProtocol.TCP,
				listenInfo!.ip,
				listenInfo!.announcedAddress ?? listenInfo!.announcedIp,
				listenInfo!.port,
				portRangeToFbs(listenInfo!.portRange),
				socketFlagsToFbs(listenInfo!.flags),
				listenInfo!.sendBufferSize,
				listenInfo!.recvBufferSize
			),
			enableRtx,
			enableSrtp
		);

		const requestOffset = new FbsRouter.CreatePipeTransportRequestT(
			transportId,
			pipeTransportOptions
		).pack(this.#channel.bufferBuilder);

		const response = await this.#channel.request(
			FbsRequest.Method.ROUTER_CREATE_PIPETRANSPORT,
			FbsRequest.Body.Router_CreatePipeTransportRequest,
			requestOffset,
			this.#internal.routerId
		);

		/* Decode Response. */
		const data = new FbsPipeTransport.DumpResponse();

		response.body(data);

		const plainTransportData = parsePipeTransportDumpResponse(data);

		const transport: PipeTransportInterface<PipeTransportAppData> =
			new PipeTransport({
				internal: {
					...this.#internal,
					transportId,
				},
				data: plainTransportData,
				channel: this.#channel,
				appData,
				getRouterRtpCapabilities: (): RtpCapabilities =>
					this.#data.rtpCapabilities,
				getProducerById: (producerId: string): ProducerInterface | undefined =>
					this.#producers.get(producerId),
				getDataProducerById: (
					dataProducerId: string
				): DataProducerInterface | undefined =>
					this.#dataProducers.get(dataProducerId),
			});

		this.#transports.set(transport.id, transport);
		transport.on('@close', () => this.#transports.delete(transport.id));
		transport.on('@listenserverclose', () =>
			this.#transports.delete(transport.id)
		);
		transport.on('@newproducer', (producer: ProducerInterface) =>
			this.#producers.set(producer.id, producer)
		);
		transport.on('@producerclose', (producer: ProducerInterface) =>
			this.#producers.delete(producer.id)
		);
		transport.on('@newdataproducer', (dataProducer: DataProducerInterface) =>
			this.#dataProducers.set(dataProducer.id, dataProducer)
		);
		transport.on('@dataproducerclose', (dataProducer: DataProducerInterface) =>
			this.#dataProducers.delete(dataProducer.id)
		);

		// Emit observer event.
		this.#observer.safeEmit('newtransport', transport);

		return transport;
	}

	/**
	 * Create a DirectTransport.
	 */
	async createDirectTransport<DirectTransportAppData extends AppData = AppData>(
		{
			maxMessageSize = 262144,
			appData,
		}: DirectTransportOptions<DirectTransportAppData> = {
			maxMessageSize: 262144,
		}
	): Promise<DirectTransportInterface<DirectTransportAppData>> {
		logger.debug('createDirectTransport()');

		if (typeof maxMessageSize !== 'number' || maxMessageSize < 0) {
			throw new TypeError('if given, maxMessageSize must be a positive number');
		} else if (appData && typeof appData !== 'object') {
			throw new TypeError('if given, appData must be an object');
		}

		const transportId = generateUUIDv4();

		/* Build Request. */
		const baseTransportOptions = new FbsTransport.OptionsT(
			true /* direct */,
			maxMessageSize,
			undefined /* initialAvailableOutgoingBitrate */,
			undefined /* enableSctp */,
			undefined /* numSctpStreams */,
			undefined /* maxSctpMessageSize */,
			undefined /* sctpSendBufferSize */,
			undefined /* isDataChannel */
		);

		const directTransportOptions =
			new FbsDirectTransport.DirectTransportOptionsT(baseTransportOptions);

		const requestOffset = new FbsRouter.CreateDirectTransportRequestT(
			transportId,
			directTransportOptions
		).pack(this.#channel.bufferBuilder);

		const response = await this.#channel.request(
			FbsRequest.Method.ROUTER_CREATE_DIRECTTRANSPORT,
			FbsRequest.Body.Router_CreateDirectTransportRequest,
			requestOffset,
			this.#internal.routerId
		);

		/* Decode Response. */
		const data = new FbsDirectTransport.DumpResponse();

		response.body(data);

		const directTransportData = parseDirectTransportDumpResponse(data);

		const transport: DirectTransportInterface<DirectTransportAppData> =
			new DirectTransport({
				internal: {
					...this.#internal,
					transportId: transportId,
				},
				data: directTransportData,
				channel: this.#channel,
				appData,
				getRouterRtpCapabilities: (): RtpCapabilities =>
					this.#data.rtpCapabilities,
				getProducerById: (producerId: string): ProducerInterface | undefined =>
					this.#producers.get(producerId),
				getDataProducerById: (
					dataProducerId: string
				): DataProducerInterface | undefined =>
					this.#dataProducers.get(dataProducerId),
			});

		this.#transports.set(transport.id, transport);
		transport.on('@close', () => this.#transports.delete(transport.id));
		transport.on('@listenserverclose', () =>
			this.#transports.delete(transport.id)
		);
		transport.on('@newproducer', (producer: ProducerInterface) =>
			this.#producers.set(producer.id, producer)
		);
		transport.on('@producerclose', (producer: ProducerInterface) =>
			this.#producers.delete(producer.id)
		);
		transport.on('@newdataproducer', (dataProducer: DataProducerInterface) =>
			this.#dataProducers.set(dataProducer.id, dataProducer)
		);
		transport.on('@dataproducerclose', (dataProducer: DataProducerInterface) =>
			this.#dataProducers.delete(dataProducer.id)
		);

		// Emit observer event.
		this.#observer.safeEmit('newtransport', transport);

		return transport;
	}

	/**
	 * Pipes the given Producer or DataProducer into another Router in same host.
	 */
	async pipeToRouter({
		producerId,
		dataProducerId,
		router,
		listenInfo,
		listenIp,
		enableSctp = true,
		numSctpStreams = { OS: 1024, MIS: 1024 },
		enableRtx = false,
		enableSrtp = false,
	}: PipeToRouterOptions): Promise<PipeToRouterResult> {
		logger.debug('pipeToRouter()');

		if (!listenInfo && !listenIp) {
			listenInfo = {
				protocol: 'udp',
				ip: '127.0.0.1',
			};
		}

		if (listenInfo && listenIp) {
			throw new TypeError('only one of listenInfo and listenIp must be given');
		} else if (!producerId && !dataProducerId) {
			throw new TypeError('missing producerId or dataProducerId');
		} else if (producerId && dataProducerId) {
			throw new TypeError('just producerId or dataProducerId can be given');
		} else if (!router) {
			throw new TypeError('Router not found');
		} else if (router === this) {
			throw new TypeError('cannot use this Router as destination');
		}

		// Convert deprecated TransportListenIps to TransportListenInfos.
		if (listenIp) {
			// Normalize IP string to TransportListenIp object.
			if (typeof listenIp === 'string') {
				listenIp = { ip: listenIp };
			}

			listenInfo = {
				protocol: 'udp',
				ip: listenIp.ip,
				announcedAddress: listenIp.announcedIp,
			};
		}

		let producer: ProducerInterface | undefined;
		let dataProducer: DataProducerInterface | undefined;

		if (producerId) {
			producer = this.#producers.get(producerId);

			if (!producer) {
				throw new TypeError('Producer not found');
			}
		} else if (dataProducerId) {
			dataProducer = this.#dataProducers.get(dataProducerId);

			if (!dataProducer) {
				throw new TypeError('DataProducer not found');
			}
		}

		const pipeTransportPairKey = router.id;
		let pipeTransportPairPromise =
			this.#mapRouterPairPipeTransportPairPromise.get(pipeTransportPairKey);
		let pipeTransportPair: PipeTransportPair;
		let localPipeTransport: PipeTransportInterface;
		let remotePipeTransport: PipeTransportInterface;

		if (pipeTransportPairPromise) {
			pipeTransportPair = await pipeTransportPairPromise;
			localPipeTransport = pipeTransportPair[this.id];
			remotePipeTransport = pipeTransportPair[router.id];
		} else {
			pipeTransportPairPromise = new Promise((resolve, reject) => {
				Promise.all([
					this.createPipeTransport({
						listenInfo: listenInfo!,
						enableSctp,
						numSctpStreams,
						enableRtx,
						enableSrtp,
					}),
					router.createPipeTransport({
						listenInfo: listenInfo!,
						enableSctp,
						numSctpStreams,
						enableRtx,
						enableSrtp,
					}),
				])
					.then(pipeTransports => {
						localPipeTransport = pipeTransports[0];
						remotePipeTransport = pipeTransports[1];
					})
					.then(() => {
						return Promise.all([
							localPipeTransport.connect({
								ip: remotePipeTransport.tuple.localAddress,
								port: remotePipeTransport.tuple.localPort,
								srtpParameters: remotePipeTransport.srtpParameters,
							}),
							remotePipeTransport.connect({
								ip: localPipeTransport.tuple.localAddress,
								port: localPipeTransport.tuple.localPort,
								srtpParameters: localPipeTransport.srtpParameters,
							}),
						]);
					})
					.then(() => {
						localPipeTransport.observer.on('close', () => {
							remotePipeTransport.close();
							this.#mapRouterPairPipeTransportPairPromise.delete(
								pipeTransportPairKey
							);
						});

						remotePipeTransport.observer.on('close', () => {
							localPipeTransport.close();
							this.#mapRouterPairPipeTransportPairPromise.delete(
								pipeTransportPairKey
							);
						});

						resolve({
							[this.id]: localPipeTransport,
							[router.id]: remotePipeTransport,
						});
					})
					.catch(error => {
						logger.error(
							'pipeToRouter() | error creating PipeTransport pair:',
							error
						);

						if (localPipeTransport) {
							localPipeTransport.close();
						}

						if (remotePipeTransport) {
							remotePipeTransport.close();
						}

						reject(error instanceof Error ? error : new Error(String(error)));
					});
			});

			this.#mapRouterPairPipeTransportPairPromise.set(
				pipeTransportPairKey,
				pipeTransportPairPromise
			);

			router.addPipeTransportPair(this.id, pipeTransportPairPromise);

			await pipeTransportPairPromise;
		}

		if (producer) {
			let pipeConsumer: ConsumerInterface | undefined;
			let pipeProducer: ProducerInterface | undefined;

			try {
				pipeConsumer = await localPipeTransport!.consume({
					producerId: producerId!,
				});

				pipeProducer = await remotePipeTransport!.produce({
					id: producer.id,
					kind: pipeConsumer.kind,
					rtpParameters: pipeConsumer.rtpParameters,
					paused: pipeConsumer.producerPaused,
					appData: producer.appData,
				});

				// Ensure that the producer has not been closed in the meanwhile.
				if (producer.closed) {
					throw new InvalidStateError('original Producer closed');
				}

				// Ensure that producer.paused has not changed in the meanwhile and, if
				// so, sync the pipeProducer.
				if (pipeProducer.paused !== producer.paused) {
					if (producer.paused) {
						await pipeProducer.pause();
					} else {
						await pipeProducer.resume();
					}
				}

				// Pipe events from the pipe Consumer to the pipe Producer.
				pipeConsumer.observer.on('close', () => pipeProducer!.close());
				pipeConsumer.observer.on('pause', () => void pipeProducer!.pause());
				pipeConsumer.observer.on('resume', () => void pipeProducer!.resume());

				// Pipe events from the pipe Producer to the pipe Consumer.
				pipeProducer.observer.on('close', () => pipeConsumer!.close());

				return { pipeConsumer, pipeProducer };
			} catch (error) {
				logger.error(
					'pipeToRouter() | error creating pipe Consumer/Producer pair:',
					error as Error
				);

				if (pipeConsumer) {
					pipeConsumer.close();
				}

				if (pipeProducer) {
					pipeProducer.close();
				}

				throw error;
			}
		} else if (dataProducer) {
			let pipeDataConsumer: DataConsumerInterface | undefined;
			let pipeDataProducer: DataProducerInterface | undefined;

			try {
				pipeDataConsumer = await localPipeTransport!.consumeData({
					dataProducerId: dataProducerId!,
				});

				pipeDataProducer = await remotePipeTransport!.produceData({
					id: dataProducer.id,
					sctpStreamParameters: pipeDataConsumer.sctpStreamParameters,
					label: pipeDataConsumer.label,
					protocol: pipeDataConsumer.protocol,
					appData: dataProducer.appData,
				});

				// Ensure that the dataProducer has not been closed in the meanwhile.
				if (dataProducer.closed) {
					throw new InvalidStateError('original DataProducer closed');
				}

				// Pipe events from the pipe DataConsumer to the pipe DataProducer.
				pipeDataConsumer.observer.on('close', () => pipeDataProducer!.close());

				// Pipe events from the pipe DataProducer to the pipe DataConsumer.
				pipeDataProducer.observer.on('close', () => pipeDataConsumer!.close());

				return { pipeDataConsumer, pipeDataProducer };
			} catch (error) {
				logger.error(
					'pipeToRouter() | error creating pipe DataConsumer/DataProducer pair:',
					error as Error
				);

				pipeDataConsumer?.close();
				pipeDataProducer?.close();

				throw error;
			}
		} else {
			throw new Error('internal error');
		}
	}

	/**
	 * @private
	 */
	addPipeTransportPair(
		pipeTransportPairKey: string,
		pipeTransportPairPromise: Promise<PipeTransportPair>
	): void {
		if (this.#mapRouterPairPipeTransportPairPromise.has(pipeTransportPairKey)) {
			throw new Error(
				'given pipeTransportPairKey already exists in this Router'
			);
		}

		this.#mapRouterPairPipeTransportPairPromise.set(
			pipeTransportPairKey,
			pipeTransportPairPromise
		);

		pipeTransportPairPromise
			.then(pipeTransportPair => {
				const localPipeTransport = pipeTransportPair[this.id];

				// NOTE: No need to do any other cleanup here since that is done by the
				// Router calling this method on us.
				localPipeTransport.observer.on('close', () => {
					this.#mapRouterPairPipeTransportPairPromise.delete(
						pipeTransportPairKey
					);
				});
			})
			.catch(() => {
				this.#mapRouterPairPipeTransportPairPromise.delete(
					pipeTransportPairKey
				);
			});
	}

	/**
	 * Create an ActiveSpeakerObserver
	 */
	async createActiveSpeakerObserver<
		ActiveSpeakerObserverAppData extends AppData = AppData,
	>({
		interval = 300,
		appData,
	}: ActiveSpeakerObserverOptions<ActiveSpeakerObserverAppData> = {}): Promise<
		ActiveSpeakerObserverInterface<ActiveSpeakerObserverAppData>
	> {
		logger.debug('createActiveSpeakerObserver()');

		if (typeof interval !== 'number') {
			throw new TypeError('if given, interval must be an number');
		} else if (appData && typeof appData !== 'object') {
			throw new TypeError('if given, appData must be an object');
		}

		const rtpObserverId = generateUUIDv4();

		/* Build Request. */
		const activeRtpObserverOptions =
			new FbsActiveSpeakerObserver.ActiveSpeakerObserverOptionsT(interval);

		const requestOffset = new FbsRouter.CreateActiveSpeakerObserverRequestT(
			rtpObserverId,
			activeRtpObserverOptions
		).pack(this.#channel.bufferBuilder);

		await this.#channel.request(
			FbsRequest.Method.ROUTER_CREATE_ACTIVESPEAKEROBSERVER,
			FbsRequest.Body.Router_CreateActiveSpeakerObserverRequest,
			requestOffset,
			this.#internal.routerId
		);

		const activeSpeakerObserver: ActiveSpeakerObserverInterface<ActiveSpeakerObserverAppData> =
			new ActiveSpeakerObserver({
				internal: {
					...this.#internal,
					rtpObserverId: rtpObserverId,
				},
				channel: this.#channel,
				appData,
				getProducerById: (producerId: string): ProducerInterface | undefined =>
					this.#producers.get(producerId),
			});

		this.#rtpObservers.set(activeSpeakerObserver.id, activeSpeakerObserver);
		activeSpeakerObserver.on('@close', () => {
			this.#rtpObservers.delete(activeSpeakerObserver.id);
		});

		// Emit observer event.
		this.#observer.safeEmit('newrtpobserver', activeSpeakerObserver);

		return activeSpeakerObserver;
	}

	/**
	 * Create an AudioLevelObserver.
	 */
	async createAudioLevelObserver<
		AudioLevelObserverAppData extends AppData = AppData,
	>({
		maxEntries = 1,
		threshold = -80,
		interval = 1000,
		appData,
	}: AudioLevelObserverOptions<AudioLevelObserverAppData> = {}): Promise<
		AudioLevelObserverInterface<AudioLevelObserverAppData>
	> {
		logger.debug('createAudioLevelObserver()');

		if (typeof maxEntries !== 'number' || maxEntries <= 0) {
			throw new TypeError('if given, maxEntries must be a positive number');
		} else if (
			typeof threshold !== 'number' ||
			threshold < -127 ||
			threshold > 0
		) {
			throw new TypeError(
				'if given, threshole must be a negative number greater than -127'
			);
		} else if (typeof interval !== 'number') {
			throw new TypeError('if given, interval must be an number');
		} else if (appData && typeof appData !== 'object') {
			throw new TypeError('if given, appData must be an object');
		}

		const rtpObserverId = generateUUIDv4();

		/* Build Request. */
		const audioLevelObserverOptions =
			new FbsAudioLevelObserver.AudioLevelObserverOptionsT(
				maxEntries,
				threshold,
				interval
			);

		const requestOffset = new FbsRouter.CreateAudioLevelObserverRequestT(
			rtpObserverId,
			audioLevelObserverOptions
		).pack(this.#channel.bufferBuilder);

		await this.#channel.request(
			FbsRequest.Method.ROUTER_CREATE_AUDIOLEVELOBSERVER,
			FbsRequest.Body.Router_CreateAudioLevelObserverRequest,
			requestOffset,
			this.#internal.routerId
		);

		const audioLevelObserver: AudioLevelObserverInterface<AudioLevelObserverAppData> =
			new AudioLevelObserver({
				internal: {
					...this.#internal,
					rtpObserverId: rtpObserverId,
				},
				channel: this.#channel,
				appData,
				getProducerById: (producerId: string): ProducerInterface | undefined =>
					this.#producers.get(producerId),
			});

		this.#rtpObservers.set(audioLevelObserver.id, audioLevelObserver);
		audioLevelObserver.on('@close', () => {
			this.#rtpObservers.delete(audioLevelObserver.id);
		});

		// Emit observer event.
		this.#observer.safeEmit('newrtpobserver', audioLevelObserver);

		return audioLevelObserver;
	}

	/**
	 * Check whether the given RTP capabilities can consume the given Producer.
	 */
	canConsume({
		producerId,
		rtpCapabilities,
	}: {
		producerId: string;
		rtpCapabilities: RtpCapabilities;
	}): boolean {
		const producer = this.#producers.get(producerId);

		if (!producer) {
			logger.error(`canConsume() | Producer with id "${producerId}" not found`);

			return false;
		}

		// Clone given RTP capabilities to not modify input data.
		const clonedRtpCapabilities = clone<RtpCapabilities>(rtpCapabilities);

		try {
			return ortc.canConsume(
				producer.consumableRtpParameters,
				clonedRtpCapabilities
			);
		} catch (error) {
			logger.error(`canConsume() | unexpected error: ${error}`);

			return false;
		}
	}
}

export function parseRouterDumpResponse(
	binary: FbsRouter.DumpResponse
): RouterDump {
	return {
		id: binary.id()!,
		transportIds: parseVector(binary, 'transportIds'),
		rtpObserverIds: parseVector(binary, 'rtpObserverIds'),
		mapProducerIdConsumerIds: parseStringStringArrayVector(
			binary,
			'mapProducerIdConsumerIds'
		),
		mapConsumerIdProducerId: parseStringStringVector(
			binary,
			'mapConsumerIdProducerId'
		),
		mapProducerIdObserverIds: parseStringStringArrayVector(
			binary,
			'mapProducerIdObserverIds'
		),
		mapDataProducerIdDataConsumerIds: parseStringStringArrayVector(
			binary,
			'mapDataProducerIdDataConsumerIds'
		),
		mapDataConsumerIdDataProducerId: parseStringStringVector(
			binary,
			'mapDataConsumerIdDataProducerId'
		),
	};
}
