#define MS_CLASS "RTC::WebRtcServer"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/WebRtcServer.hpp"
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include "Settings.hpp"
#include "Utils.hpp"
#include <cmath> // std::pow()

namespace RTC
{
	/* Static. */

	static constexpr uint16_t IceCandidateDefaultLocalPriority{ 10000 };
	// We just provide "host" candidates so type preference is fixed.
	static constexpr uint16_t IceTypePreference{ 64 };
	// We do not support non rtcp-mux so component is always 1.
	static constexpr uint16_t IceComponent{ 1 };

	static inline uint32_t generateIceCandidatePriority(uint16_t localPreference)
	{
		MS_TRACE();

		return std::pow(2, 24) * IceTypePreference + std::pow(2, 8) * localPreference +
		       std::pow(2, 0) * (256 - IceComponent);
	}

	/* Class methods. */

	inline std::string WebRtcServer::GetLocalIceUsernameFragmentFromReceivedStunPacket(
	  RTC::StunPacket* packet)
	{
		MS_TRACE();

		// Here we inspect the USERNAME attribute of a received STUN request and
		// extract its remote usernameFragment (the one given to our IceServer as
		// local usernameFragment) which is the first value in the attribute value
		// before the ":" symbol.

		const auto& username  = packet->GetUsername();
		const size_t colonPos = username.find(':');

		// If no colon is found just return the whole USERNAME attribute anyway.
		if (colonPos == std::string::npos)
		{
			return username;
		}

		return username.substr(0, colonPos);
	}

	/* Instance methods. */

