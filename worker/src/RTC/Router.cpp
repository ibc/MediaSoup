#define MS_CLASS "RTC::Router"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/Router.hpp"
#ifdef MS_LIBURING_SUPPORTED
#include "DepLibUring.hpp"
#endif
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include "RTC/ActiveSpeakerObserver.hpp"
#include "RTC/AudioLevelObserver.hpp"
#include "RTC/DirectTransport.hpp"
#include "RTC/PipeTransport.hpp"
#include "RTC/PlainTransport.hpp"
#include "RTC/WebRtcTransport.hpp"

namespace RTC
{
	/* Instance methods. */

	Router::Router(RTC::Shared* shared, const std::string& id, Listener* listener)
	  : id(id), shared(shared), listener(listener)
	{
		MS_TRACE();

		// NOTE: This may throw.
		this->shared->channelMessageRegistrator->RegisterHandler(
		  this->id,
		  /*channelRequestHandler*/ this,
		  /*ChannelNotificationHandler*/ nullptr);
	}

	Router::~Router()
	{
		MS_TRACE();

		this->shared->channelMessageRegistrator->UnregisterHandler(this->id);

		// Close all Transports.
		for (auto& kv : this->mapTransports)
		{
			auto* transport = kv.second;

			delete transport;
		}
		this->mapTransports.clear();

		// Close all RtpObservers.
		for (auto& kv : this->mapRtpObservers)
		{
			auto* rtpObserver = kv.second;

			delete rtpObserver;
		}
		this->mapRtpObservers.clear();

		// Clear other maps.
		this->mapProducerConsumers.clear();
		this->mapConsumerProducer.clear();
		this->mapProducerRtpObservers.clear();
		this->mapProducers.clear();
		this->mapDataProducerDataConsumers.clear();
		this->mapDataConsumerDataProducer.clear();
		this->mapDataProducers.clear();
	}

	flatbuffers::Offset<FBS::Router::DumpResponse> Router::FillBuffer(
	  flatbuffers::FlatBufferBuilder& builder) const
	{
		MS_TRACE();

		// Add transportIds.
		std::vector<flatbuffers::Offset<flatbuffers::String>> transportIds;
		transportIds.reserve(this->mapTransports.size());

		for (const auto& kv : this->mapTransports)
		{
			const auto& transportId = kv.first;

			transportIds.push_back(builder.CreateString(transportId));
		}

		// Add rtpObserverIds.
		std::vector<flatbuffers::Offset<flatbuffers::String>> rtpObserverIds;
		rtpObserverIds.reserve(this->mapRtpObservers.size());

		for (const auto& kv : this->mapRtpObservers)
		{
			const auto& rtpObserverId = kv.first;

			rtpObserverIds.push_back(builder.CreateString(rtpObserverId));
		}

		// Add mapProducerIdConsumerIds.
		std::vector<flatbuffers::Offset<FBS::Common::StringStringArray>> mapProducerIdConsumerIds;
		mapProducerIdConsumerIds.reserve(this->mapProducerConsumers.size());

		for (const auto& kv : this->mapProducerConsumers)
		{
			auto* producer        = kv.first;
			const auto& consumers = kv.second;

			std::vector<flatbuffers::Offset<flatbuffers::String>> consumerIds;
			consumerIds.reserve(consumers.size());

			for (auto* consumer : consumers)
			{
				consumerIds.emplace_back(builder.CreateString(consumer->id));
			}

			mapProducerIdConsumerIds.emplace_back(
			  FBS::Common::CreateStringStringArrayDirect(builder, producer->id.c_str(), &consumerIds));
		}

		// Add mapConsumerIdProducerId.
		std::vector<flatbuffers::Offset<FBS::Common::StringString>> mapConsumerIdProducerId;
		mapConsumerIdProducerId.reserve(this->mapConsumerProducer.size());

		for (const auto& kv : this->mapConsumerProducer)
		{
			auto* consumer = kv.first;
			auto* producer = kv.second;

			mapConsumerIdProducerId.emplace_back(
			  FBS::Common::CreateStringStringDirect(builder, consumer->id.c_str(), producer->id.c_str()));
		}

		// Add mapProducerIdObserverIds.
		std::vector<flatbuffers::Offset<FBS::Common::StringStringArray>> mapProducerIdObserverIds;
		mapProducerIdObserverIds.reserve(this->mapProducerRtpObservers.size());

		for (const auto& kv : this->mapProducerRtpObservers)
		{
			auto* producer           = kv.first;
			const auto& rtpObservers = kv.second;

			std::vector<flatbuffers::Offset<flatbuffers::String>> observerIds;
			observerIds.reserve(rtpObservers.size());

			for (auto* rtpObserver : rtpObservers)
			{
				observerIds.emplace_back(builder.CreateString(rtpObserver->id));
			}

			mapProducerIdObserverIds.emplace_back(
			  FBS::Common::CreateStringStringArrayDirect(builder, producer->id.c_str(), &observerIds));
		}

		// Add mapDataProducerIdDataConsumerIds.
		std::vector<flatbuffers::Offset<FBS::Common::StringStringArray>> mapDataProducerIdDataConsumerIds;
		mapDataProducerIdDataConsumerIds.reserve(this->mapDataProducerDataConsumers.size());

		for (const auto& kv : this->mapDataProducerDataConsumers)
		{
			auto* dataProducer        = kv.first;
			const auto& dataConsumers = kv.second;

			std::vector<flatbuffers::Offset<flatbuffers::String>> dataConsumerIds;
			dataConsumerIds.reserve(dataConsumers.size());

			for (auto* dataConsumer : dataConsumers)
			{
				dataConsumerIds.emplace_back(builder.CreateString(dataConsumer->id));
			}

			mapDataProducerIdDataConsumerIds.emplace_back(FBS::Common::CreateStringStringArrayDirect(
			  builder, dataProducer->id.c_str(), &dataConsumerIds));
		}

		// Add mapDataConsumerIdDataProducerId.
		std::vector<flatbuffers::Offset<FBS::Common::StringString>> mapDataConsumerIdDataProducerId;
		mapDataConsumerIdDataProducerId.reserve(this->mapDataConsumerDataProducer.size());

		for (const auto& kv : this->mapDataConsumerDataProducer)
		{
			auto* dataConsumer = kv.first;
			auto* dataProducer = kv.second;

			mapDataConsumerIdDataProducerId.emplace_back(FBS::Common::CreateStringStringDirect(
			  builder, dataConsumer->id.c_str(), dataProducer->id.c_str()));
		}

		return FBS::Router::CreateDumpResponseDirect(
		  builder,
		  this->id.c_str(),
		  &transportIds,
		  &rtpObserverIds,
		  &mapProducerIdConsumerIds,
		  &mapConsumerIdProducerId,
		  &mapProducerIdObserverIds,
		  &mapDataProducerIdDataConsumerIds,
		  &mapDataConsumerIdDataProducerId);
	}

