#define MS_CLASS "RTC::PipeTransport"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/PipeTransport.hpp"
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include "Utils.hpp"
#include <cstring> // std::memcpy()

namespace RTC
{
	/* Static. */

	// NOTE: PipeTransport just allows AEAD_AES_256_GCM SRTP crypto suite.
	RTC::SrtpSession::CryptoSuite PipeTransport::srtpCryptoSuite{
		RTC::SrtpSession::CryptoSuite::AEAD_AES_256_GCM
	};
	// MAster length of AEAD_AES_256_GCM.
	size_t PipeTransport::srtpMasterLength{ 44 };

	/* Instance methods. */

	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
	PipeTransport::PipeTransport(
	  RTC::Shared* shared,
	  const std::string& id,
	  RTC::Transport::Listener* listener,
	  const FBS::PipeTransport::PipeTransportOptions* options)
	  : RTC::Transport::Transport(shared, id, listener, options->base())
	{
		MS_TRACE();

		if (options->listenInfo()->protocol() != FBS::Transport::Protocol::UDP)
		{
			MS_THROW_TYPE_ERROR("unsupported listen protocol");
		}

		this->listenInfo.ip.assign(options->listenInfo()->ip()->str());

		// This may throw.
		Utils::IP::NormalizeIp(this->listenInfo.ip);

		if (flatbuffers::IsFieldPresent(
		      options->listenInfo(), FBS::Transport::ListenInfo::VT_ANNOUNCEDADDRESS))
		{
			this->listenInfo.announcedAddress.assign(options->listenInfo()->announcedAddress()->str());
		}

		if (flatbuffers::IsFieldPresent(
		      options->listenInfo(), FBS::Transport::ListenInfo::VT_ANNOUNCEDADDRESS))
		{
			this->listenInfo.announcedAddress.assign(options->listenInfo()->announcedAddress()->str());
		}

		this->listenInfo.port               = options->listenInfo()->port();
		this->listenInfo.portRange.min      = options->listenInfo()->portRange()->min();
		this->listenInfo.portRange.max      = options->listenInfo()->portRange()->max();
		this->listenInfo.sendBufferSize     = options->listenInfo()->sendBufferSize();
		this->listenInfo.recvBufferSize     = options->listenInfo()->recvBufferSize();
		this->listenInfo.flags.ipv6Only     = options->listenInfo()->flags()->ipv6Only();
		this->listenInfo.flags.udpReusePort = options->listenInfo()->flags()->udpReusePort();

		this->rtx = options->enableRtx();

		if (options->enableSrtp())
		{
			this->srtpKey       = Utils::Crypto::GetRandomString(PipeTransport::srtpMasterLength);
			this->srtpKeyBase64 = Utils::String::Base64Encode(this->srtpKey);
		}

		try
		{
			if (this->listenInfo.portRange.min != 0 && this->listenInfo.portRange.max != 0)
			{
				uint64_t portRangeHash{ 0u };

				this->udpSocket = new RTC::UdpSocket(
				  this,
				  this->listenInfo.ip,
				  this->listenInfo.portRange.min,
				  this->listenInfo.portRange.max,
				  this->listenInfo.flags,
				  portRangeHash);
			}
			else if (this->listenInfo.port != 0)
			{
				this->udpSocket = new RTC::UdpSocket(
				  this, this->listenInfo.ip, this->listenInfo.port, this->listenInfo.flags);
			}
			// NOTE: This is temporal to allow deprecated usage of worker port range.
			// In the future this should throw since |port| or |portRange| will be
			// required.
			else
			{
				uint64_t portRangeHash{ 0u };

				this->udpSocket = new RTC::UdpSocket(
				  this,
				  this->listenInfo.ip,
				  Settings::configuration.rtcMinPort,
				  Settings::configuration.rtcMaxPort,
				  this->listenInfo.flags,
				  portRangeHash);
			}

			if (this->listenInfo.sendBufferSize != 0)
			{
				// NOTE: This may throw.
				this->udpSocket->SetSendBufferSize(this->listenInfo.sendBufferSize);
			}

			if (this->listenInfo.recvBufferSize != 0)
			{
				// NOTE: This may throw.
				this->udpSocket->SetRecvBufferSize(this->listenInfo.recvBufferSize);
			}

			MS_DEBUG_TAG(
			  info,
			  "UDP socket buffer sizes [send:%" PRIu32 ", recv:%" PRIu32 "]",
			  udpSocket->GetSendBufferSize(),
			  udpSocket->GetRecvBufferSize());

			// NOTE: This may throw.
			this->shared->channelMessageRegistrator->RegisterHandler(
			  this->id,
			  /*channelRequestHandler*/ this,
			  /*channelNotificationHandler*/ this);
		}
		catch (const MediaSoupError& error)
		{
			// Must delete everything since the destructor won't be called.

			delete this->udpSocket;
			this->udpSocket = nullptr;

			throw;
		}
	}