	WebRtcServer::WebRtcServer(
	  RTC::Shared* shared,
	  const std::string& id,
	  const flatbuffers::Vector<flatbuffers::Offset<FBS::Transport::ListenInfo>>* listenInfos)
	  : id(id), shared(shared)
	{
		MS_TRACE();

		if (listenInfos->size() == 0)
		{
			MS_THROW_TYPE_ERROR("wrong listenInfos (empty array)");
		}
		else if (listenInfos->size() > 8)
		{
			MS_THROW_TYPE_ERROR("wrong listenInfos (too many entries)");
		}

		try
		{
			for (const auto* listenInfo : *listenInfos)
			{
				auto ip = listenInfo->ip()->str();

				// This may throw.
				Utils::IP::NormalizeIp(ip);

				std::string announcedAddress;

				if (flatbuffers::IsFieldPresent(listenInfo, FBS::Transport::ListenInfo::VT_ANNOUNCEDADDRESS))
				{
					announcedAddress = listenInfo->announcedAddress()->str();
				}

				RTC::Transport::SocketFlags flags;

				flags.ipv6Only     = listenInfo->flags()->ipv6Only();
				flags.udpReusePort = listenInfo->flags()->udpReusePort();

				if (listenInfo->protocol() == FBS::Transport::Protocol::UDP)
				{
					// This may throw.
					RTC::UdpSocket* udpSocket;

					if (listenInfo->portRange()->min() != 0 && listenInfo->portRange()->max() != 0)
					{
						uint64_t portRangeHash{ 0u };

						udpSocket = new RTC::UdpSocket(
						  this,
						  ip,
						  listenInfo->portRange()->min(),
						  listenInfo->portRange()->max(),
						  flags,
						  portRangeHash);
					}
					else if (listenInfo->port() != 0)
					{
						udpSocket = new RTC::UdpSocket(this, ip, listenInfo->port(), flags);
					}
					// NOTE: This is temporal to allow deprecated usage of worker port range.
					// In the future this should throw since |port| or |portRange| will be
					// required.
					else
					{
						uint64_t portRangeHash{ 0u };

						udpSocket = new RTC::UdpSocket(
						  this,
						  ip,
						  Settings::configuration.rtcMinPort,
						  Settings::configuration.rtcMaxPort,
						  flags,
						  portRangeHash);
					}

					this->udpSocketOrTcpServers.emplace_back(udpSocket, nullptr, announcedAddress);

					if (listenInfo->sendBufferSize() != 0)
					{
						udpSocket->SetSendBufferSize(listenInfo->sendBufferSize());
					}

					if (listenInfo->recvBufferSize() != 0)
					{
						udpSocket->SetRecvBufferSize(listenInfo->recvBufferSize());
					}

					MS_DEBUG_TAG(
					  info,
					  "UDP socket send buffer size: %d, recv buffer size: %d",
					  udpSocket->GetSendBufferSize(),
					  udpSocket->GetRecvBufferSize());
				}
				else if (listenInfo->protocol() == FBS::Transport::Protocol::TCP)
				{
					// This may throw.
					RTC::TcpServer* tcpServer;

					if (listenInfo->portRange()->min() != 0 && listenInfo->portRange()->max() != 0)
					{
						uint64_t portRangeHash{ 0u };

						tcpServer = new RTC::TcpServer(
						  this,
						  this,
						  ip,
						  listenInfo->portRange()->min(),
						  listenInfo->portRange()->max(),
						  flags,
						  portRangeHash);
					}
					else if (listenInfo->port() != 0)
					{
						tcpServer = new RTC::TcpServer(this, this, ip, listenInfo->port(), flags);
					}
					// NOTE: This is temporal to allow deprecated usage of worker port range.
					// In the future this should throw since |port| or |portRange| will be
					// required.
					else
					{
						uint64_t portRangeHash{ 0u };

						tcpServer = new RTC::TcpServer(
						  this,
						  this,
						  ip,
						  Settings::configuration.rtcMinPort,
						  Settings::configuration.rtcMaxPort,
						  flags,
						  portRangeHash);
					}

					this->udpSocketOrTcpServers.emplace_back(nullptr, tcpServer, announcedAddress);

					if (listenInfo->sendBufferSize() != 0)
					{
						tcpServer->SetSendBufferSize(listenInfo->sendBufferSize());
					}

					if (listenInfo->recvBufferSize() != 0)
					{
						tcpServer->SetRecvBufferSize(listenInfo->recvBufferSize());
					}

					MS_DEBUG_TAG(
					  info,
					  "TCP server send buffer size: %d, recv buffer size: %d",
					  tcpServer->GetSendBufferSize(),
					  tcpServer->GetRecvBufferSize());
				}
			}

			// NOTE: This may throw.
			this->shared->channelMessageRegistrator->RegisterHandler(
			  this->id,
			  /*channelRequestHandler*/ this,
			  /*channelNotificationHandler*/ nullptr);
		}
		catch (const MediaSoupError& error)
		{
			// Must delete everything since the destructor won't be called.

			for (auto& item : this->udpSocketOrTcpServers)
			{
				delete item.udpSocket;
				item.udpSocket = nullptr;

				delete item.tcpServer;
				item.tcpServer = nullptr;
			}
			this->udpSocketOrTcpServers.clear();

			throw;
		}
	}

	WebRtcServer::~WebRtcServer()
	{
		MS_TRACE();

		this->closing = true;

		this->shared->channelMessageRegistrator->UnregisterHandler(this->id);

		// NOTE: We need to close WebRtcTransports first since they may need to
		// send DTLS Close Alert so UDP sockets and TCP connections must remain
		// open.
		for (auto* webRtcTransport : this->webRtcTransports)
		{
			webRtcTransport->ListenServerClosed();
		}
		this->webRtcTransports.clear();

		for (auto& item : this->udpSocketOrTcpServers)
		{
			delete item.udpSocket;
			item.udpSocket = nullptr;

			delete item.tcpServer;
			item.tcpServer = nullptr;
		}
		this->udpSocketOrTcpServers.clear();
	}