	void Router::HandleRequest(Channel::ChannelRequest* request)
	{
		MS_TRACE();

		switch (request->method)
		{
			case Channel::ChannelRequest::Method::ROUTER_DUMP:
			{
				auto dumpOffset = FillBuffer(request->GetBufferBuilder());

				request->Accept(FBS::Response::Body::Router_DumpResponse, dumpOffset);

				break;
			}

			case Channel::ChannelRequest::Method::ROUTER_CREATE_WEBRTCTRANSPORT:
			{
				const auto* body = request->data->body_as<FBS::Router::CreateWebRtcTransportRequest>();

				auto transportId = body->transportId()->str();

				// This may throw.
				CheckNoTransport(transportId);

				// This may throw.
				auto* webRtcTransport =
				  new RTC::WebRtcTransport(this->shared, transportId, this, body->options());

				// Insert into the map.
				this->mapTransports[transportId] = webRtcTransport;

				MS_DEBUG_DEV("WebRtcTransport created [transportId:%s]", transportId.c_str());

				auto dumpOffset = webRtcTransport->FillBuffer(request->GetBufferBuilder());

				request->Accept(FBS::Response::Body::WebRtcTransport_DumpResponse, dumpOffset);

				break;
			}

			case Channel::ChannelRequest::Method::ROUTER_CREATE_WEBRTCTRANSPORT_WITH_SERVER:
			{
				const auto* body = request->data->body_as<FBS::Router::CreateWebRtcTransportRequest>();
				auto transportId = body->transportId()->str();

				// This may throw.
				CheckNoTransport(transportId);

				const auto* options    = body->options();
				const auto* listenInfo = options->listen_as<FBS::WebRtcTransport::ListenServer>();

				auto webRtcServerId = listenInfo->webRtcServerId()->str();

				auto* webRtcServer = this->listener->OnRouterNeedWebRtcServer(this, webRtcServerId);

				if (!webRtcServer)
				{
					MS_THROW_ERROR("wrong webRtcServerId (no associated WebRtcServer found)");
				}

				auto iceCandidates = webRtcServer->GetIceCandidates(
				  options->enableUdp(), options->enableTcp(), options->preferUdp(), options->preferTcp());

				// This may throw.
				auto* webRtcTransport = new RTC::WebRtcTransport(
				  this->shared, transportId, this, webRtcServer, iceCandidates, options);

				// Insert into the map.
				this->mapTransports[transportId] = webRtcTransport;

				MS_DEBUG_DEV(
				  "WebRtcTransport with WebRtcServer created [transportId:%s]", transportId.c_str());

				auto dumpOffset = webRtcTransport->FillBuffer(request->GetBufferBuilder());

				request->Accept(FBS::Response::Body::WebRtcTransport_DumpResponse, dumpOffset);

				break;
			}

			case Channel::ChannelRequest::Method::ROUTER_CREATE_PLAINTRANSPORT:
			{
				const auto* body = request->data->body_as<FBS::Router::CreatePlainTransportRequest>();
				auto transportId = body->transportId()->str();

				// This may throw.
				CheckNoTransport(transportId);

				auto* plainTransport =
				  new RTC::PlainTransport(this->shared, transportId, this, body->options());

				// Insert into the map.
				this->mapTransports[transportId] = plainTransport;

				MS_DEBUG_DEV("PlainTransport created [transportId:%s]", transportId.c_str());

				auto dumpOffset = plainTransport->FillBuffer(request->GetBufferBuilder());

				request->Accept(FBS::Response::Body::PlainTransport_DumpResponse, dumpOffset);

				break;
			}

			case Channel::ChannelRequest::Method::ROUTER_CREATE_PIPETRANSPORT:
			{
				const auto* body = request->data->body_as<FBS::Router::CreatePipeTransportRequest>();
				auto transportId = body->transportId()->str();

				// This may throw.
				CheckNoTransport(transportId);

				auto* pipeTransport =
				  new RTC::PipeTransport(this->shared, transportId, this, body->options());

				// Insert into the map.
				this->mapTransports[transportId] = pipeTransport;

				MS_DEBUG_DEV("PipeTransport created [transportId:%s]", transportId.c_str());

				auto dumpOffset = pipeTransport->FillBuffer(request->GetBufferBuilder());

				request->Accept(FBS::Response::Body::PipeTransport_DumpResponse, dumpOffset);

				break;
			}

			case Channel::ChannelRequest::Method::ROUTER_CREATE_DIRECTTRANSPORT:
			{
				const auto* body = request->data->body_as<FBS::Router::CreateDirectTransportRequest>();
				auto transportId = body->transportId()->str();

				// This may throw.
				CheckNoTransport(transportId);

				auto* directTransport =
				  new RTC::DirectTransport(this->shared, transportId, this, body->options());

				// Insert into the map.
				this->mapTransports[transportId] = directTransport;

				MS_DEBUG_DEV("DirectTransport created [transportId:%s]", transportId.c_str());

				auto dumpOffset = directTransport->FillBuffer(request->GetBufferBuilder());

				request->Accept(FBS::Response::Body::DirectTransport_DumpResponse, dumpOffset);

				break;
			}

			case Channel::ChannelRequest::Method::ROUTER_CREATE_ACTIVESPEAKEROBSERVER:
			{
				const auto* body = request->data->body_as<FBS::Router::CreateActiveSpeakerObserverRequest>();
				auto rtpObserverId = body->rtpObserverId()->str();

				// This may throw.
				CheckNoRtpObserver(rtpObserverId);

				auto* activeSpeakerObserver =
				  new RTC::ActiveSpeakerObserver(this->shared, rtpObserverId, this, body->options());

				// Insert into the map.
				this->mapRtpObservers[rtpObserverId] = activeSpeakerObserver;

				MS_DEBUG_DEV("ActiveSpeakerObserver created [rtpObserverId:%s]", rtpObserverId.c_str());

				request->Accept();

				break;
			}

			case Channel::ChannelRequest::Method::ROUTER_CREATE_AUDIOLEVELOBSERVER:
			{
				const auto* body   = request->data->body_as<FBS::Router::CreateAudioLevelObserverRequest>();
				auto rtpObserverId = body->rtpObserverId()->str();

				// This may throw.
				CheckNoRtpObserver(rtpObserverId);

				auto* audioLevelObserver =
				  new RTC::AudioLevelObserver(this->shared, rtpObserverId, this, body->options());

				// Insert into the map.
				this->mapRtpObservers[rtpObserverId] = audioLevelObserver;

				MS_DEBUG_DEV("AudioLevelObserver created [rtpObserverId:%s]", rtpObserverId.c_str());

				request->Accept();

				break;
			}

			case Channel::ChannelRequest::Method::ROUTER_CLOSE_TRANSPORT:
			{
				const auto* body = request->data->body_as<FBS::Router::CloseTransportRequest>();
				auto transportId = body->transportId()->str();

				// This may throw.
				RTC::Transport* transport = GetTransportById(transportId);

				// Tell the Transport to close all its Producers and Consumers so it will
				// notify us about their closures.
				transport->CloseProducersAndConsumers();

				// Remove it from the map.
				this->mapTransports.erase(transport->id);

				MS_DEBUG_DEV("Transport closed [transportId:%s]", transport->id.c_str());

				// Delete it.
				delete transport;

				request->Accept();

				break;
			}

			case Channel::ChannelRequest::Method::ROUTER_CLOSE_RTPOBSERVER:
			{
				const auto* body   = request->data->body_as<FBS::Router::CloseRtpObserverRequest>();
				auto rtpObserverId = body->rtpObserverId()->str();

				// This may throw.
				RTC::RtpObserver* rtpObserver = GetRtpObserverById(rtpObserverId);

				// Remove it from the map.
				this->mapRtpObservers.erase(rtpObserver->id);

				// Iterate all entries in mapProducerRtpObservers and remove the closed one.
				for (auto& kv : this->mapProducerRtpObservers)
				{
					auto& rtpObservers = kv.second;

					rtpObservers.erase(rtpObserver);
				}

				MS_DEBUG_DEV("RtpObserver closed [rtpObserverId:%s]", rtpObserver->id.c_str());

				// Delete it.
				delete rtpObserver;

				request->Accept();

				break;
			}

			default:
			{
				MS_THROW_ERROR("unknown method '%s'", Channel::ChannelRequest::method2String[request->method]);
			}
		}
	}

