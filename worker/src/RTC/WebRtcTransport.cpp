#define MS_CLASS "RTC::WebRtcTransport"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/WebRtcTransport.hpp"
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include "Settings.hpp"
#include "Utils.hpp"
#include "FBS/webRtcTransport.h"
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

	/* Instance methods. */

	/**
	 * This constructor is used when the WebRtcTransport doesn't use a WebRtcServer.
	 */
	WebRtcTransport::WebRtcTransport(
	  RTC::Shared* shared,
	  const std::string& id,
	  RTC::Transport::Listener* listener,
	  const FBS::WebRtcTransport::WebRtcTransportOptions* options)
	  : RTC::Transport::Transport(shared, id, listener, options->base())
	{
		MS_TRACE();

		try
		{
			const auto* listenIndividual = options->listen_as<FBS::WebRtcTransport::ListenIndividual>();
			const auto* listenInfos      = listenIndividual->listenInfos();
			uint16_t iceLocalPreferenceDecrement{ 0u };

			this->iceCandidates.reserve(listenInfos->size());

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

				const uint16_t iceLocalPreference =
				  IceCandidateDefaultLocalPriority - iceLocalPreferenceDecrement;
				const uint32_t icePriority = generateIceCandidatePriority(iceLocalPreference);

				if (listenInfo->protocol() == FBS::Transport::Protocol::UDP)
				{
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

					this->udpSockets[udpSocket] = announcedAddress;

					if (announcedAddress.empty())
					{
						this->iceCandidates.emplace_back(udpSocket, icePriority);
					}
					else
					{
						this->iceCandidates.emplace_back(udpSocket, icePriority, announcedAddress);
					}

					if (listenInfo->sendBufferSize() != 0)
					{
						// NOTE: This may throw.
						udpSocket->SetSendBufferSize(listenInfo->sendBufferSize());
					}

					if (listenInfo->recvBufferSize() != 0)
					{
						// NOTE: This may throw.
						udpSocket->SetRecvBufferSize(listenInfo->recvBufferSize());
					}

					MS_DEBUG_TAG(
					  info,
					  "UDP socket buffer sizes [send:%" PRIu32 ", recv:%" PRIu32 "]",
					  udpSocket->GetSendBufferSize(),
					  udpSocket->GetRecvBufferSize());
				}
				else if (listenInfo->protocol() == FBS::Transport::Protocol::TCP)
				{
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

					this->tcpServers[tcpServer] = announcedAddress;

					if (announcedAddress.empty())
					{
						this->iceCandidates.emplace_back(tcpServer, icePriority);
					}
					else
					{
						this->iceCandidates.emplace_back(tcpServer, icePriority, announcedAddress);
					}

					if (listenInfo->sendBufferSize() != 0)
					{
						// NOTE: This may throw.
						tcpServer->SetSendBufferSize(listenInfo->sendBufferSize());
					}

					if (listenInfo->recvBufferSize() != 0)
					{
						// NOTE: This may throw.
						tcpServer->SetRecvBufferSize(listenInfo->recvBufferSize());
					}

					MS_DEBUG_TAG(
					  info,
					  "TCP sockets buffer sizes [send:%" PRIu32 ", recv:%" PRIu32 "]",
					  tcpServer->GetSendBufferSize(),
					  tcpServer->GetRecvBufferSize());
				}

				// Decrement initial ICE local preference for next IP.
				iceLocalPreferenceDecrement += 100;
			}

			auto iceConsentTimeout = options->iceConsentTimeout();

			// Create a ICE server.
			this->iceServer = new RTC::IceServer(
			  this, Utils::Crypto::GetRandomString(32), Utils::Crypto::GetRandomString(32), iceConsentTimeout);

			// Create a DTLS transport.
			this->dtlsTransport = new RTC::DtlsTransport(this);

			// NOTE: This may throw.
			this->shared->channelMessageRegistrator->RegisterHandler(
			  this->id,
			  /*channelRequestHandler*/ this,
			  /*channelNotificationHandler*/ this);
		}
		catch (const MediaSoupError& error)
		{
			// Must delete everything since the destructor won't be called.

			delete this->dtlsTransport;
			this->dtlsTransport = nullptr;

			delete this->iceServer;
			this->iceServer = nullptr;

			for (auto& kv : this->udpSockets)
			{
				auto* udpSocket = kv.first;

				delete udpSocket;
			}
			this->udpSockets.clear();

			for (auto& kv : this->tcpServers)
			{
				auto* tcpServer = kv.first;

				delete tcpServer;
			}
			this->tcpServers.clear();

			this->iceCandidates.clear();

			throw;
		}
	}

	/**
	 * This constructor is used when the WebRtcTransport uses a WebRtcServer.
	 */
	WebRtcTransport::WebRtcTransport(
	  RTC::Shared* shared,
	  const std::string& id,
	  RTC::Transport::Listener* listener,
	  WebRtcTransportListener* webRtcTransportListener,
	  const std::vector<RTC::IceCandidate>& iceCandidates,
	  const FBS::WebRtcTransport::WebRtcTransportOptions* options)
	  : RTC::Transport::Transport(shared, id, listener, options->base()),
	    webRtcTransportListener(webRtcTransportListener), iceCandidates(iceCandidates)
	{
		MS_TRACE();

		try
		{
			if (iceCandidates.empty())
			{
				MS_THROW_TYPE_ERROR("empty iceCandidates");
			}

			auto iceConsentTimeout = options->iceConsentTimeout();

			// Create a ICE server.
			this->iceServer = new RTC::IceServer(
			  this, Utils::Crypto::GetRandomString(32), Utils::Crypto::GetRandomString(32), iceConsentTimeout);

			// Create a DTLS transport.
			this->dtlsTransport = new RTC::DtlsTransport(this);

			// Notify the webRtcTransportListener.
			this->webRtcTransportListener->OnWebRtcTransportCreated(this);

			// NOTE: This may throw.
			this->shared->channelMessageRegistrator->RegisterHandler(
			  this->id,
			  /*channelRequestHandler*/ this,
			  /*channelNotificationHandler*/ this);
		}
		catch (const MediaSoupError& error)
		{
			// Must delete everything since the destructor won't be called.

			delete this->dtlsTransport;
			this->dtlsTransport = nullptr;

			delete this->iceServer;
			this->iceServer = nullptr;

			throw;
		}
	}

	WebRtcTransport::~WebRtcTransport()
	{
		MS_TRACE();

		// We need to tell the Transport parent class that we are about to destroy
		// the class instance. This is because child's destructor runs before
		// parent's destructor. See comment in Transport::OnSctpAssociationSendData().
		Destroying();

		this->shared->channelMessageRegistrator->UnregisterHandler(this->id);

		// Must delete the DTLS transport first since it will generate a DTLS alert
		// to be sent.
		delete this->dtlsTransport;
		this->dtlsTransport = nullptr;

		delete this->iceServer;
		this->iceServer = nullptr;

		for (auto& kv : this->udpSockets)
		{
			auto* udpSocket = kv.first;

			delete udpSocket;
		}
		this->udpSockets.clear();

		for (auto& kv : this->tcpServers)
		{
			auto* tcpServer = kv.first;

			delete tcpServer;
		}
		this->tcpServers.clear();

		this->iceCandidates.clear();

		delete this->srtpSendSession;
		this->srtpSendSession = nullptr;

		delete this->srtpRecvSession;
		this->srtpRecvSession = nullptr;

		// Notify the webRtcTransportListener.
		if (this->webRtcTransportListener)
		{
			this->webRtcTransportListener->OnWebRtcTransportClosed(this);
		}
	}

	flatbuffers::Offset<FBS::WebRtcTransport::DumpResponse> WebRtcTransport::FillBuffer(
	  flatbuffers::FlatBufferBuilder& builder) const
	{
		MS_TRACE();

		// Add iceParameters.
		auto iceParameters = FBS::WebRtcTransport::CreateIceParametersDirect(
		  builder,
		  this->iceServer->GetUsernameFragment().c_str(),
		  this->iceServer->GetPassword().c_str(),
		  true);

		std::vector<flatbuffers::Offset<FBS::WebRtcTransport::IceCandidate>> iceCandidates;
		iceCandidates.reserve(this->iceCandidates.size());

		for (const auto& iceCandidate : this->iceCandidates)
		{
			iceCandidates.emplace_back(iceCandidate.FillBuffer(builder));
		}

		// Add iceState.
		auto iceState = RTC::IceServer::IceStateToFbs(this->iceServer->GetState());

		// Add iceSelectedTuple.
		flatbuffers::Offset<FBS::Transport::Tuple> iceSelectedTuple;

		if (this->iceServer->GetSelectedTuple())
		{
			iceSelectedTuple = this->iceServer->GetSelectedTuple()->FillBuffer(builder);
		}

		// Add dtlsParameters.fingerprints.
		std::vector<flatbuffers::Offset<FBS::WebRtcTransport::Fingerprint>> fingerprints;

		for (const auto& fingerprint : RTC::DtlsTransport::GetLocalFingerprints())
		{
			auto algorithm    = DtlsTransport::AlgorithmToFbs(fingerprint.algorithm);
			const auto& value = fingerprint.value;

			fingerprints.emplace_back(
			  FBS::WebRtcTransport::CreateFingerprintDirect(builder, algorithm, value.c_str()));
		}

		// Add dtlsParameters.role.
		auto dtlsRole  = DtlsTransport::RoleToFbs(this->dtlsRole);
		auto dtlsState = DtlsTransport::StateToFbs(this->dtlsTransport->GetState());

		// Add base transport dump.
		auto base = Transport::FillBuffer(builder);
		// Add dtlsParameters.
		auto dtlsParameters =
		  FBS::WebRtcTransport::CreateDtlsParametersDirect(builder, &fingerprints, dtlsRole);

		return FBS::WebRtcTransport::CreateDumpResponseDirect(
		  builder,
		  base,
		  FBS::WebRtcTransport::IceRole::CONTROLLED,
		  iceParameters,
		  &iceCandidates,
		  iceState,
		  iceSelectedTuple,
		  dtlsParameters,
		  dtlsState);
	}

	flatbuffers::Offset<FBS::WebRtcTransport::GetStatsResponse> WebRtcTransport::FillBufferStats(
	  flatbuffers::FlatBufferBuilder& builder)
	{
		MS_TRACE();

		// Add iceState.
		auto iceState = RTC::IceServer::IceStateToFbs(this->iceServer->GetState());

		// Add iceSelectedTuple.
		flatbuffers::Offset<FBS::Transport::Tuple> iceSelectedTuple;

		if (this->iceServer->GetSelectedTuple())
		{
			iceSelectedTuple = this->iceServer->GetSelectedTuple()->FillBuffer(builder);
		}

		auto dtlsState = DtlsTransport::StateToFbs(this->dtlsTransport->GetState());

		// Base Transport stats.
		auto base = Transport::FillBufferStats(builder);

		return FBS::WebRtcTransport::CreateGetStatsResponse(
		  builder,
		  base,
		  // iceRole (we are always "controlled").
		  FBS::WebRtcTransport::IceRole::CONTROLLED,
		  iceState,
		  iceSelectedTuple,
		  dtlsState);
	}

	void WebRtcTransport::HandleRequest(Channel::ChannelRequest* request)
	{
		MS_TRACE();

		switch (request->method)
		{
			case Channel::ChannelRequest::Method::TRANSPORT_GET_STATS:
			{
				auto responseOffset = FillBufferStats(request->GetBufferBuilder());

				request->Accept(FBS::Response::Body::WebRtcTransport_GetStatsResponse, responseOffset);

				break;
			}

			case Channel::ChannelRequest::Method::TRANSPORT_DUMP:
			{
				auto dumpOffset = FillBuffer(request->GetBufferBuilder());

				request->Accept(FBS::Response::Body::WebRtcTransport_DumpResponse, dumpOffset);

				break;
			}

			case Channel::ChannelRequest::Method::WEBRTCTRANSPORT_CONNECT:
			{
				// Ensure this method is not called twice.
				if (this->connectCalled)
				{
					MS_THROW_ERROR("connect() already called");
				}

				const auto* body = request->data->body_as<FBS::WebRtcTransport::ConnectRequest>();

				const auto* dtlsParameters = body->dtlsParameters();

				RTC::DtlsTransport::Fingerprint dtlsRemoteFingerprint;
				RTC::DtlsTransport::Role dtlsRemoteRole;

				if (dtlsParameters->fingerprints()->size() == 0)
				{
					MS_THROW_TYPE_ERROR("empty dtlsParameters.fingerprints array");
				}

				// NOTE: Just take the first fingerprint.
				for (const auto& fingerprint : *dtlsParameters->fingerprints())
				{
					dtlsRemoteFingerprint.algorithm = DtlsTransport::AlgorithmFromFbs(fingerprint->algorithm());

					dtlsRemoteFingerprint.value = fingerprint->value()->str();

					// Just use the first fingerprint.
					break;
				}

				dtlsRemoteRole = RTC::DtlsTransport::RoleFromFbs(dtlsParameters->role());

				// Set local DTLS role.
				switch (dtlsRemoteRole)
				{
					case RTC::DtlsTransport::Role::CLIENT:
					{
						this->dtlsRole = RTC::DtlsTransport::Role::SERVER;

						break;
					}
					// If the peer has role "auto" we become "client" since we are ICE controlled.
					case RTC::DtlsTransport::Role::SERVER:
					case RTC::DtlsTransport::Role::AUTO:
					{
						this->dtlsRole = RTC::DtlsTransport::Role::CLIENT;

						break;
					}
				}

				this->connectCalled = true;

				// Pass the remote fingerprint to the DTLS transport.
				if (this->dtlsTransport->SetRemoteFingerprint(dtlsRemoteFingerprint))
				{
					// If everything is fine, we may run the DTLS transport if ready.
					MayRunDtlsTransport();
				}

				// Tell the caller about the selected local DTLS role.
				auto dtlsLocalRole = DtlsTransport::RoleToFbs(this->dtlsRole);

				auto responseOffset =
				  FBS::WebRtcTransport::CreateConnectResponse(request->GetBufferBuilder(), dtlsLocalRole);

				request->Accept(FBS::Response::Body::WebRtcTransport_ConnectResponse, responseOffset);

				break;
			}

			case Channel::ChannelRequest::Method::TRANSPORT_RESTART_ICE:
			{
				const std::string usernameFragment = Utils::Crypto::GetRandomString(32);
				const std::string password         = Utils::Crypto::GetRandomString(32);

				this->iceServer->RestartIce(usernameFragment, password);

				MS_DEBUG_DEV(
				  "WebRtcTransport ICE usernameFragment and password changed [id:%s]", this->id.c_str());

				// Reply with the updated ICE local parameters.
				auto responseOffset = FBS::Transport::CreateRestartIceResponseDirect(
				  request->GetBufferBuilder(),
				  this->iceServer->GetUsernameFragment().c_str(),
				  this->iceServer->GetPassword().c_str(),
				  true /* iceLite */
				);

				request->Accept(FBS::Response::Body::Transport_RestartIceResponse, responseOffset);

				break;
			}

			default:
			{
				// Pass it to the parent class.
				RTC::Transport::HandleRequest(request);
			}
		}
	}

	void WebRtcTransport::HandleNotification(Channel::ChannelNotification* notification)
	{
		MS_TRACE();

		// Pass it to the parent class.
		RTC::Transport::HandleNotification(notification);
	}

	void WebRtcTransport::ProcessStunPacketFromWebRtcServer(
	  RTC::TransportTuple* tuple, RTC::StunPacket* packet)
	{
		MS_TRACE();

		// Pass it to the IceServer.
		this->iceServer->ProcessStunPacket(packet, tuple);
	}

	void WebRtcTransport::ProcessNonStunPacketFromWebRtcServer(
	  RTC::TransportTuple* tuple, const uint8_t* data, size_t len)
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
		// Check if it's DTLS.
		else if (RTC::DtlsTransport::IsDtls(data, len))
		{
			OnDtlsDataReceived(tuple, data, len);
		}
		else
		{
			MS_WARN_DEV("ignoring received packet of unknown type");
		}
	}

	void WebRtcTransport::RemoveTuple(RTC::TransportTuple* tuple)
	{
		MS_TRACE();

		this->iceServer->RemoveTuple(tuple);
	}

	inline bool WebRtcTransport::IsConnected() const
	{
		MS_TRACE();

		// clang-format off
		return (
			(
				this->iceServer->GetState() == RTC::IceServer::IceState::CONNECTED ||
				this->iceServer->GetState() == RTC::IceServer::IceState::COMPLETED
			) &&
			this->dtlsTransport->GetState() == RTC::DtlsTransport::DtlsState::CONNECTED
		);
		// clang-format on
	}

	void WebRtcTransport::MayRunDtlsTransport()
	{
		MS_TRACE();

		// Do nothing if we have the same local DTLS role as the DTLS transport.
		// NOTE: local role in DTLS transport can be NONE, but not ours.
		if (this->dtlsTransport->GetLocalRole() == this->dtlsRole)
		{
			return;
		}

		// Check our local DTLS role.
		switch (this->dtlsRole)
		{
			// If still 'auto' then transition to 'server' if ICE is 'connected' or
			// 'completed'.
			case RTC::DtlsTransport::Role::AUTO:
			{
				// clang-format off
				if (
					this->iceServer->GetState() == RTC::IceServer::IceState::CONNECTED ||
					this->iceServer->GetState() == RTC::IceServer::IceState::COMPLETED
				)
				// clang-format on
				{
					MS_DEBUG_TAG(
					  dtls, "transition from DTLS local role 'auto' to 'server' and running DTLS transport");

					this->dtlsRole = RTC::DtlsTransport::Role::SERVER;
					this->dtlsTransport->Run(RTC::DtlsTransport::Role::SERVER);
				}

				break;
			}

			// 'client' is only set if a 'connect' request was previously called with
			// remote DTLS role 'server'.
			//
			// If 'client' then wait for ICE to be 'completed' (got USE-CANDIDATE).
			//
			// NOTE: This is the theory, however let's be more flexible as told here:
			//   https://bugs.chromium.org/p/webrtc/issues/detail?id=3661
			case RTC::DtlsTransport::Role::CLIENT:
			{
				// clang-format off
				if (
					this->iceServer->GetState() == RTC::IceServer::IceState::CONNECTED ||
					this->iceServer->GetState() == RTC::IceServer::IceState::COMPLETED
				)
				// clang-format on
				{
					MS_DEBUG_TAG(dtls, "running DTLS transport in local role 'client'");

					this->dtlsTransport->Run(RTC::DtlsTransport::Role::CLIENT);
				}

				break;
			}

			// If 'server' then run the DTLS transport if ICE is 'connected' (not yet
			// USE-CANDIDATE) or 'completed'.
			case RTC::DtlsTransport::Role::SERVER:
			{
				// clang-format off
				if (
					this->iceServer->GetState() == RTC::IceServer::IceState::CONNECTED ||
					this->iceServer->GetState() == RTC::IceServer::IceState::COMPLETED
				)
				// clang-format on
				{
					MS_DEBUG_TAG(dtls, "running DTLS transport in local role 'server'");

					this->dtlsTransport->Run(RTC::DtlsTransport::Role::SERVER);
				}

				break;
			}
		}
	}

	void WebRtcTransport::SendRtpPacket(
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

		// Ensure there is sending SRTP session.
		if (!this->srtpSendSession)
		{
			MS_WARN_DEV("ignoring RTP packet due to non sending SRTP session");

			if (cb)
			{
				(*cb)(false);
				delete cb;
			}

			return;
		}

		const uint8_t* data = packet->GetData();
		auto len            = packet->GetSize();

		if (!this->srtpSendSession->EncryptRtp(&data, &len))
		{
			if (cb)
			{
				(*cb)(false);
				delete cb;
			}

			return;
		}

		this->iceServer->GetSelectedTuple()->Send(data, len, cb);

		// Increase send transmission.
		RTC::Transport::DataSent(len);
	}

	void WebRtcTransport::SendRtcpPacket(RTC::RTCP::Packet* packet)
	{
		MS_TRACE();

		if (!IsConnected())
		{
			return;
		}

		const uint8_t* data = packet->GetData();
		auto len            = packet->GetSize();

		// Ensure there is sending SRTP session.
		if (!this->srtpSendSession)
		{
			MS_WARN_DEV("ignoring RTCP packet due to non sending SRTP session");

			return;
		}

		if (!this->srtpSendSession->EncryptRtcp(&data, &len))
		{
			return;
		}

		this->iceServer->GetSelectedTuple()->Send(data, len);

		// Increase send transmission.
		RTC::Transport::DataSent(len);
	}

	void WebRtcTransport::SendRtcpCompoundPacket(RTC::RTCP::CompoundPacket* packet)
	{
		MS_TRACE();

		if (!IsConnected())
		{
			return;
		}

		packet->Serialize(RTC::RTCP::Buffer);

		const uint8_t* data = packet->GetData();
		auto len            = packet->GetSize();

		// Ensure there is sending SRTP session.
		if (!this->srtpSendSession)
		{
			MS_WARN_TAG(rtcp, "ignoring RTCP compound packet due to non sending SRTP session");

			return;
		}

		if (!this->srtpSendSession->EncryptRtcp(&data, &len))
		{
			return;
		}

		this->iceServer->GetSelectedTuple()->Send(data, len);

		// Increase send transmission.
		RTC::Transport::DataSent(len);
	}

	void WebRtcTransport::SendMessage(
	  RTC::DataConsumer* dataConsumer, const uint8_t* msg, size_t len, uint32_t ppid, onQueuedCallback* cb)
	{
		MS_TRACE();

		this->sctpAssociation->SendSctpMessage(dataConsumer, msg, len, ppid, cb);
	}

	void WebRtcTransport::SendSctpData(const uint8_t* data, size_t len)
	{
		MS_TRACE();

		// clang-format on
		if (!IsConnected())
		{
			MS_WARN_TAG(sctp, "DTLS not connected, cannot send SCTP data");

			return;
		}

		this->dtlsTransport->SendApplicationData(data, len);
	}

	void WebRtcTransport::RecvStreamClosed(uint32_t ssrc)
	{
		MS_TRACE();

		if (this->srtpRecvSession)
		{
			this->srtpRecvSession->RemoveStream(ssrc);
		}
	}

	void WebRtcTransport::SendStreamClosed(uint32_t ssrc)
	{
		MS_TRACE();

		if (this->srtpSendSession)
		{
			this->srtpSendSession->RemoveStream(ssrc);
		}
	}

	inline void WebRtcTransport::OnPacketReceived(
	  RTC::TransportTuple* tuple, const uint8_t* data, size_t len)
	{
		MS_TRACE();

		// Increase receive transmission.
		RTC::Transport::DataReceived(len);

		// Check if it's STUN.
		if (RTC::StunPacket::IsStun(data, len))
		{
			OnStunDataReceived(tuple, data, len);
		}
		// Check if it's RTCP.
		else if (RTC::RTCP::Packet::IsRtcp(data, len))
		{
			OnRtcpDataReceived(tuple, data, len);
		}
		// Check if it's RTP.
		else if (RTC::RtpPacket::IsRtp(data, len))
		{
			OnRtpDataReceived(tuple, data, len);
		}
		// Check if it's DTLS.
		else if (RTC::DtlsTransport::IsDtls(data, len))
		{
			OnDtlsDataReceived(tuple, data, len);
		}
		else
		{
			MS_WARN_DEV("ignoring received packet of unknown type");
		}
	}

	inline void WebRtcTransport::OnStunDataReceived(
	  RTC::TransportTuple* tuple, const uint8_t* data, size_t len)
	{
		MS_TRACE();

		RTC::StunPacket* packet = RTC::StunPacket::Parse(data, len);

		if (!packet)
		{
			MS_WARN_DEV("ignoring wrong STUN packet received");

			return;
		}

		// Pass it to the IceServer.
		this->iceServer->ProcessStunPacket(packet, tuple);

		delete packet;
	}

	inline void WebRtcTransport::OnDtlsDataReceived(
	  const RTC::TransportTuple* tuple, const uint8_t* data, size_t len)
	{
		MS_TRACE();

		// Ensure it comes from a valid tuple.
		if (!this->iceServer->IsValidTuple(tuple))
		{
			MS_WARN_TAG(dtls, "ignoring DTLS data coming from an invalid tuple");

			return;
		}

		// Trick for clients performing aggressive ICE regardless we are ICE-Lite.
		this->iceServer->MayForceSelectedTuple(tuple);

		// Check that DTLS status is 'connecting' or 'connected'.
		if (
		  this->dtlsTransport->GetState() == RTC::DtlsTransport::DtlsState::CONNECTING ||
		  this->dtlsTransport->GetState() == RTC::DtlsTransport::DtlsState::CONNECTED)
		{
			MS_DEBUG_DEV("DTLS data received, passing it to the DTLS transport");

			this->dtlsTransport->ProcessDtlsData(data, len);
		}
		else
		{
			MS_WARN_TAG(dtls, "Transport is not 'connecting' or 'connected', ignoring received DTLS data");

			return;
		}
	}

	inline void WebRtcTransport::OnRtpDataReceived(
	  RTC::TransportTuple* tuple, const uint8_t* data, size_t len)
	{
		MS_TRACE();

		// Ensure DTLS is connected.
		if (this->dtlsTransport->GetState() != RTC::DtlsTransport::DtlsState::CONNECTED)
		{
			MS_DEBUG_2TAGS(dtls, rtp, "ignoring RTP packet while DTLS not connected");

			return;
		}

		// Ensure there is receiving SRTP session.
		if (!this->srtpRecvSession)
		{
			MS_DEBUG_TAG(srtp, "ignoring RTP packet due to non receiving SRTP session");

			return;
		}

		// Ensure it comes from a valid tuple.
		if (!this->iceServer->IsValidTuple(tuple))
		{
			MS_WARN_TAG(rtp, "ignoring RTP packet coming from an invalid tuple");

			return;
		}

		// Decrypt the SRTP packet.
		if (!this->srtpRecvSession->DecryptSrtp(const_cast<uint8_t*>(data), &len))
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

		// Trick for clients performing aggressive ICE regardless we are ICE-Lite.
		this->iceServer->MayForceSelectedTuple(tuple);

		// Pass the packet to the parent transport.
		RTC::Transport::ReceiveRtpPacket(packet);
	}

	inline void WebRtcTransport::OnRtcpDataReceived(
	  RTC::TransportTuple* tuple, const uint8_t* data, size_t len)
	{
		MS_TRACE();

		// Ensure DTLS is connected.
		if (this->dtlsTransport->GetState() != RTC::DtlsTransport::DtlsState::CONNECTED)
		{
			MS_DEBUG_2TAGS(dtls, rtcp, "ignoring RTCP packet while DTLS not connected");

			return;
		}

		// Ensure there is receiving SRTP session.
		if (!this->srtpRecvSession)
		{
			MS_DEBUG_TAG(srtp, "ignoring RTCP packet due to non receiving SRTP session");

			return;
		}

		// Ensure it comes from a valid tuple.
		if (!this->iceServer->IsValidTuple(tuple))
		{
			MS_WARN_TAG(rtcp, "ignoring RTCP packet coming from an invalid tuple");

			return;
		}

		// Decrypt the SRTCP packet.
		if (!this->srtpRecvSession->DecryptSrtcp(const_cast<uint8_t*>(data), &len))
		{
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

	inline void WebRtcTransport::OnUdpSocketPacketReceived(
	  RTC::UdpSocket* socket, const uint8_t* data, size_t len, const struct sockaddr* remoteAddr)
	{
		MS_TRACE();

		RTC::TransportTuple tuple(socket, remoteAddr);

		OnPacketReceived(&tuple, data, len);
	}

	inline void WebRtcTransport::OnRtcTcpConnectionClosed(
	  RTC::TcpServer* /*tcpServer*/, RTC::TcpConnection* connection)
	{
		MS_TRACE();

		RTC::TransportTuple tuple(connection);

		this->iceServer->RemoveTuple(&tuple);
	}

	inline void WebRtcTransport::OnTcpConnectionPacketReceived(
	  RTC::TcpConnection* connection, const uint8_t* data, size_t len)
	{
		MS_TRACE();

		RTC::TransportTuple tuple(connection);

		OnPacketReceived(&tuple, data, len);
	}

	inline void WebRtcTransport::OnIceServerSendStunPacket(
	  const RTC::IceServer* /*iceServer*/, const RTC::StunPacket* packet, RTC::TransportTuple* tuple)
	{
		MS_TRACE();

		// Send the STUN response over the same transport tuple.
		tuple->Send(packet->GetData(), packet->GetSize());

		// Increase send transmission.
		RTC::Transport::DataSent(packet->GetSize());
	}

	inline void WebRtcTransport::OnIceServerLocalUsernameFragmentAdded(
	  const RTC::IceServer* /*iceServer*/, const std::string& usernameFragment)
	{
		MS_TRACE();

		if (this->webRtcTransportListener)
		{
			this->webRtcTransportListener->OnWebRtcTransportLocalIceUsernameFragmentAdded(
			  this, usernameFragment);
		}
	}

	inline void WebRtcTransport::OnIceServerLocalUsernameFragmentRemoved(
	  const RTC::IceServer* /*iceServer*/, const std::string& usernameFragment)
	{
		MS_TRACE();

		if (this->webRtcTransportListener)
		{
			this->webRtcTransportListener->OnWebRtcTransportLocalIceUsernameFragmentRemoved(
			  this, usernameFragment);
		}
	}

	inline void WebRtcTransport::OnIceServerTupleAdded(
	  const RTC::IceServer* /*iceServer*/, RTC::TransportTuple* tuple)
	{
		MS_TRACE();

		if (this->webRtcTransportListener)
		{
			this->webRtcTransportListener->OnWebRtcTransportTransportTupleAdded(this, tuple);
		}
	}

	inline void WebRtcTransport::OnIceServerTupleRemoved(
	  const RTC::IceServer* /*iceServer*/, RTC::TransportTuple* tuple)
	{
		MS_TRACE();

		if (this->webRtcTransportListener)
		{
			this->webRtcTransportListener->OnWebRtcTransportTransportTupleRemoved(this, tuple);
		}

		// If this is a TCP tuple, close its underlaying TCP connection.
		if (tuple->GetProtocol() == RTC::TransportTuple::Protocol::TCP)
		{
			tuple->CloseTcpConnection();
		}
	}

	inline void WebRtcTransport::OnIceServerSelectedTuple(
	  const RTC::IceServer* /*iceServer*/, RTC::TransportTuple* /*tuple*/)
	{
		MS_TRACE();

		/*
		 * RFC 5245 section 11.2 "Receiving Media":
		 *
		 * ICE implementations MUST be prepared to receive media on each component
		 * on any candidates provided for that component.
		 */

		MS_DEBUG_TAG(ice, "ICE selected tuple");

		// Notify the Node WebRtcTransport.
		auto tuple = this->iceServer->GetSelectedTuple()->FillBuffer(
		  this->shared->channelNotifier->GetBufferBuilder());

		auto notification = FBS::WebRtcTransport::CreateIceSelectedTupleChangeNotification(
		  this->shared->channelNotifier->GetBufferBuilder(), tuple);

		this->shared->channelNotifier->Emit(
		  this->id,
		  FBS::Notification::Event::WEBRTCTRANSPORT_ICE_SELECTED_TUPLE_CHANGE,
		  FBS::Notification::Body::WebRtcTransport_IceSelectedTupleChangeNotification,
		  notification);
	}

	inline void WebRtcTransport::OnIceServerConnected(const RTC::IceServer* /*iceServer*/)
	{
		MS_TRACE();

		MS_DEBUG_TAG(ice, "ICE connected");

		// Notify the Node WebRtcTransport.
		auto iceStateChangeOffset = FBS::WebRtcTransport::CreateIceStateChangeNotification(
		  this->shared->channelNotifier->GetBufferBuilder(), FBS::WebRtcTransport::IceState::CONNECTED);

		this->shared->channelNotifier->Emit(
		  this->id,
		  FBS::Notification::Event::WEBRTCTRANSPORT_ICE_STATE_CHANGE,
		  FBS::Notification::Body::WebRtcTransport_IceStateChangeNotification,
		  iceStateChangeOffset);

		// If ready, run the DTLS handler.
		MayRunDtlsTransport();

		// If DTLS was already connected, notify the parent class.
		if (this->dtlsTransport->GetState() == RTC::DtlsTransport::DtlsState::CONNECTED)
		{
			RTC::Transport::Connected();
		}
	}

	inline void WebRtcTransport::OnIceServerCompleted(const RTC::IceServer* /*iceServer*/)
	{
		MS_TRACE();

		MS_DEBUG_TAG(ice, "ICE completed");

		// Notify the Node WebRtcTransport.
		auto iceStateChangeOffset = FBS::WebRtcTransport::CreateIceStateChangeNotification(
		  this->shared->channelNotifier->GetBufferBuilder(), FBS::WebRtcTransport::IceState::COMPLETED);

		this->shared->channelNotifier->Emit(
		  this->id,
		  FBS::Notification::Event::WEBRTCTRANSPORT_ICE_STATE_CHANGE,
		  FBS::Notification::Body::WebRtcTransport_IceStateChangeNotification,
		  iceStateChangeOffset);

		// If ready, run the DTLS handler.
		MayRunDtlsTransport();

		// If DTLS was already connected, notify the parent class.
		if (this->dtlsTransport->GetState() == RTC::DtlsTransport::DtlsState::CONNECTED)
		{
			RTC::Transport::Connected();
		}
	}

	inline void WebRtcTransport::OnIceServerDisconnected(const RTC::IceServer* /*iceServer*/)
	{
		MS_TRACE();

		MS_DEBUG_TAG(ice, "ICE disconnected");

		// Notify the Node WebRtcTransport.
		auto iceStateChangeOffset = FBS::WebRtcTransport::CreateIceStateChangeNotification(
		  this->shared->channelNotifier->GetBufferBuilder(),
		  FBS::WebRtcTransport::IceState::DISCONNECTED);

		this->shared->channelNotifier->Emit(
		  this->id,
		  FBS::Notification::Event::WEBRTCTRANSPORT_ICE_STATE_CHANGE,
		  FBS::Notification::Body::WebRtcTransport_IceStateChangeNotification,
		  iceStateChangeOffset);

		// If DTLS was already connected, notify the parent class.
		if (this->dtlsTransport->GetState() == RTC::DtlsTransport::DtlsState::CONNECTED)
		{
			RTC::Transport::Disconnected();
		}
	}

	inline void WebRtcTransport::OnDtlsTransportConnecting(const RTC::DtlsTransport* /*dtlsTransport*/)
	{
		MS_TRACE();

		MS_DEBUG_TAG(dtls, "DTLS connecting");

		// Notify the Node WebRtcTransport.
		auto dtlsStateChangeOffset = FBS::WebRtcTransport::CreateDtlsStateChangeNotification(
		  this->shared->channelNotifier->GetBufferBuilder(), FBS::WebRtcTransport::DtlsState::CONNECTING);

		this->shared->channelNotifier->Emit(
		  this->id,
		  FBS::Notification::Event::WEBRTCTRANSPORT_DTLS_STATE_CHANGE,
		  FBS::Notification::Body::WebRtcTransport_DtlsStateChangeNotification,
		  dtlsStateChangeOffset);
	}

	inline void WebRtcTransport::OnDtlsTransportConnected(
	  const RTC::DtlsTransport* /*dtlsTransport*/,
	  RTC::SrtpSession::CryptoSuite srtpCryptoSuite,
	  uint8_t* srtpLocalKey,
	  size_t srtpLocalKeyLen,
	  uint8_t* srtpRemoteKey,
	  size_t srtpRemoteKeyLen,
	  std::string& remoteCert)
	{
		MS_TRACE();

		MS_DEBUG_TAG(dtls, "DTLS connected");

		// Close it if it was already set and update it.
		delete this->srtpSendSession;
		this->srtpSendSession = nullptr;

		delete this->srtpRecvSession;
		this->srtpRecvSession = nullptr;

		try
		{
			this->srtpSendSession = new RTC::SrtpSession(
			  RTC::SrtpSession::Type::OUTBOUND, srtpCryptoSuite, srtpLocalKey, srtpLocalKeyLen);
		}
		catch (const MediaSoupError& error)
		{
			MS_ERROR("error creating SRTP sending session: %s", error.what());
		}

		try
		{
			this->srtpRecvSession = new RTC::SrtpSession(
			  RTC::SrtpSession::Type::INBOUND, srtpCryptoSuite, srtpRemoteKey, srtpRemoteKeyLen);

			// Notify the Node WebRtcTransport.
			auto dtlsStateChangeOffset = FBS::WebRtcTransport::CreateDtlsStateChangeNotificationDirect(
			  this->shared->channelNotifier->GetBufferBuilder(),
			  FBS::WebRtcTransport::DtlsState::CONNECTED,
			  remoteCert.c_str());

			this->shared->channelNotifier->Emit(
			  this->id,
			  FBS::Notification::Event::WEBRTCTRANSPORT_DTLS_STATE_CHANGE,
			  FBS::Notification::Body::WebRtcTransport_DtlsStateChangeNotification,
			  dtlsStateChangeOffset);

			// Tell the parent class.
			RTC::Transport::Connected();
		}
		catch (const MediaSoupError& error)
		{
			MS_ERROR("error creating SRTP receiving session: %s", error.what());

			delete this->srtpSendSession;
			this->srtpSendSession = nullptr;
		}
	}

	inline void WebRtcTransport::OnDtlsTransportFailed(const RTC::DtlsTransport* /*dtlsTransport*/)
	{
		MS_TRACE();

		MS_WARN_TAG(dtls, "DTLS failed");

		// Notify the Node WebRtcTransport.
		auto dtlsStateChangeOffset = FBS::WebRtcTransport::CreateDtlsStateChangeNotification(
		  this->shared->channelNotifier->GetBufferBuilder(), FBS::WebRtcTransport::DtlsState::FAILED);

		this->shared->channelNotifier->Emit(
		  this->id,
		  FBS::Notification::Event::WEBRTCTRANSPORT_DTLS_STATE_CHANGE,
		  FBS::Notification::Body::WebRtcTransport_DtlsStateChangeNotification,
		  dtlsStateChangeOffset);
	}

	inline void WebRtcTransport::OnDtlsTransportClosed(const RTC::DtlsTransport* /*dtlsTransport*/)
	{
		MS_TRACE();

		MS_WARN_TAG(dtls, "DTLS remotely closed");

		// Notify the Node WebRtcTransport.
		auto dtlsStateChangeOffset = FBS::WebRtcTransport::CreateDtlsStateChangeNotification(
		  this->shared->channelNotifier->GetBufferBuilder(), FBS::WebRtcTransport::DtlsState::CLOSED);

		this->shared->channelNotifier->Emit(
		  this->id,
		  FBS::Notification::Event::WEBRTCTRANSPORT_DTLS_STATE_CHANGE,
		  FBS::Notification::Body::WebRtcTransport_DtlsStateChangeNotification,
		  dtlsStateChangeOffset);

		// Tell the parent class.
		RTC::Transport::Disconnected();
	}

	inline void WebRtcTransport::OnDtlsTransportSendData(
	  const RTC::DtlsTransport* /*dtlsTransport*/, const uint8_t* data, size_t len)
	{
		MS_TRACE();

		if (!this->iceServer->GetSelectedTuple())
		{
			MS_WARN_TAG(dtls, "no selected tuple set, cannot send DTLS packet");

			return;
		}

		this->iceServer->GetSelectedTuple()->Send(data, len);

		// Increase send transmission.
		RTC::Transport::DataSent(len);
	}

	inline void WebRtcTransport::OnDtlsTransportApplicationDataReceived(
	  const RTC::DtlsTransport* /*dtlsTransport*/, const uint8_t* data, size_t len)
	{
		MS_TRACE();

		// Pass it to the parent transport.
		RTC::Transport::ReceiveSctpData(data, len);
	}
} // namespace RTC