	flatbuffers::Offset<FBS::WebRtcServer::DumpResponse> WebRtcServer::FillBuffer(
	  flatbuffers::FlatBufferBuilder& builder) const
	{
		MS_TRACE();

		// Add udpSockets and tcpServers.
		std::vector<flatbuffers::Offset<FBS::WebRtcServer::IpPort>> udpSockets;
		std::vector<flatbuffers::Offset<FBS::WebRtcServer::IpPort>> tcpServers;

		for (const auto& item : this->udpSocketOrTcpServers)
		{
			if (item.udpSocket)
			{
				udpSockets.emplace_back(FBS::WebRtcServer::CreateIpPortDirect(
				  builder, item.udpSocket->GetLocalIp().c_str(), item.udpSocket->GetLocalPort()));
			}
			else if (item.tcpServer)
			{
				tcpServers.emplace_back(FBS::WebRtcServer::CreateIpPortDirect(
				  builder, item.tcpServer->GetLocalIp().c_str(), item.tcpServer->GetLocalPort()));
			}
		}

		// Add webRtcTransportIds.
		std::vector<flatbuffers::Offset<flatbuffers::String>> webRtcTransportIds;

		for (auto* webRtcTransport : this->webRtcTransports)
		{
			webRtcTransportIds.emplace_back(builder.CreateString(webRtcTransport->id));
		}

		// Add localIceUsernameFragments.
		std::vector<flatbuffers::Offset<FBS::WebRtcServer::IceUserNameFragment>> localIceUsernameFragments;

		for (const auto& kv : this->mapLocalIceUsernameFragmentWebRtcTransport)
		{
			const auto& localIceUsernameFragment = kv.first;
			const auto* webRtcTransport          = kv.second;

			localIceUsernameFragments.emplace_back(FBS::WebRtcServer::CreateIceUserNameFragmentDirect(
			  builder, localIceUsernameFragment.c_str(), webRtcTransport->id.c_str()));
		}

		// Add tupleHashes.
		std::vector<flatbuffers::Offset<FBS::WebRtcServer::TupleHash>> tupleHashes;

		for (const auto& kv : this->mapTupleWebRtcTransport)
		{
			const auto& tupleHash       = kv.first;
			const auto* webRtcTransport = kv.second;

			tupleHashes.emplace_back(
			  FBS::WebRtcServer::CreateTupleHashDirect(builder, tupleHash, webRtcTransport->id.c_str()));
		}

		return FBS::WebRtcServer::CreateDumpResponseDirect(
		  builder,
		  this->id.c_str(),
		  &udpSockets,
		  &tcpServers,
		  &webRtcTransportIds,
		  &localIceUsernameFragments,
		  &tupleHashes);
	}

	void WebRtcServer::HandleRequest(Channel::ChannelRequest* request)
	{
		MS_TRACE();

		switch (request->method)
		{
			case Channel::ChannelRequest::Method::WEBRTCSERVER_DUMP:
			{
				auto dumpOffset = FillBuffer(request->GetBufferBuilder());

				request->Accept(FBS::Response::Body::WebRtcServer_DumpResponse, dumpOffset);

				break;
			}

			default:
			{
				MS_THROW_ERROR("unknown method '%s'", request->methodCStr);
			}
		}
	}

	std::vector<RTC::IceCandidate> WebRtcServer::GetIceCandidates(
	  bool enableUdp, bool enableTcp, bool preferUdp, bool preferTcp) const
	{
		MS_TRACE();

		std::vector<RTC::IceCandidate> iceCandidates;
		uint16_t iceLocalPreferenceDecrement{ 0 };

		for (const auto& item : this->udpSocketOrTcpServers)
		{
			if (item.udpSocket && enableUdp)
			{
				uint16_t iceLocalPreference = IceCandidateDefaultLocalPriority - iceLocalPreferenceDecrement;

				if (preferUdp)
				{
					iceLocalPreference += 1000;
				}

				const uint32_t icePriority = generateIceCandidatePriority(iceLocalPreference);

				if (item.announcedAddress.empty())
				{
					iceCandidates.emplace_back(item.udpSocket, icePriority);
				}
				else
				{
					iceCandidates.emplace_back(
					  item.udpSocket, icePriority, const_cast<std::string&>(item.announcedAddress));
				}
			}
			else if (item.tcpServer && enableTcp)
			{
				uint16_t iceLocalPreference = IceCandidateDefaultLocalPriority - iceLocalPreferenceDecrement;

				if (preferTcp)
				{
					iceLocalPreference += 1000;
				}

				const uint32_t icePriority = generateIceCandidatePriority(iceLocalPreference);

				if (item.announcedAddress.empty())
				{
					iceCandidates.emplace_back(item.tcpServer, icePriority);
				}
				else
				{
					iceCandidates.emplace_back(
					  item.tcpServer, icePriority, const_cast<std::string&>(item.announcedAddress));
				}
			}

			// Decrement initial ICE local preference for next IP.
			iceLocalPreferenceDecrement += 100;
		}

		return iceCandidates;
	}