	void Router::CheckNoTransport(const std::string& transportId) const
	{
		if (this->mapTransports.find(transportId) != this->mapTransports.end())
		{
			MS_THROW_ERROR("a Transport with same id already exists");
		}
	}

	void Router::CheckNoRtpObserver(const std::string& rtpObserverId) const
	{
		if (this->mapRtpObservers.find(rtpObserverId) != this->mapRtpObservers.end())
		{
			MS_THROW_ERROR("an RtpObserver with same id already exists");
		}
	}

	RTC::Transport* Router::GetTransportById(const std::string& transportId) const
	{
		MS_TRACE();

		auto it = this->mapTransports.find(transportId);

		if (this->mapTransports.find(transportId) == this->mapTransports.end())
		{
			MS_THROW_ERROR("Transport not found");
		}

		return it->second;
	}

	RTC::RtpObserver* Router::GetRtpObserverById(const std::string& rtpObserverId) const
	{
		MS_TRACE();

		auto it = this->mapRtpObservers.find(rtpObserverId);

		if (this->mapRtpObservers.find(rtpObserverId) == this->mapRtpObservers.end())
		{
			MS_THROW_ERROR("RtpObserver not found");
		}

		return it->second;
	}

	inline void Router::OnTransportNewProducer(RTC::Transport* /*transport*/, RTC::Producer* producer)
	{
		MS_TRACE();

		MS_ASSERT(
		  this->mapProducerConsumers.find(producer) == this->mapProducerConsumers.end(),
		  "Producer already present in mapProducerConsumers");

		if (this->mapProducers.find(producer->id) != this->mapProducers.end())
		{
			MS_THROW_ERROR("Producer already present in mapProducers [producerId:%s]", producer->id.c_str());
		}

		// Insert the Producer in the maps.
		this->mapProducers[producer->id] = producer;
		this->mapProducerConsumers[producer];
		this->mapProducerRtpObservers[producer];
	}