	PipeTransport::~PipeTransport()
	{
		MS_TRACE();

		// Tell the Transport parent class that we are about to destroy
		// the class instance.
		Destroying();

		this->shared->channelMessageRegistrator->UnregisterHandler(this->id);

		delete this->udpSocket;
		this->udpSocket = nullptr;

		delete this->tuple;
		this->tuple = nullptr;

		delete this->srtpSendSession;
		this->srtpSendSession = nullptr;

		delete this->srtpRecvSession;
		this->srtpRecvSession = nullptr;
	}

	flatbuffers::Offset<FBS::PipeTransport::DumpResponse> PipeTransport::FillBuffer(
	  flatbuffers::FlatBufferBuilder& builder) const
	{
		MS_TRACE();

		// Add tuple.
		flatbuffers::Offset<FBS::Transport::Tuple> tuple;

		if (this->tuple)
		{
			tuple = this->tuple->FillBuffer(builder);
		}
		else
		{
			std::string localIp;

			if (this->listenInfo.announcedAddress.empty())
			{
				localIp = this->udpSocket->GetLocalIp();
			}
			else
			{
				localIp = this->listenInfo.announcedAddress;
			}

			tuple = FBS::Transport::CreateTupleDirect(
			  builder,
			  localIp.c_str(),
			  this->udpSocket->GetLocalPort(),
			  nullptr,
			  0,
			  FBS::Transport::Protocol::UDP);
		}

		// Add srtpParameters.
		flatbuffers::Offset<FBS::SrtpParameters::SrtpParameters> srtpParameters;

		if (HasSrtp())
		{
			srtpParameters = FBS::SrtpParameters::CreateSrtpParametersDirect(
			  builder,
			  SrtpSession::CryptoSuiteToFbs(PipeTransport::srtpCryptoSuite),
			  this->srtpKeyBase64.c_str());
		}

		// Add base transport dump.
		auto base = Transport::FillBuffer(builder);

		return FBS::PipeTransport::CreateDumpResponse(builder, base, tuple, this->rtx, srtpParameters);
	}

	flatbuffers::Offset<FBS::PipeTransport::GetStatsResponse> PipeTransport::FillBufferStats(
	  flatbuffers::FlatBufferBuilder& builder)
	{
		MS_TRACE();

		// Add tuple.
		flatbuffers::Offset<FBS::Transport::Tuple> tuple;

		if (this->tuple)
		{
			tuple = this->tuple->FillBuffer(builder);
		}
		else
		{
			std::string localIp;

			if (this->listenInfo.announcedAddress.empty())
			{
				localIp = this->udpSocket->GetLocalIp();
			}
			else
			{
				localIp = this->listenInfo.announcedAddress;
			}

			tuple = FBS::Transport::CreateTupleDirect(
			  builder,
			  // localIp.
			  localIp.c_str(),
			  // localPort,
			  this->udpSocket->GetLocalPort(),
			  // remoteIp.
			  nullptr,
			  // remotePort.
			  0,
			  // protocol.
			  FBS::Transport::Protocol::UDP);
		}

		// Base Transport stats.
		auto base = Transport::FillBufferStats(builder);

		return FBS::PipeTransport::CreateGetStatsResponse(builder, base, tuple);
	}

