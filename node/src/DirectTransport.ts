import { Logger } from './Logger';
import { EnhancedEventEmitter } from './enhancedEvents';
import type {
	DirectTransport,
	DirectTransportDump,
	DirectTransportStat,
	DirectTransportEvents,
	DirectTransportObserver,
	DirectTransportObserverEvents,
} from './DirectTransportTypes';
import type { Transport, BaseTransportDump } from './TransportTypes';
import {
	TransportImpl,
	TransportConstructorOptions,
	parseBaseTransportDump,
	parseBaseTransportStats,
	parseTransportTraceEventData,
} from './Transport';
import type { SctpParameters } from './sctpParametersTypes';
import type { AppData } from './types';
import { UnsupportedError } from './errors';
import { Event, Notification } from './fbs/notification';
import * as FbsDirectTransport from './fbs/direct-transport';
import * as FbsTransport from './fbs/transport';
import * as FbsNotification from './fbs/notification';
import * as FbsRequest from './fbs/request';

type DirectTransportConstructorOptions<DirectTransportAppData> =
	TransportConstructorOptions<DirectTransportAppData> & {
		data: DirectTransportData;
	};

export type DirectTransportData = {
	sctpParameters?: SctpParameters;
};

const logger = new Logger('DirectTransport');

export class DirectTransportImpl<
		DirectTransportAppData extends AppData = AppData,
	>
	extends TransportImpl<
		DirectTransportAppData,
		DirectTransportEvents,
		DirectTransportObserver
	>
	implements Transport, DirectTransport
{
	// DirectTransport data.
	// eslint-disable-next-line no-unused-private-class-members
	readonly #data: DirectTransportData;

	constructor(
		options: DirectTransportConstructorOptions<DirectTransportAppData>
	) {
		const observer: DirectTransportObserver =
			new EnhancedEventEmitter<DirectTransportObserverEvents>();

		super(options, observer);

		logger.debug('constructor()');

		this.#data = {
			// Nothing.
		};

		this.handleWorkerNotifications();
		this.handleListenerError();
	}

	get observer(): DirectTransportObserver {
		return super.observer;
	}

	close(): void {
		if (this.closed) {
			return;
		}

		super.close();
	}

	routerClosed(): void {
		if (this.closed) {
			return;
		}

		super.routerClosed();
	}

	async dump(): Promise<DirectTransportDump> {
		logger.debug('dump()');

		const response = await this.channel.request(
			FbsRequest.Method.TRANSPORT_DUMP,
			undefined,
			undefined,
			this.internal.transportId
		);

		/* Decode Response. */
		const data = new FbsDirectTransport.DumpResponse();

		response.body(data);

		return parseDirectTransportDumpResponse(data);
	}

	async getStats(): Promise<DirectTransportStat[]> {
		logger.debug('getStats()');

		const response = await this.channel.request(
			FbsRequest.Method.TRANSPORT_GET_STATS,
			undefined,
			undefined,
			this.internal.transportId
		);

		/* Decode Response. */
		const data = new FbsDirectTransport.GetStatsResponse();

		response.body(data);

		return [parseGetStatsResponse(data)];
	}

	// eslint-disable-next-line @typescript-eslint/require-await
	async connect(): Promise<void> {
		logger.debug('connect()');
	}

	// eslint-disable-next-line @typescript-eslint/no-unused-vars, @typescript-eslint/require-await
	async setMaxIncomingBitrate(bitrate: number): Promise<void> {
		throw new UnsupportedError(
			'setMaxIncomingBitrate() not implemented in DirectTransport'
		);
	}

	// eslint-disable-next-line @typescript-eslint/no-unused-vars, @typescript-eslint/require-await
	async setMaxOutgoingBitrate(bitrate: number): Promise<void> {
		throw new UnsupportedError(
			'setMaxOutgoingBitrate() not implemented in DirectTransport'
		);
	}

	// eslint-disable-next-line @typescript-eslint/no-unused-vars, @typescript-eslint/require-await
	async setMinOutgoingBitrate(bitrate: number): Promise<void> {
		throw new UnsupportedError(
			'setMinOutgoingBitrate() not implemented in DirectTransport'
		);
	}

	sendRtcp(rtcpPacket: Buffer): void {
		if (!Buffer.isBuffer(rtcpPacket)) {
			throw new TypeError('rtcpPacket must be a Buffer');
		}

		const builder = this.channel.bufferBuilder;
		const dataOffset = FbsTransport.SendRtcpNotification.createDataVector(
			builder,
			rtcpPacket
		);
		const notificationOffset =
			FbsTransport.SendRtcpNotification.createSendRtcpNotification(
				builder,
				dataOffset
			);

		this.channel.notify(
			FbsNotification.Event.TRANSPORT_SEND_RTCP,
			FbsNotification.Body.Transport_SendRtcpNotification,
			notificationOffset,
			this.internal.transportId
		);
	}

	private handleWorkerNotifications(): void {
		this.channel.on(
			this.internal.transportId,
			(event: Event, data?: Notification) => {
				switch (event) {
					case Event.TRANSPORT_TRACE: {
						const notification = new FbsTransport.TraceNotification();

						data!.body(notification);

						const trace = parseTransportTraceEventData(notification);

						this.safeEmit('trace', trace);

						// Emit observer event.
						this.observer.safeEmit('trace', trace);

						break;
					}

					case Event.DIRECTTRANSPORT_RTCP: {
						if (this.closed) {
							break;
						}

						const notification = new FbsDirectTransport.RtcpNotification();

						data!.body(notification);

						this.safeEmit('rtcp', Buffer.from(notification.dataArray()!));

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

export function parseDirectTransportDumpResponse(
	binary: FbsDirectTransport.DumpResponse
): BaseTransportDump {
	return parseBaseTransportDump(binary.base()!);
}

function parseGetStatsResponse(
	binary: FbsDirectTransport.GetStatsResponse
): DirectTransportStat {
	const base = parseBaseTransportStats(binary.base()!);

	return {
		...base,
		type: 'direct-transport',
	};
}