	inline void Router::OnTransportProducerClosed(RTC::Transport* /*transport*/, RTC::Producer* producer)
	{
		MS_TRACE();

		auto mapProducerConsumersIt    = this->mapProducerConsumers.find(producer);
		auto mapProducersIt            = this->mapProducers.find(producer->id);
		auto mapProducerRtpObserversIt = this->mapProducerRtpObservers.find(producer);

		MS_ASSERT(
		  mapProducerConsumersIt != this->mapProducerConsumers.end(),
		  "Producer not present in mapProducerConsumers");
		MS_ASSERT(mapProducersIt != this->mapProducers.end(), "Producer not present in mapProducers");
		MS_ASSERT(
		  mapProducerRtpObserversIt != this->mapProducerRtpObservers.end(),
		  "Producer not present in mapProducerRtpObservers");

		// Close all Consumers associated to the closed Producer.
		auto& consumers = mapProducerConsumersIt->second;

		// NOTE: While iterating the set of Consumers, we call ProducerClosed() on each
		// one, which will end calling Router::OnTransportConsumerProducerClosed(),
		// which will remove the Consumer from mapConsumerProducer but won't remove the
		// closed Consumer from the set of Consumers in mapProducerConsumers (here will
		// erase the complete entry in that map).
		for (auto* consumer : consumers)
		{
			// Call consumer->ProducerClosed() so the Consumer will notify the Node process,
			// will notify its Transport, and its Transport will delete the Consumer.
			consumer->ProducerClosed();
		}

		// Tell all RtpObservers that the Producer has been closed.
		auto& rtpObservers = mapProducerRtpObserversIt->second;

		for (auto* rtpObserver : rtpObservers)
		{
			rtpObserver->RemoveProducer(producer);
		}

		// Remove the Producer from the maps.
		this->mapProducers.erase(mapProducersIt);
		this->mapProducerConsumers.erase(mapProducerConsumersIt);
		this->mapProducerRtpObservers.erase(mapProducerRtpObserversIt);
	}