	inline void WebRtcServer::OnPacketReceived(RTC::TransportTuple* tuple, const uint8_t* data, size_t len)
	{
		MS_TRACE();

		if (RTC::StunPacket::IsStun(data, len))
		{
			OnStunDataReceived(tuple, data, len);
		}
		else
		{
			OnNonStunDataReceived(tuple, data, len);
		}
	}

	inline void WebRtcServer::OnStunDataReceived(RTC::TransportTuple* tuple, const uint8_t* data, size_t len)
	{
		MS_TRACE();

		RTC::StunPacket* packet = RTC::StunPacket::Parse(data, len);

		if (!packet)
		{
			MS_WARN_TAG(ice, "ignoring wrong STUN packet received");

			return;
		}

		// First try doing lookup in the tuples table.
		auto it1 = this->mapTupleWebRtcTransport.find(tuple->hash);

		if (it1 != this->mapTupleWebRtcTransport.end())
		{
			auto* webRtcTransport = it1->second;

			webRtcTransport->ProcessStunPacketFromWebRtcServer(tuple, packet);

			delete packet;

			return;
		}

		// Otherwise try to match the local ICE username fragment.
		auto key = WebRtcServer::GetLocalIceUsernameFragmentFromReceivedStunPacket(packet);
		auto it2 = this->mapLocalIceUsernameFragmentWebRtcTransport.find(key);

		if (it2 == this->mapLocalIceUsernameFragmentWebRtcTransport.end())
		{
			MS_WARN_TAG(ice, "ignoring received STUN packet with unknown remote ICE usernameFragment");

			delete packet;

			return;
		}

		auto* webRtcTransport = it2->second;

		webRtcTransport->ProcessStunPacketFromWebRtcServer(tuple, packet);

		delete packet;
	}

	inline void WebRtcServer::OnNonStunDataReceived(
	  RTC::TransportTuple* tuple, const uint8_t* data, size_t len)
	{
		MS_TRACE();

		auto it = this->mapTupleWebRtcTransport.find(tuple->hash);

		if (it == this->mapTupleWebRtcTransport.end())
		{
			MS_WARN_TAG(ice, "ignoring received non STUN data from unknown tuple");

			return;
		}

		auto* webRtcTransport = it->second;

		webRtcTransport->ProcessNonStunPacketFromWebRtcServer(tuple, data, len);
	}

	inline void WebRtcServer::OnWebRtcTransportCreated(RTC::WebRtcTransport* webRtcTransport)
	{
		MS_TRACE();

		MS_ASSERT(
		  this->webRtcTransports.find(webRtcTransport) == this->webRtcTransports.end(),
		  "WebRtcTransport already handled");

		this->webRtcTransports.insert(webRtcTransport);
	}

