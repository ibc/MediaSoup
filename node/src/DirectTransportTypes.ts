import type { EnhancedEventEmitter } from './enhancedEvents';
import type {
	Transport,
	BaseTransportDump,
	BaseTransportStats,
	TransportEvents,
	TransportObserverEvents,
} from './TransportTypes';
import type { AppData } from './types';

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

export interface DirectTransport<
	DirectTransportAppData extends AppData = AppData,
> extends Transport<
		DirectTransportAppData,
		DirectTransportEvents,
		DirectTransportObserver
	> {
	/**
	 * Transport type.
	 *
	 * @override
	 */
	get type(): 'direct';

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