	inline void Router::OnTransportProducerPaused(RTC::Transport* /*transport*/, RTC::Producer* producer)
	{
		MS_TRACE();

		auto& consumers = this->mapProducerConsumers.at(producer);

		for (auto* consumer : consumers)
		{
			consumer->ProducerPaused();
		}

		auto it = this->mapProducerRtpObservers.find(producer);

		if (it != this->mapProducerRtpObservers.end())
		{
			auto& rtpObservers = it->second;

			for (auto* rtpObserver : rtpObservers)
			{
				rtpObserver->ProducerPaused(producer);
			}
		}
	}

	inline void Router::OnTransportProducerResumed(RTC::Transport* /*transport*/, RTC::Producer* producer)
	{
		MS_TRACE();

		auto& consumers = this->mapProducerConsumers.at(producer);

		for (auto* consumer : consumers)
		{
			consumer->ProducerResumed();
		}

		auto it = this->mapProducerRtpObservers.find(producer);

		if (it != this->mapProducerRtpObservers.end())
		{
			auto& rtpObservers = it->second;

			for (auto* rtpObserver : rtpObservers)
			{
				rtpObserver->ProducerResumed(producer);
			}
		}
	}

	inline void Router::OnTransportProducerNewRtpStream(
	  RTC::Transport* /*transport*/,
	  RTC::Producer* producer,
	  RTC::RtpStreamRecv* rtpStream,
	  uint32_t mappedSsrc)
	{
		MS_TRACE();

		auto& consumers = this->mapProducerConsumers.at(producer);

		for (auto* consumer : consumers)
		{
			consumer->ProducerNewRtpStream(rtpStream, mappedSsrc);
		}
	}

	inline void Router::OnTransportProducerRtpStreamScore(
	  RTC::Transport* /*transport*/,
	  RTC::Producer* producer,
	  RTC::RtpStreamRecv* rtpStream,
	  uint8_t score,
	  uint8_t previousScore)
	{
		MS_TRACE();

		auto& consumers = this->mapProducerConsumers.at(producer);

		for (auto* consumer : consumers)
		{
			consumer->ProducerRtpStreamScore(rtpStream, score, previousScore);
		}
	}

	inline void Router::OnTransportProducerRtcpSenderReport(
	  RTC::Transport* /*transport*/, RTC::Producer* producer, RTC::RtpStreamRecv* rtpStream, bool first)
	{
		MS_TRACE();

		auto& consumers = this->mapProducerConsumers.at(producer);

		for (auto* consumer : consumers)
		{
			consumer->ProducerRtcpSenderReport(rtpStream, first);
		}
	}

	inline void Router::OnTransportProducerRtpPacketReceived(
	  RTC::Transport* /*transport*/, RTC::Producer* producer, RTC::RtpPacket* packet)
	{
		MS_TRACE();

#ifdef MS_RTC_LOGGER_RTP
		packet->logger.routerId = this->id;
#endif

		auto& consumers = this->mapProducerConsumers.at(producer);

		if (!consumers.empty())
		{
			// Cloned ref-counted packet that RtpStreamSend will store for as long as
			// needed avoiding multiple allocations unless absolutely necessary.
			// Clone only happens if needed.
			std::shared_ptr<RTC::RtpPacket> sharedPacket;

#ifdef MS_LIBURING_SUPPORTED
			if (DepLibUring::IsEnabled())
			{
				// Activate liburing usage.
				DepLibUring::SetActive();
			}
#endif

			for (auto* consumer : consumers)
			{
				// Update MID RTP extension value.
				const auto& mid = consumer->GetRtpParameters().mid;

				if (!mid.empty())
				{
					packet->UpdateMid(mid);
				}

				consumer->SendRtpPacket(packet, sharedPacket);
			}

#ifdef MS_LIBURING_SUPPORTED
			if (DepLibUring::IsEnabled())
			{
				// Submit all prepared submission entries.
				DepLibUring::Submit();
			}
#endif
		}

		auto it = this->mapProducerRtpObservers.find(producer);

		if (it != this->mapProducerRtpObservers.end())
		{
			auto& rtpObservers = it->second;

			for (auto* rtpObserver : rtpObservers)
			{
				rtpObserver->ReceiveRtpPacket(producer, packet);
			}
		}
	}