	void PipeTransport::HandleRequest(Channel::ChannelRequest* request)
	{
		MS_TRACE();

		switch (request->method)
		{
			case Channel::ChannelRequest::Method::TRANSPORT_GET_STATS:
			{
				auto responseOffset = FillBufferStats(request->GetBufferBuilder());

				request->Accept(FBS::Response::Body::PipeTransport_GetStatsResponse, responseOffset);

				break;
			}

			case Channel::ChannelRequest::Method::TRANSPORT_DUMP:
			{
				auto dumpOffset = FillBuffer(request->GetBufferBuilder());

				request->Accept(FBS::Response::Body::PipeTransport_DumpResponse, dumpOffset);

				break;
			}

			case Channel::ChannelRequest::Method::PIPETRANSPORT_CONNECT:
			{
				// Ensure this method is not called twice.
				if (this->tuple)
				{
					MS_THROW_ERROR("connect() already called");
				}

				try
				{
					std::string ip;
					uint16_t port{ 0u };
					std::string srtpKeyBase64;

					const auto* body = request->data->body_as<FBS::PipeTransport::ConnectRequest>();

					auto srtpParametersPresent =
					  flatbuffers::IsFieldPresent(body, FBS::PipeTransport::ConnectRequest::VT_SRTPPARAMETERS);

					if (!HasSrtp() && srtpParametersPresent)
					{
						MS_THROW_TYPE_ERROR("invalid srtpParameters (SRTP not enabled)");
					}
					else if (HasSrtp())
					{
						if (!srtpParametersPresent)
						{
							MS_THROW_TYPE_ERROR("missing srtpParameters (SRTP enabled)");
						}

						const auto* srtpParameters = body->srtpParameters();

						// NOTE: We just use AEAD_AES_256_GCM as SRTP crypto suite in
						// PipeTransport.
						if (srtpParameters->cryptoSuite() != FBS::SrtpParameters::SrtpCryptoSuite::AEAD_AES_256_GCM)
						{
							MS_THROW_TYPE_ERROR("invalid/unsupported srtpParameters.cryptoSuite");
						}

						srtpKeyBase64 = srtpParameters->keyBase64()->str();

						size_t outLen;
						// This may throw.
						auto* srtpKey = Utils::String::Base64Decode(srtpKeyBase64, outLen);

						if (outLen != PipeTransport::srtpMasterLength)
						{
							MS_THROW_TYPE_ERROR("invalid decoded SRTP key length");
						}

						auto* srtpLocalKey  = new uint8_t[PipeTransport::srtpMasterLength];
						auto* srtpRemoteKey = new uint8_t[PipeTransport::srtpMasterLength];

						std::memcpy(srtpLocalKey, this->srtpKey.c_str(), PipeTransport::srtpMasterLength);
						std::memcpy(srtpRemoteKey, srtpKey, PipeTransport::srtpMasterLength);

						try
						{
							this->srtpSendSession = new RTC::SrtpSession(
							  RTC::SrtpSession::Type::OUTBOUND,
							  PipeTransport::srtpCryptoSuite,
							  srtpLocalKey,
							  PipeTransport::srtpMasterLength);
						}
						catch (const MediaSoupError& error)
						{
							delete[] srtpLocalKey;
							delete[] srtpRemoteKey;

							MS_THROW_ERROR("error creating SRTP sending session: %s", error.what());
						}

						try
						{
							this->srtpRecvSession = new RTC::SrtpSession(
							  RTC::SrtpSession::Type::INBOUND,
							  PipeTransport::srtpCryptoSuite,
							  srtpRemoteKey,
							  PipeTransport::srtpMasterLength);
						}
						catch (const MediaSoupError& error)
						{
							delete[] srtpLocalKey;
							delete[] srtpRemoteKey;

							MS_THROW_ERROR("error creating SRTP receiving session: %s", error.what());
						}

						delete[] srtpLocalKey;
						delete[] srtpRemoteKey;
					}

					if (!flatbuffers::IsFieldPresent(body, FBS::PipeTransport::ConnectRequest::VT_IP))
					{
						MS_THROW_TYPE_ERROR("missing ip");
					}

					ip = body->ip()->str();

					// This may throw.
					Utils::IP::NormalizeIp(ip);

					if (!body->port().has_value())
					{
						MS_THROW_TYPE_ERROR("missing port");
					}

					port = body->port().value();

					int err;

					switch (Utils::IP::GetFamily(ip))
					{
						case AF_INET:
						{
							err = uv_ip4_addr(
							  ip.c_str(),
							  static_cast<int>(port),
							  reinterpret_cast<struct sockaddr_in*>(&this->remoteAddrStorage));

							if (err != 0)
							{
								MS_THROW_ERROR("uv_ip4_addr() failed: %s", uv_strerror(err));
							}

							break;
						}

						case AF_INET6:
						{
							err = uv_ip6_addr(
							  ip.c_str(),
							  static_cast<int>(port),
							  reinterpret_cast<struct sockaddr_in6*>(&this->remoteAddrStorage));

							if (err != 0)
							{
								MS_THROW_ERROR("uv_ip6_addr() failed: %s", uv_strerror(err));
							}

							break;
						}

						default:
						{
							MS_THROW_ERROR("invalid IP '%s'", ip.c_str());
						}
					}

					// Create the tuple.
					this->tuple = new RTC::TransportTuple(
					  this->udpSocket, reinterpret_cast<struct sockaddr*>(&this->remoteAddrStorage));

					if (!this->listenInfo.announcedAddress.empty())
					{
						this->tuple->SetLocalAnnouncedAddress(this->listenInfo.announcedAddress);
					}
				}
				catch (const MediaSoupError& error)
				{
					delete this->tuple;
					this->tuple = nullptr;

					delete this->srtpSendSession;
					this->srtpSendSession = nullptr;

					delete this->srtpRecvSession;
					this->srtpRecvSession = nullptr;

					throw;
				}

				auto tupleOffset = this->tuple->FillBuffer(request->GetBufferBuilder());

				auto responseOffset =
				  FBS::PipeTransport::CreateConnectResponse(request->GetBufferBuilder(), tupleOffset);

				request->Accept(FBS::Response::Body::PipeTransport_ConnectResponse, responseOffset);

				// Assume we are connected (there is no much more we can do to know it)
				// and tell the parent class.
				RTC::Transport::Connected();

				break;
			}

			default:
			{
				// Pass it to the parent class.
				RTC::Transport::HandleRequest(request);
			}
		}
	}

