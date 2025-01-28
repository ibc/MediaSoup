import type { EnhancedEventEmitter } from './enhancedEvents';
import type { Worker, WorkerSettings } from './WorkerTypes';
import type { RtpCapabilities } from './rtpParametersTypes';
import type { parseScalabilityMode } from './scalabilityModesUtils';
import type { AppData } from './types';

export type ObserverEvents = {
	newworker: [Worker];
};

export type Observer = EnhancedEventEmitter<ObserverEvents>;

/**
 * Event listeners for mediasoup generated logs.
 */
export type LogEventListeners = {
	ondebug?: (namespace: string, log: string) => void;
	onwarn?: (namespace: string, log: string) => void;
	onerror?: (namespace: string, log: string, error?: Error) => void;
};

export interface Index {
	version: string;
	observer: EnhancedEventEmitter<ObserverEvents>;
	workerBin: string;
	setLogEventListeners: (listeners?: LogEventListeners) => void;
	createWorker: <WorkerAppData extends AppData = AppData>(
		options?: WorkerSettings<WorkerAppData>
	) => Promise<Worker<WorkerAppData>>;
	getSupportedRtpCapabilities: () => RtpCapabilities;
	parseScalabilityMode: typeof parseScalabilityMode;
}