	inline void Router::OnTransportNeedWorstRemoteFractionLost(
	  RTC::Transport* /*transport*/,
	  RTC::Producer* producer,
	  uint32_t mappedSsrc,
	  uint8_t& worstRemoteFractionLost)
	{
		MS_TRACE();

		auto& consumers = this->mapProducerConsumers.at(producer);

		for (auto* consumer : consumers)
		{
			consumer->NeedWorstRemoteFractionLost(mappedSsrc, worstRemoteFractionLost);
		}
	}

	inline void Router::OnTransportNewConsumer(
	  RTC::Transport* /*transport*/, RTC::Consumer* consumer, const std::string& producerId)
	{
		MS_TRACE();

		auto mapProducersIt = this->mapProducers.find(producerId);

		if (mapProducersIt == this->mapProducers.end())
		{
			MS_THROW_ERROR("Producer not found [producerId:%s]", producerId.c_str());
		}

		auto* producer              = mapProducersIt->second;
		auto mapProducerConsumersIt = this->mapProducerConsumers.find(producer);

		MS_ASSERT(
		  mapProducerConsumersIt != this->mapProducerConsumers.end(),
		  "Producer not present in mapProducerConsumers");
		MS_ASSERT(
		  this->mapConsumerProducer.find(consumer) == this->mapConsumerProducer.end(),
		  "Consumer already present in mapConsumerProducer");

		// Update the Consumer status based on the Producer status.
		if (producer->IsPaused())
		{
			consumer->ProducerPaused();
		}

		// Insert the Consumer in the maps.
		auto& consumers = mapProducerConsumersIt->second;

		consumers.insert(consumer);
		this->mapConsumerProducer[consumer] = producer;

		// Get all streams in the Producer and provide the Consumer with them.
		for (const auto& kv : producer->GetRtpStreams())
		{
			auto* rtpStream           = kv.first;
			const uint32_t mappedSsrc = kv.second;

			consumer->ProducerRtpStream(rtpStream, mappedSsrc);
		}

		// Provide the Consumer with the scores of all streams in the Producer.
		consumer->ProducerRtpStreamScores(producer->GetRtpStreamScores());
	}

	inline void Router::OnTransportConsumerClosed(RTC::Transport* /*transport*/, RTC::Consumer* consumer)
	{
		MS_TRACE();

		// NOTE:
		// This callback is called when the Consumer has been closed but its Producer
		// remains alive, so the entry in mapProducerConsumers still exists and must
		// be removed.

		auto mapConsumerProducerIt = this->mapConsumerProducer.find(consumer);

		MS_ASSERT(
		  mapConsumerProducerIt != this->mapConsumerProducer.end(),
		  "Consumer not present in mapConsumerProducer");

		// Get the associated Producer.
		auto* producer = mapConsumerProducerIt->second;

		MS_ASSERT(
		  this->mapProducerConsumers.find(producer) != this->mapProducerConsumers.end(),
		  "Producer not present in mapProducerConsumers");

		// Remove the Consumer from the set of Consumers of the Producer.
		auto& consumers = this->mapProducerConsumers.at(producer);

		consumers.erase(consumer);

		// Remove the Consumer from the map.
		this->mapConsumerProducer.erase(mapConsumerProducerIt);
	}

	inline void Router::OnTransportConsumerProducerClosed(
	  RTC::Transport* /*transport*/, RTC::Consumer* consumer)
	{
		MS_TRACE();

		// NOTE:
		// This callback is called when the Consumer has been closed because its
		// Producer was closed, so the entry in mapProducerConsumers has already been
		// removed.

		auto mapConsumerProducerIt = this->mapConsumerProducer.find(consumer);

		MS_ASSERT(
		  mapConsumerProducerIt != this->mapConsumerProducer.end(),
		  "Consumer not present in mapConsumerProducer");

		// Remove the Consumer from the map.
		this->mapConsumerProducer.erase(mapConsumerProducerIt);
	}

	inline void Router::OnTransportConsumerKeyFrameRequested(
	  RTC::Transport* /*transport*/, RTC::Consumer* consumer, uint32_t mappedSsrc)
	{
		MS_TRACE();

		auto* producer = this->mapConsumerProducer.at(consumer);

		producer->RequestKeyFrame(mappedSsrc);
	}