	void PipeTransport::HandleNotification(Channel::ChannelNotification* notification)
	{
		MS_TRACE();

		// Pass it to the parent class.
		RTC::Transport::HandleNotification(notification);
	}

	inline bool PipeTransport::IsConnected() const
	{
		return this->tuple;
	}

	inline bool PipeTransport::HasSrtp() const
	{
		return !this->srtpKey.empty();
	}

	void PipeTransport::SendRtpPacket(
	  RTC::Consumer* /*consumer*/, RTC::RtpPacket* packet, RTC::Transport::onSendCallback* cb)
	{
		MS_TRACE();

		if (!IsConnected())
		{
			if (cb)
			{
				(*cb)(false);
				delete cb;
			}

			return;
		}

		const uint8_t* data = packet->GetData();
		auto len            = packet->GetSize();

		if (HasSrtp() && !this->srtpSendSession->EncryptRtp(&data, &len))
		{
			if (cb)
			{
				(*cb)(false);
				delete cb;
			}

			return;
		}

		this->tuple->Send(data, len, cb);

		// Increase send transmission.
		RTC::Transport::DataSent(len);
	}

	void PipeTransport::SendRtcpPacket(RTC::RTCP::Packet* packet)
	{
		MS_TRACE();

		if (!IsConnected())
		{
			return;
		}

		const uint8_t* data = packet->GetData();
		auto len            = packet->GetSize();

		if (HasSrtp() && !this->srtpSendSession->EncryptRtcp(&data, &len))
		{
			return;
		}

		this->tuple->Send(data, len);

		// Increase send transmission.
		RTC::Transport::DataSent(len);
	}

	void PipeTransport::SendRtcpCompoundPacket(RTC::RTCP::CompoundPacket* packet)
	{
		MS_TRACE();

		if (!IsConnected())
		{
			return;
		}

		packet->Serialize(RTC::RTCP::Buffer);

		const uint8_t* data = packet->GetData();
		auto len            = packet->GetSize();

		if (HasSrtp() && !this->srtpSendSession->EncryptRtcp(&data, &len))
		{
			return;
		}

		this->tuple->Send(data, len);

		// Increase send transmission.
		RTC::Transport::DataSent(len);
	}

	void PipeTransport::SendMessage(
	  RTC::DataConsumer* dataConsumer, const uint8_t* msg, size_t len, uint32_t ppid, onQueuedCallback* cb)
	{
		MS_TRACE();

		this->sctpAssociation->SendSctpMessage(dataConsumer, msg, len, ppid, cb);
	}

	void PipeTransport::SendSctpData(const uint8_t* data, size_t len)
	{
		MS_TRACE();

		if (!IsConnected())
		{
			return;
		}

		this->tuple->Send(data, len);

		// Increase send transmission.
		RTC::Transport::DataSent(len);
	}

	void PipeTransport::RecvStreamClosed(uint32_t ssrc)
	{
		MS_TRACE();

		if (this->srtpRecvSession)
		{
			this->srtpRecvSession->RemoveStream(ssrc);
		}
	}

	void PipeTransport::SendStreamClosed(uint32_t ssrc)
	{
		MS_TRACE();

		if (this->srtpSendSession)
		{
			this->srtpSendSession->RemoveStream(ssrc);
		}
	}

