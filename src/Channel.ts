import * as os from 'os';
import { Duplex } from 'stream';
import { Logger } from './Logger';
import { EnhancedEventEmitter } from './EnhancedEventEmitter';
import { InvalidStateError } from './errors';

const littleEndian = os.endianness() == 'LE';
const logger = new Logger('Channel');

type Sent =
{
	id: number;
	method: string;
	resolve: (data?: any) => void;
	reject: (error: Error) => void;
	close: () => void;
}

// Binary length for a 4194304 bytes payload.
const MESSAGE_MAX_LEN = 4194308;
const PAYLOAD_MAX_LEN = 4194304;

export class Channel extends EnhancedEventEmitter
{
	// Closed flag.
	private _closed = false;

	// Unix Socket instance for sending messages to the worker process.
	private readonly _producerSocket: Duplex;

	// Unix Socket instance for receiving messages to the worker process.
	private readonly _consumerSocket: Duplex;

	// Next id for messages sent to the worker process.
	private _nextId = 0;

	// Map of pending sent requests.
	private readonly _sents: Map<number, Sent> = new Map();

	// Buffer for reading messages from the worker.
	private _recvBuffer = Buffer.alloc(0);

	/**
	 * @private
	 */
	constructor(
		{
			producerSocket,
			consumerSocket,
			pid
		}:
		{
			producerSocket: any;
			consumerSocket: any;
			pid: number;
		})
	{
		super();

		logger.debug('constructor()');

		this._producerSocket = producerSocket as Duplex;
		this._consumerSocket = consumerSocket as Duplex;

		// Read Channel responses/notifications from the worker.
		this._consumerSocket.on('data', (buffer: Buffer) =>
		{
			if (!this._recvBuffer.length)
			{
				this._recvBuffer = buffer;
			}
			else
			{
				this._recvBuffer = Buffer.concat(
					[ this._recvBuffer, buffer ],
					this._recvBuffer.length + buffer.length);
			}

			if (this._recvBuffer.length > PAYLOAD_MAX_LEN)
			{
				logger.error('receiving buffer is full, discarding all data in it');

				// Reset the buffer and exit.
				this._recvBuffer = Buffer.alloc(0);

				return;
			}

			let msgStart = 0;

			while (true) // eslint-disable-line no-constant-condition
			{
				const readLen = this._recvBuffer.length - msgStart;

				if (readLen < 4)
				{
					// Incomplete data.
					break;
				}

				const dataView = new DataView(
					this._recvBuffer.buffer,
					this._recvBuffer.byteOffset + msgStart);
				const msgLen = dataView.getUint32(0, littleEndian);

				if (readLen < 4 + msgLen)
				{
					// Incomplete data.
					break;
				}

				const payload = this._recvBuffer.subarray(msgStart + 4, msgStart + 4 + msgLen);

				msgStart += 4 + msgLen;

				try
				{
					// We can receive JSON messages (Channel messages) or log strings.
					switch (payload[0])
					{
						// 123 = '{' (a Channel JSON message).
						case 123:
							this._processMessage(JSON.parse(payload.toString('utf8')));
							break;

						// 68 = 'D' (a debug log).
						case 68:
							logger.debug(`[pid:${pid}] ${payload.toString('utf8', 1)}`);
							break;

						// 87 = 'W' (a warn log).
						case 87:
							logger.warn(`[pid:${pid}] ${payload.toString('utf8', 1)}`);
							break;

						// 69 = 'E' (an error log).
						case 69:
							logger.error(`[pid:${pid} ${payload.toString('utf8', 1)}`);
							break;

						// 88 = 'X' (a dump log).
						case 88:
							// eslint-disable-next-line no-console
							console.log(payload.toString('utf8', 1));
							break;

						default:
							// eslint-disable-next-line no-console
							console.warn(
								`worker[pid:${pid}] unexpected data: %s`,
								payload.toString('utf8', 1));
					}
				}
				catch (error)
				{
					logger.error(
						'received invalid message from the worker process: %s',
						String(error));
				}
			}

			if (msgStart != 0)
			{
				this._recvBuffer = this._recvBuffer.slice(msgStart);
			}
		});

		this._consumerSocket.on('end', () => (
			logger.debug('Consumer Channel ended by the worker process')
		));

		this._consumerSocket.on('error', (error) => (
			logger.error('Consumer Channel error: %s', String(error))
		));

		this._producerSocket.on('end', () => (
			logger.debug('Producer Channel ended by the worker process')
		));

		this._producerSocket.on('error', (error) => (
			logger.error('Producer Channel error: %s', String(error))
		));
	}

