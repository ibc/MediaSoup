import { EnhancedEventEmitter } from './enhancedEvents';
import {
	TransportInterface,
	BaseTransportDump,
	BaseTransportStats,
	TransportEvents,
	TransportObserverEvents,
} from './TransportTypes';
import { AppData } from './types';

export type DirectTransportOptions<
	DirectTransportAppData extends AppData = AppData,
> = {
	/**
	 * Maximum allowed size for direct messages sent from DataProducers.
	 * Default 262144.
	 */
	maxMessageSize: number;

	/**
	 * Custom application data.
	 */
	appData?: DirectTransportAppData;
};

export type DirectTransportDump = BaseTransportDump;

export type DirectTransportStat = BaseTransportStats & {
	type: string;
};

export type DirectTransportEvents = TransportEvents & {
	rtcp: [Buffer];
};

export type DirectTransportObserver =
	EnhancedEventEmitter<DirectTransportObserverEvents>;

export type DirectTransportObserverEvents = TransportObserverEvents & {
	rtcp: [Buffer];
};

export interface DirectTransportInterface<
	DirectTransportAppData extends AppData = AppData,
> extends TransportInterface<
		DirectTransportAppData,
		DirectTransportEvents,
		DirectTransportObserver
	> {
	/**
	 * Observer.
	 *
	 * @override
	 */
	get observer(): DirectTransportObserver;

	/**
	 * Dump DirectTransport.
	 *
	 * @override
	 */
	dump(): Promise<DirectTransportDump>;

	/**
	 * Get DirectTransport stats.
	 *
	 * @override
	 */
	getStats(): Promise<DirectTransportStat[]>;

	/**
	 * NO-OP method in DirectTransport.
	 *
	 * @override
	 */
	connect(): Promise<void>;

	/**
	 * Send RTCP packet.
	 */
	sendRtcp(rtcpPacket: Buffer): void;
}
