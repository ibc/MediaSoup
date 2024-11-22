#define MS_CLASS "RTC::TcpConnection"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/TcpConnection.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include <cstring> // std::memmove(), std::memcpy()

namespace RTC
{
	/* Static. */

	static constexpr size_t ReadBufferSize{ 65536 };
	thread_local static uint8_t ReadBuffer[ReadBufferSize];

	/* Instance methods. */

	TcpConnection::TcpConnection(Listener* listener, size_t bufferSize)
	  : ::TcpConnectionHandle::TcpConnectionHandle(bufferSize), listener(listener)
	{
		MS_TRACE();
	}

	TcpConnection::~TcpConnection()
	{
		MS_TRACE();
	}

	void TcpConnection::UserOnTcpConnectionRead()
	{
		MS_TRACE();

		MS_DEBUG_DEV(
		  "data received [local:%s :%" PRIu16 ", remote:%s :%" PRIu16 "]",
		  GetLocalIp().c_str(),
		  GetLocalPort(),
		  GetPeerIp().c_str(),
		  GetPeerPort());

		/*
		 * Framing RFC 4571
		 *
		 *     0                   1                   2                   3
		 *     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		 *     ---------------------------------------------------------------
		 *     |             LENGTH            |  STUN / DTLS / RTP / RTCP   |
		 *     ---------------------------------------------------------------
		 *
		 * A 16-bit unsigned integer LENGTH field, coded in network byte order
		 * (big-endian), begins the frame.  If LENGTH is non-zero, an RTP or
		 * RTCP packet follows the LENGTH field.  The value coded in the LENGTH
		 * field MUST equal the number of octets in the RTP or RTCP packet.
		 * Zero is a valid value for LENGTH, and it codes the null packet.
		 */

		// Be ready to parse more than a single frame in a single TCP chunk.
		while (true)
		{
			// We may receive multiple packets in the same TCP chunk. If one of them is
			// a DTLS Close Alert this would be closed (Close() called) so we cannot call
			// our listeners anymore.
			if (IsClosed())
			{
				return;
			}

			const size_t dataLen = this->bufferDataLen - this->frameStart;
			size_t packetLen;

			if (dataLen >= 2)
			{
				packetLen = size_t{ Utils::Byte::Get2Bytes(this->buffer + this->frameStart, 0) };
			}

			// We have packetLen bytes.
			if (dataLen >= 2 && dataLen >= 2 + packetLen)
			{
				const uint8_t* packet = this->buffer + this->frameStart + 2;

				// Update received bytes and notify the listener.
				if (packetLen != 0)
				{
					// Copy the received packet into the static buffer so it can be expanded
					// later.
					std::memcpy(ReadBuffer, packet, packetLen);

					this->listener->OnTcpConnectionPacketReceived(this, ReadBuffer, packetLen);
				}

				// If there is no more space available in the buffer and that is because
				// the latest parsed frame filled it, then empty the full buffer.
				if ((this->frameStart + 2 + packetLen) == this->bufferSize)
				{
					MS_DEBUG_DEV("no more space in the buffer, emptying the buffer data");

					this->frameStart    = 0;
					this->bufferDataLen = 0;
				}
				// If there is still space in the buffer, set the beginning of the next
				// frame to the next position after the parsed frame.
				else
				{
					this->frameStart += 2 + packetLen;
				}

				// If there is more data in the buffer after the parsed frame then
				// parse again. Otherwise break here and wait for more data.
				if (this->bufferDataLen > this->frameStart)
				{
					MS_DEBUG_DEV("there is more data after the parsed frame, continue parsing");

					continue;
				}

				break;
			}

			// Incomplete packet.

			// Check if the buffer is full.
			if (this->bufferDataLen == this->bufferSize)
			{
				// First case: the incomplete frame does not begin at position 0 of
				// the buffer, so move the frame to the position 0.
				if (this->frameStart != 0)
				{
					MS_DEBUG_DEV(
					  "no more space in the buffer, moving parsed bytes to the beginning of "
					  "the buffer and wait for more data");

					std::memmove(
					  this->buffer, this->buffer + this->frameStart, this->bufferSize - this->frameStart);
					this->bufferDataLen = this->bufferSize - this->frameStart;
					this->frameStart    = 0;
				}
				// Second case: the incomplete frame begins at position 0 of the buffer.
				// The frame is too big.
				else
				{
					MS_WARN_DEV(
					  "no more space in the buffer for the unfinished frame being parsed, closing the "
					  "connection");

					ErrorReceiving();

					// And exit fast since we are supposed to be deallocated.
					return;
				}
			}
			// The buffer is not full.
			else
			{
				MS_DEBUG_DEV("frame not finished yet, waiting for more data");
			}

			// Exit the parsing loop.
			break;
		}
	}

	void TcpConnection::Send(const uint8_t* data, size_t len, ::TcpConnectionHandle::onSendCallback* cb)
	{
		MS_TRACE();

		// Write according to Framing RFC 4571.

		uint8_t frameLen[2];

		Utils::Byte::Set2Bytes(frameLen, 0, len);
		::TcpConnectionHandle::Write(frameLen, 2, data, len, cb);
	}
} // namespace RTC