	inline void Router::OnTransportNewDataProducer(
	  RTC::Transport* /*transport*/, RTC::DataProducer* dataProducer)
	{
		MS_TRACE();

		MS_ASSERT(
		  this->mapDataProducerDataConsumers.find(dataProducer) ==
		    this->mapDataProducerDataConsumers.end(),
		  "DataProducer already present in mapDataProducerDataConsumers");

		if (this->mapDataProducers.find(dataProducer->id) != this->mapDataProducers.end())
		{
			MS_THROW_ERROR(
			  "DataProducer already present in mapDataProducers [dataProducerId:%s]",
			  dataProducer->id.c_str());
		}

		// Insert the DataProducer in the maps.
		this->mapDataProducers[dataProducer->id] = dataProducer;
		this->mapDataProducerDataConsumers[dataProducer];
	}

	inline void Router::OnTransportDataProducerClosed(
	  RTC::Transport* /*transport*/, RTC::DataProducer* dataProducer)
	{
		MS_TRACE();

		auto mapDataProducerDataConsumersIt = this->mapDataProducerDataConsumers.find(dataProducer);
		auto mapDataProducersIt             = this->mapDataProducers.find(dataProducer->id);

		MS_ASSERT(
		  mapDataProducerDataConsumersIt != this->mapDataProducerDataConsumers.end(),
		  "DataProducer not present in mapDataProducerDataConsumers");
		MS_ASSERT(
		  mapDataProducersIt != this->mapDataProducers.end(),
		  "DataProducer not present in mapDataProducers");

		// Close all DataConsumers associated to the closed DataProducer.
		auto& dataConsumers = mapDataProducerDataConsumersIt->second;

		// NOTE: While iterating the set of DataConsumers, we call DataProducerClosed()
		// on each one, which will end calling
		// Router::OnTransportDataConsumerDataProducerClosed(), which will remove the
		// DataConsumer from mapDataConsumerDataProducer but won't remove the closed
		// DataConsumer from the set of DataConsumers in mapDataProducerDataConsumers
		// (here will erase the complete entry in that map).
		for (auto* dataConsumer : dataConsumers)
		{
			// Call dataConsumer->DataProducerClosed() so the DataConsumer will notify the Node
			// process, will notify its Transport, and its Transport will delete the DataConsumer.
			dataConsumer->DataProducerClosed();
		}

		// Remove the DataProducer from the maps.
		this->mapDataProducers.erase(mapDataProducersIt);
		this->mapDataProducerDataConsumers.erase(mapDataProducerDataConsumersIt);
	}

	inline void Router::OnTransportDataProducerPaused(
	  RTC::Transport* /*transport*/, RTC::DataProducer* dataProducer)
	{
		MS_TRACE();

		auto& dataConsumers = this->mapDataProducerDataConsumers.at(dataProducer);

		for (auto* dataConsumer : dataConsumers)
		{
			dataConsumer->DataProducerPaused();
		}
	}

	inline void Router::OnTransportDataProducerResumed(
	  RTC::Transport* /*transport*/, RTC::DataProducer* dataProducer)
	{
		MS_TRACE();

		auto& dataConsumers = this->mapDataProducerDataConsumers.at(dataProducer);

		for (auto* dataConsumer : dataConsumers)
		{
			dataConsumer->DataProducerResumed();
		}
	}

	inline void Router::OnTransportDataProducerMessageReceived(
	  RTC::Transport* /*transport*/,
	  RTC::DataProducer* dataProducer,
	  const uint8_t* msg,
	  size_t len,
	  uint32_t ppid,
	  std::vector<uint16_t>& subchannels,
	  std::optional<uint16_t> requiredSubchannel)
	{
		MS_TRACE();

		auto& dataConsumers = this->mapDataProducerDataConsumers.at(dataProducer);

		if (!dataConsumers.empty())
		{
#ifdef MS_LIBURING_SUPPORTED
			if (DepLibUring::IsEnabled())
			{
				// Activate liburing usage.
				// The effective sending could be synchronous, thus we would send those
				// messages within a single system call.
				DepLibUring::SetActive();
			}
#endif

			for (auto* dataConsumer : dataConsumers)
			{
				dataConsumer->SendMessage(msg, len, ppid, subchannels, requiredSubchannel);
			}

#ifdef MS_LIBURING_SUPPORTED
			if (DepLibUring::IsEnabled())
			{
				// Submit all prepared submission entries.
				DepLibUring::Submit();
			}
#endif
		}
	}

