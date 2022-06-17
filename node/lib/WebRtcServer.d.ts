import { EnhancedEventEmitter } from './EnhancedEventEmitter';
import { Channel } from './Channel';
import { TransportProtocol } from './Transport';
import { WebRtcTransport } from './WebRtcTransport';
export interface WebRtcServerListenInfo {
    /**
     * Network protocol.
     */
    protocol: TransportProtocol;
    /**
     * Listening IPv4 or IPv6.
     */
    ip: string;
    /**
     * Announced IPv4 or IPv6 (useful when running mediasoup behind NAT with
     * private IP).
     */
    announcedIp?: string;
    /**
     * Listening port.
     */
    port: number;
}
export declare type WebRtcServerOptions = {
    /**
     * Listen infos.
     */
    listenInfos: WebRtcServerListenInfo[];
    /**
     * Custom application data.
     */
    appData?: Record<string, unknown>;
};
export declare type WebRtcServerEvents = {
    workerclose: [];
};
export declare type WebRtcServerObserverEvents = {
    close: [];
    newwebrtctransport: [WebRtcTransport];
};
export declare class WebRtcServer extends EnhancedEventEmitter<WebRtcServerEvents> {
    #private;
    /**
     * @private
     * @emits workerclose
     * @emits @close
     */
    constructor({ internal, channel, appData }: {
        internal: any;
        channel: Channel;
        appData?: Record<string, unknown>;
    });
    /**
     * Router id.
     */
    get id(): string;
    /**
     * Whether the Router is closed.
     */
    get closed(): boolean;
    /**
     * App custom data.
     */
    get appData(): Record<string, unknown>;
    /**
     * Invalid setter.
     */
    set appData(appData: Record<string, unknown>);
    /**
     * Observer.
     *
     * @emits close
     * @emits newwebrtctransport - (webRtcTransport: WebRtcTransport)
     */
    get observer(): EnhancedEventEmitter<WebRtcServerObserverEvents>;
    /**
     * Close the WebRtcServer.
     */
    close(): void;
    /**
     * Worker was closed.
     *
     * @private
     */
    workerClosed(): void;
    /**
     * Dump WebRtcServer.
     */
    dump(): Promise<any>;
    /**
     * @private
     */
    handleWebRtcTransport(webRtcTransport: WebRtcTransport): void;
}
//# sourceMappingURL=WebRtcServer.d.ts.map