	/**
	 * @private
	 */
	close(): void
	{
		if (this._closed)
			return;

		logger.debug('close()');

		this._closed = true;

		// Close every pending sent.
		for (const sent of this._sents.values())
		{
			sent.close();
		}

		// Remove event listeners but leave a fake 'error' hander to avoid
		// propagation.
		this._consumerSocket.removeAllListeners('end');
		this._consumerSocket.removeAllListeners('error');
		this._consumerSocket.on('error', () => {});

		this._producerSocket.removeAllListeners('end');
		this._producerSocket.removeAllListeners('error');
		this._producerSocket.on('error', () => {});

		// Destroy the socket after a while to allow pending incoming messages.
		setTimeout(() =>
		{
			try { this._producerSocket.destroy(); }
			catch (error) {}
			try { this._consumerSocket.destroy(); }
			catch (error) {}
		}, 200);
	}

	/**
	 * @private
	 */
	async request(method: string, internal?: object, data?: any): Promise<any>
	{
		this._nextId < 4294967295 ? ++this._nextId : (this._nextId = 1);

		const id = this._nextId;

		logger.debug('request() [method:%s, id:%s]', method, id);

		if (this._closed)
			throw new InvalidStateError('Channel closed');

		const request = { id, method, internal, data };
		const payload = JSON.stringify(request);

		if (Buffer.byteLength(payload) > MESSAGE_MAX_LEN)
			throw new Error('Channel request too big');

		// This may throw if closed or remote side ended.
		this._producerSocket.write(
			Buffer.from(Uint32Array.of(Buffer.byteLength(payload)).buffer));
		this._producerSocket.write(payload);

		return new Promise((pResolve, pReject) =>
		{
			const sent: Sent =
			{
				id      : id,
				method  : method,
				resolve : (data2) =>
				{
					if (!this._sents.delete(id))
						return;

					pResolve(data2);
				},
				reject : (error) =>
				{
					if (!this._sents.delete(id))
						return;

					pReject(error);
				},
				close : () =>
				{
					pReject(new InvalidStateError('Channel closed'));
				}
			};

			// Add sent stuff to the map.
			this._sents.set(id, sent);
		});
	}

	private _processMessage(msg: any): void
	{
		// If a response, retrieve its associated request.
		if (msg.id)
		{
			const sent = this._sents.get(msg.id);

			if (!sent)
			{
				logger.error(
					'received response does not match any sent request [id:%s]', msg.id);

				return;
			}

			if (msg.accepted)
			{
				logger.debug(
					'request succeeded [method:%s, id:%s]', sent.method, sent.id);

				sent.resolve(msg.data);
			}
			else if (msg.error)
			{
				logger.warn(
					'request failed [method:%s, id:%s]: %s',
					sent.method, sent.id, msg.reason);

				switch (msg.error)
				{
					case 'TypeError':
						sent.reject(new TypeError(msg.reason));
						break;

					default:
						sent.reject(new Error(msg.reason));
				}
			}
			else
			{
				logger.error(
					'received response is not accepted nor rejected [method:%s, id:%s]',
					sent.method, sent.id);
			}
		}
		// If a notification emit it to the corresponding entity.
		else if (msg.targetId && msg.event)
		{
			// Due to how Promises work, it may happen that we receive a response
			// from the worker followed by a notification from the worker. If we
			// emit the notification immediately it may reach its target **before**
			// the response, destroying the ordered delivery. So we must wait a bit
			// here.
			// See https://github.com/versatica/mediasoup/issues/510
			setImmediate(() => this.emit(msg.targetId, msg.event, msg.data));
		}
		// Otherwise unexpected message.
		else
		{
			logger.error(
				'received message is not a response nor a notification');
		}
	}
}