	inline void Router::OnTransportNewDataConsumer(
	  RTC::Transport* /*transport*/, RTC::DataConsumer* dataConsumer, std::string& dataProducerId)
	{
		MS_TRACE();

		auto mapDataProducersIt = this->mapDataProducers.find(dataProducerId);

		if (mapDataProducersIt == this->mapDataProducers.end())
		{
			MS_THROW_ERROR("DataProducer not found [dataProducerId:%s]", dataProducerId.c_str());
		}

		auto* dataProducer                  = mapDataProducersIt->second;
		auto mapDataProducerDataConsumersIt = this->mapDataProducerDataConsumers.find(dataProducer);

		MS_ASSERT(
		  mapDataProducerDataConsumersIt != this->mapDataProducerDataConsumers.end(),
		  "DataProducer not present in mapDataProducerDataConsumers");
		MS_ASSERT(
		  this->mapDataConsumerDataProducer.find(dataConsumer) == this->mapDataConsumerDataProducer.end(),
		  "DataConsumer already present in mapDataConsumerDataProducer");

		// Update the DataConsumer status based on the DataProducer status.
		if (dataProducer->IsPaused())
		{
			dataConsumer->DataProducerPaused();
		}

		// Insert the DataConsumer in the maps.
		auto& dataConsumers = mapDataProducerDataConsumersIt->second;

		dataConsumers.insert(dataConsumer);
		this->mapDataConsumerDataProducer[dataConsumer] = dataProducer;
	}

	inline void Router::OnTransportDataConsumerClosed(
	  RTC::Transport* /*transport*/, RTC::DataConsumer* dataConsumer)
	{
		MS_TRACE();

		// NOTE:
		// This callback is called when the DataConsumer has been closed but its DataProducer
		// remains alive, so the entry in mapDataProducerDataConsumers still exists and must
		// be removed.

		auto mapDataConsumerDataProducerIt = this->mapDataConsumerDataProducer.find(dataConsumer);

		MS_ASSERT(
		  mapDataConsumerDataProducerIt != this->mapDataConsumerDataProducer.end(),
		  "DataConsumer not present in mapDataConsumerDataProducer");

		// Get the associated DataProducer.
		auto* dataProducer = mapDataConsumerDataProducerIt->second;

		MS_ASSERT(
		  this->mapDataProducerDataConsumers.find(dataProducer) !=
		    this->mapDataProducerDataConsumers.end(),
		  "DataProducer not present in mapDataProducerDataConsumers");

		// Remove the DataConsumer from the set of DataConsumers of the DataProducer.
		auto& dataConsumers = this->mapDataProducerDataConsumers.at(dataProducer);

		dataConsumers.erase(dataConsumer);

		// Remove the DataConsumer from the map.
		this->mapDataConsumerDataProducer.erase(mapDataConsumerDataProducerIt);
	}

	inline void Router::OnTransportDataConsumerDataProducerClosed(
	  RTC::Transport* /*transport*/, RTC::DataConsumer* dataConsumer)
	{
		MS_TRACE();

		// NOTE:
		// This callback is called when the DataConsumer has been closed because its
		// DataProducer was closed, so the entry in mapDataProducerDataConsumers has already
		// been removed.

		auto mapDataConsumerDataProducerIt = this->mapDataConsumerDataProducer.find(dataConsumer);

		MS_ASSERT(
		  mapDataConsumerDataProducerIt != this->mapDataConsumerDataProducer.end(),
		  "DataConsumer not present in mapDataConsumerDataProducer");

		// Remove the DataConsumer from the map.
		this->mapDataConsumerDataProducer.erase(mapDataConsumerDataProducerIt);
	}

	inline void Router::OnTransportListenServerClosed(RTC::Transport* transport)
	{
		MS_TRACE();

		MS_ASSERT(
		  this->mapTransports.find(transport->id) != this->mapTransports.end(),
		  "Transport not present in mapTransports");

		// Tell the Transport to close all its Producers and Consumers so it will
		// notify us about their closures.
		transport->CloseProducersAndConsumers();

		// Remove it from the map.
		this->mapTransports.erase(transport->id);

		// Delete it.
		delete transport;
	}

	void Router::OnRtpObserverAddProducer(RTC::RtpObserver* rtpObserver, RTC::Producer* producer)
	{
		// Add to the map.
		this->mapProducerRtpObservers[producer].insert(rtpObserver);
	}

	void Router::OnRtpObserverRemoveProducer(RTC::RtpObserver* rtpObserver, RTC::Producer* producer)
	{
		// Remove from the map.
		this->mapProducerRtpObservers[producer].erase(rtpObserver);
	}

	RTC::Producer* Router::RtpObserverGetProducer(
	  RTC::RtpObserver* /* rtpObserver */, const std::string& id)
	{
		auto it = this->mapProducers.find(id);

		if (it == this->mapProducers.end())
		{
			MS_THROW_ERROR("Producer not found");
		}

		RTC::Producer* producer = it->second;

		return producer;
	}
} // namespace RTC