	inline void WebRtcServer::OnWebRtcTransportClosed(RTC::WebRtcTransport* webRtcTransport)
	{
		MS_TRACE();

		MS_ASSERT(
		  this->webRtcTransports.find(webRtcTransport) != this->webRtcTransports.end(),
		  "WebRtcTransport not handled");

		// NOTE: If WebRtcServer is closing then do not remove the transport from
		// the set since it would modify the set while the WebRtcServer destructor
		// is iterating it.
		// See: https://github.com/versatica/mediasoup/pull/1369#issuecomment-2044672247
		if (!this->closing)
		{
			this->webRtcTransports.erase(webRtcTransport);
		}
	}

	inline void WebRtcServer::OnWebRtcTransportLocalIceUsernameFragmentAdded(
	  RTC::WebRtcTransport* webRtcTransport, const std::string& usernameFragment)
	{
		MS_TRACE();

		MS_ASSERT(
		  this->mapLocalIceUsernameFragmentWebRtcTransport.find(usernameFragment) ==
		    this->mapLocalIceUsernameFragmentWebRtcTransport.end(),
		  "local ICE username fragment already exists in the table");

		this->mapLocalIceUsernameFragmentWebRtcTransport[usernameFragment] = webRtcTransport;
	}

	inline void WebRtcServer::OnWebRtcTransportLocalIceUsernameFragmentRemoved(
	  RTC::WebRtcTransport* /*webRtcTransport*/, const std::string& usernameFragment)
	{
		MS_TRACE();

		MS_ASSERT(
		  this->mapLocalIceUsernameFragmentWebRtcTransport.find(usernameFragment) !=
		    this->mapLocalIceUsernameFragmentWebRtcTransport.end(),
		  "local ICE username fragment not found in the table");

		this->mapLocalIceUsernameFragmentWebRtcTransport.erase(usernameFragment);
	}

	inline void WebRtcServer::OnWebRtcTransportTransportTupleAdded(
	  RTC::WebRtcTransport* webRtcTransport, RTC::TransportTuple* tuple)
	{
		MS_TRACE();

		if (this->mapTupleWebRtcTransport.find(tuple->hash) != this->mapTupleWebRtcTransport.end())
		{
			MS_WARN_TAG(ice, "tuple hash already exists in the table");

			return;
		}

		this->mapTupleWebRtcTransport[tuple->hash] = webRtcTransport;
	}

	inline void WebRtcServer::OnWebRtcTransportTransportTupleRemoved(
	  RTC::WebRtcTransport* /*webRtcTransport*/, RTC::TransportTuple* tuple)
	{
		MS_TRACE();

		if (this->mapTupleWebRtcTransport.find(tuple->hash) == this->mapTupleWebRtcTransport.end())
		{
			MS_DEBUG_TAG(ice, "tuple hash not found in the table");

			return;
		}

		this->mapTupleWebRtcTransport.erase(tuple->hash);
	}

	inline void WebRtcServer::OnUdpSocketPacketReceived(
	  RTC::UdpSocket* socket, const uint8_t* data, size_t len, const struct sockaddr* remoteAddr)
	{
		MS_TRACE();

		RTC::TransportTuple tuple(socket, remoteAddr);

		OnPacketReceived(&tuple, data, len);
	}

	inline void WebRtcServer::OnRtcTcpConnectionClosed(
	  RTC::TcpServer* /*tcpServer*/, RTC::TcpConnection* connection)
	{
		MS_TRACE();

		RTC::TransportTuple tuple(connection);

		// NOTE: We cannot assert whether this tuple is still in our
		// mapTupleWebRtcTransport because this event may be called after the tuple
		// was removed from it.

		auto it = this->mapTupleWebRtcTransport.find(tuple.hash);

		if (it == this->mapTupleWebRtcTransport.end())
		{
			return;
		}

		auto* webRtcTransport = it->second;

		webRtcTransport->RemoveTuple(&tuple);
	}

	inline void WebRtcServer::OnTcpConnectionPacketReceived(
	  RTC::TcpConnection* connection, const uint8_t* data, size_t len)
	{
		MS_TRACE();

		RTC::TransportTuple tuple(connection);

		OnPacketReceived(&tuple, data, len);
	}
} // namespace RTC