	inline void PipeTransport::OnPacketReceived(RTC::TransportTuple* tuple, const uint8_t* data, size_t len)
	{
		MS_TRACE();

		// Increase receive transmission.
		RTC::Transport::DataReceived(len);

		// Check if it's RTCP.
		if (RTC::RTCP::Packet::IsRtcp(data, len))
		{
			OnRtcpDataReceived(tuple, data, len);
		}
		// Check if it's RTP.
		else if (RTC::RtpPacket::IsRtp(data, len))
		{
			OnRtpDataReceived(tuple, data, len);
		}
		// Check if it's SCTP.
		else if (RTC::SctpAssociation::IsSctp(data, len))
		{
			OnSctpDataReceived(tuple, data, len);
		}
		else
		{
			MS_WARN_DEV("ignoring received packet of unknown type");
		}
	}

	inline void PipeTransport::OnRtpDataReceived(RTC::TransportTuple* tuple, const uint8_t* data, size_t len)
	{
		MS_TRACE();

		if (!IsConnected())
		{
			return;
		}

		// Decrypt the SRTP packet.
		if (HasSrtp() && !this->srtpRecvSession->DecryptSrtp(const_cast<uint8_t*>(data), &len))
		{
			RTC::RtpPacket* packet = RTC::RtpPacket::Parse(data, len);

			if (!packet)
			{
				MS_WARN_TAG(srtp, "DecryptSrtp() failed due to an invalid RTP packet");
			}
			else
			{
				MS_WARN_TAG(
				  srtp,
				  "DecryptSrtp() failed [ssrc:%" PRIu32 ", payloadType:%" PRIu8 ", seq:%" PRIu16 "]",
				  packet->GetSsrc(),
				  packet->GetPayloadType(),
				  packet->GetSequenceNumber());

				delete packet;
			}

			return;
		}

		RTC::RtpPacket* packet = RTC::RtpPacket::Parse(data, len);

		if (!packet)
		{
			MS_WARN_TAG(rtp, "received data is not a valid RTP packet");

			return;
		}

		// Verify that the packet's tuple matches our tuple.
		if (!this->tuple->Compare(tuple))
		{
			MS_DEBUG_TAG(rtp, "ignoring RTP packet from unknown IP:port");

			// Remove this SSRC.
			RecvStreamClosed(packet->GetSsrc());

			delete packet;

			return;
		}

		// Pass the packet to the parent transport.
		RTC::Transport::ReceiveRtpPacket(packet);
	}

	inline void PipeTransport::OnRtcpDataReceived(
	  RTC::TransportTuple* tuple, const uint8_t* data, size_t len)
	{
		MS_TRACE();

		if (!IsConnected())
		{
			return;
		}

		// Decrypt the SRTCP packet.
		if (HasSrtp() && !this->srtpRecvSession->DecryptSrtcp(const_cast<uint8_t*>(data), &len))
		{
			return;
		}

		// Verify that the packet's tuple matches our tuple.
		if (!this->tuple->Compare(tuple))
		{
			MS_DEBUG_TAG(rtcp, "ignoring RTCP packet from unknown IP:port");

			return;
		}

		RTC::RTCP::Packet* packet = RTC::RTCP::Packet::Parse(data, len);

		if (!packet)
		{
			MS_WARN_TAG(rtcp, "received data is not a valid RTCP compound or single packet");

			return;
		}

		// Pass the packet to the parent transport.
		RTC::Transport::ReceiveRtcpPacket(packet);
	}

	inline void PipeTransport::OnSctpDataReceived(
	  RTC::TransportTuple* tuple, const uint8_t* data, size_t len)
	{
		MS_TRACE();

		if (!IsConnected())
		{
			return;
		}

		// Verify that the packet's tuple matches our tuple.
		if (!this->tuple->Compare(tuple))
		{
			MS_DEBUG_TAG(sctp, "ignoring SCTP packet from unknown IP:port");

			return;
		}

		// Pass it to the parent transport.
		RTC::Transport::ReceiveSctpData(data, len);
	}

	inline void PipeTransport::OnUdpSocketPacketReceived(
	  RTC::UdpSocket* socket, const uint8_t* data, size_t len, const struct sockaddr* remoteAddr)
	{
		MS_TRACE();

		RTC::TransportTuple tuple(socket, remoteAddr);

		OnPacketReceived(&tuple, data, len);
	}
} // namespace RTC
