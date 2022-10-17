import { EnhancedEventEmitter } from './EnhancedEventEmitter';
import { Worker, WorkerSettings } from './Worker';
import { RtpCapabilities } from './RtpParameters';
import * as types from './types';
/**
 * Expose all types.
 */
export { types };
/**
 * Expose mediasoup version.
 */
export declare const version = "3.9.10-lv14-notranscode";
/**
 * Expose parseScalabilityMode() function.
 */
export { parse as parseScalabilityMode } from './scalabilityModes';
export declare type ObserverEvents = {
    newworker: [Worker];
};
declare const observer: EnhancedEventEmitter<ObserverEvents>;
/**
 * Observer.
 */
export { observer };
/**
 * Create a Worker.
 */
export declare function createWorker({ logLevel, logDevLevel, logTraceEnabled, logTags, logFile, rtcMinPort, rtcMaxPort, dtlsCertificateFile, dtlsPrivateKeyFile, appData, }: WorkerSettings): Promise<Worker>;
/**
 * Get a cloned copy of the mediasoup supported RTP capabilities.
 */
export declare function getSupportedRtpCapabilities(): RtpCapabilities;
//# sourceMappingURL=index.d.ts.map