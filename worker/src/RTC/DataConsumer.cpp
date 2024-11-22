#define MS_CLASS "RTC::DataConsumer"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/DataConsumer.hpp"
#include "DepLibUV.hpp"
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include "RTC/SctpAssociation.hpp"

namespace RTC
{
	/* Instance methods. */

	DataConsumer::DataConsumer(
	  RTC::Shared* shared,
	  const std::string& id,
	  const std::string& dataProducerId,
	  RTC::SctpAssociation* sctpAssociation,
	  RTC::DataConsumer::Listener* listener,
	  const FBS::Transport::ConsumeDataRequest* data,
	  size_t maxMessageSize)
	  : id(id), dataProducerId(dataProducerId), shared(shared), sctpAssociation(sctpAssociation),
	    listener(listener), maxMessageSize(maxMessageSize)
	{
		MS_TRACE();

		switch (data->type())
		{
			case FBS::DataProducer::Type::SCTP:
			{
				this->type = DataConsumer::Type::SCTP;

				break;
			}
			case FBS::DataProducer::Type::DIRECT:
			{
				this->type = DataConsumer::Type::DIRECT;

				break;
			}
		}

		if (this->type == DataConsumer::Type::SCTP)
		{
			if (!flatbuffers::IsFieldPresent(
			      data, FBS::Transport::ConsumeDataRequest::VT_SCTPSTREAMPARAMETERS))
			{
				MS_THROW_TYPE_ERROR("missing sctpStreamParameters");
			}

			// This may throw.
			this->sctpStreamParameters = RTC::SctpStreamParameters(data->sctpStreamParameters());
		}

		if (flatbuffers::IsFieldPresent(data, FBS::Transport::ConsumeDataRequest::VT_LABEL))
		{
			this->label = data->label()->str();
		}

		if (flatbuffers::IsFieldPresent(data, FBS::Transport::ConsumeDataRequest::VT_PROTOCOL))
		{
			this->protocol = data->protocol()->str();
		}

		// paused is set to false by default.
		this->paused = data->paused();

		if (flatbuffers::IsFieldPresent(data, FBS::Transport::ConsumeDataRequest::VT_SUBCHANNELS))
		{
			for (const auto subchannel : *data->subchannels())
			{
				this->subchannels.insert(subchannel);
			}
		}

		// NOTE: This may throw.
		this->shared->channelMessageRegistrator->RegisterHandler(
		  this->id,
		  /*channelRequestHandler*/ this,
		  /*channelNotificationHandler*/ nullptr);
	}

	DataConsumer::~DataConsumer()
	{
		MS_TRACE();

		this->shared->channelMessageRegistrator->UnregisterHandler(this->id);
	}

	flatbuffers::Offset<FBS::DataConsumer::DumpResponse> DataConsumer::FillBuffer(
	  flatbuffers::FlatBufferBuilder& builder) const
	{
		MS_TRACE();

		flatbuffers::Offset<FBS::SctpParameters::SctpStreamParameters> sctpStreamParameters;

		// Add sctpStreamParameters.
		if (this->type == DataConsumer::Type::SCTP)
		{
			sctpStreamParameters = this->sctpStreamParameters.FillBuffer(builder);
		}

		std::vector<uint16_t> subchannels;

		subchannels.reserve(this->subchannels.size());

		for (auto subchannel : this->subchannels)
		{
			subchannels.emplace_back(subchannel);
		}

		return FBS::DataConsumer::CreateDumpResponseDirect(
		  builder,
		  this->id.c_str(),
		  this->dataProducerId.c_str(),
		  this->type == DataConsumer::Type::SCTP ? FBS::DataProducer::Type::SCTP
		                                         : FBS::DataProducer::Type::DIRECT,
		  sctpStreamParameters,
		  this->label.c_str(),
		  this->protocol.c_str(),
		  this->bufferedAmountLowThreshold,
		  this->paused,
		  this->dataProducerPaused,
		  std::addressof(subchannels));
	}

	flatbuffers::Offset<FBS::DataConsumer::GetStatsResponse> DataConsumer::FillBufferStats(
	  flatbuffers::FlatBufferBuilder& builder) const
	{
		MS_TRACE();

		return FBS::DataConsumer::CreateGetStatsResponseDirect(
		  builder,
		  // timestamp.
		  DepLibUV::GetTimeMs(),
		  // label.
		  this->label.c_str(),
		  // protocol.
		  this->protocol.c_str(),
		  // messagesSent.
		  this->messagesSent,
		  // bytesSent.
		  this->bytesSent,
		  // bufferedAmount.
		  this->bufferedAmount);
	}

	void DataConsumer::HandleRequest(Channel::ChannelRequest* request)
	{
		MS_TRACE();

		switch (request->method)
		{
			case Channel::ChannelRequest::Method::DATACONSUMER_DUMP:
			{
				auto dumpOffset = FillBuffer(request->GetBufferBuilder());

				request->Accept(FBS::Response::Body::DataConsumer_DumpResponse, dumpOffset);

				break;
			}

			case Channel::ChannelRequest::Method::DATACONSUMER_GET_STATS:
			{
				auto responseOffset = FillBufferStats(request->GetBufferBuilder());

				request->Accept(FBS::Response::Body::DataConsumer_GetStatsResponse, responseOffset);

				break;
			}

			case Channel::ChannelRequest::Method::DATACONSUMER_PAUSE:
			{
				if (this->paused)
				{
					request->Accept();

					break;
				}

				this->paused = true;

				MS_DEBUG_DEV("DataConsumer paused [dataConsumerId:%s]", this->id.c_str());

				request->Accept();

				break;
			}

			case Channel::ChannelRequest::Method::DATACONSUMER_RESUME:
			{
				if (!this->paused)
				{
					request->Accept();

					break;
				}

				this->paused = false;

				MS_DEBUG_DEV("DataConsumer resumed [dataConsumerId:%s]", this->id.c_str());

				request->Accept();

				break;
			}

			case Channel::ChannelRequest::Method::DATACONSUMER_GET_BUFFERED_AMOUNT:
			{
				if (this->GetType() != RTC::DataConsumer::Type::SCTP)
				{
					MS_THROW_TYPE_ERROR("invalid DataConsumer type");
				}

				if (!this->sctpAssociation)
				{
					MS_THROW_ERROR("no SCTP association present");
				}

				// Create status response.
				auto responseOffset = FBS::DataConsumer::CreateGetBufferedAmountResponse(
				  request->GetBufferBuilder(), this->sctpAssociation->GetSctpBufferedAmount());

				request->Accept(FBS::Response::Body::DataConsumer_GetBufferedAmountResponse, responseOffset);

				break;
			}

			case Channel::ChannelRequest::Method::DATACONSUMER_SET_BUFFERED_AMOUNT_LOW_THRESHOLD:
			{
				if (this->GetType() != DataConsumer::Type::SCTP)
				{
					MS_THROW_TYPE_ERROR("invalid DataConsumer type");
				}

				const auto* body =
				  request->data->body_as<FBS::DataConsumer::SetBufferedAmountLowThresholdRequest>();

				this->bufferedAmountLowThreshold = body->threshold();

				request->Accept();

				// There is less or same buffered data than the given threshold.
				// Trigger 'bufferedamountlow' now.
				if (this->bufferedAmount <= this->bufferedAmountLowThreshold)
				{
					// Notify the Node DataConsumer.
					auto bufferedAmountLowOffset = FBS::DataConsumer::CreateBufferedAmountLowNotification(
					  this->shared->channelNotifier->GetBufferBuilder(), this->bufferedAmount);

					this->shared->channelNotifier->Emit(
					  this->id,
					  FBS::Notification::Event::DATACONSUMER_BUFFERED_AMOUNT_LOW,
					  FBS::Notification::Body::DataConsumer_BufferedAmountLowNotification,
					  bufferedAmountLowOffset);
				}
				// Force the trigger of 'bufferedamountlow' once there is less or same
				// buffered data than the given threshold.
				else
				{
					this->forceTriggerBufferedAmountLow = true;
				}

				break;
			}

			case Channel::ChannelRequest::Method::DATACONSUMER_SEND:
			{
				if (this->GetType() != RTC::DataConsumer::Type::SCTP)
				{
					MS_THROW_TYPE_ERROR("invalid DataConsumer type");
				}

				if (!this->sctpAssociation)
				{
					MS_THROW_ERROR("no SCTP association present");
				}

				const auto* body    = request->data->body_as<FBS::DataConsumer::SendRequest>();
				const uint8_t* data = body->data()->Data();
				const size_t len    = body->data()->size();

				if (len > this->maxMessageSize)
				{
					MS_THROW_TYPE_ERROR(
					  "given message exceeds maxMessageSize value [maxMessageSize:%zu, len:%zu]",
					  this->maxMessageSize,
					  len);
				}

				const auto* cb = new onQueuedCallback(
				  [&request](bool queued, bool sctpSendBufferFull)
				  {
					  if (queued)
					  {
						  request->Accept();
					  }
					  else
					  {
						  request->Error(sctpSendBufferFull ? "sctpsendbufferfull" : "message send failed");
					  }
				  });

				static std::vector<uint16_t> emptySubchannels;

				SendMessage(data, len, body->ppid(), emptySubchannels, std::nullopt, cb);

				break;
			}

			case Channel::ChannelRequest::Method::DATACONSUMER_SET_SUBCHANNELS:
			{
				const auto* body = request->data->body_as<FBS::DataConsumer::SetSubchannelsRequest>();

				this->subchannels.clear();

				for (const auto subchannel : *body->subchannels())
				{
					this->subchannels.insert(subchannel);
				}

				std::vector<uint16_t> subchannels;

				subchannels.reserve(this->subchannels.size());

				for (auto subchannel : this->subchannels)
				{
					subchannels.emplace_back(subchannel);
				}

				// Create response.
				auto responseOffset = FBS::DataConsumer::CreateSetSubchannelsResponseDirect(
				  request->GetBufferBuilder(), std::addressof(subchannels));

				request->Accept(FBS::Response::Body::DataConsumer_SetSubchannelsResponse, responseOffset);

				break;
			}

			case Channel::ChannelRequest::Method::DATACONSUMER_ADD_SUBCHANNEL:
			{
				const auto* body = request->data->body_as<FBS::DataConsumer::AddSubchannelRequest>();

				this->subchannels.insert(body->subchannel());

				std::vector<uint16_t> subchannels;

				subchannels.reserve(this->subchannels.size());

				for (auto subchannel : this->subchannels)
				{
					subchannels.emplace_back(subchannel);
				}

				// Create response.
				auto responseOffset = FBS::DataConsumer::CreateAddSubchannelResponseDirect(
				  request->GetBufferBuilder(), std::addressof(subchannels));

				request->Accept(FBS::Response::Body::DataConsumer_AddSubchannelResponse, responseOffset);

				break;
			}

			case Channel::ChannelRequest::Method::DATACONSUMER_REMOVE_SUBCHANNEL:
			{
				const auto* body = request->data->body_as<FBS::DataConsumer::RemoveSubchannelRequest>();

				this->subchannels.erase(body->subchannel());

				std::vector<uint16_t> subchannels;

				subchannels.reserve(this->subchannels.size());

				for (auto subchannel : this->subchannels)
				{
					subchannels.emplace_back(subchannel);
				}

				// Create response.
				auto responseOffset = FBS::DataConsumer::CreateRemoveSubchannelResponseDirect(
				  request->GetBufferBuilder(), std::addressof(subchannels));

				request->Accept(FBS::Response::Body::DataConsumer_RemoveSubchannelResponse, responseOffset);

				break;
			}

			default:
			{
				MS_THROW_ERROR("unknown method '%s'", request->methodCStr);
			}
		}
	}

	void DataConsumer::TransportConnected()
	{
		MS_TRACE();

		this->transportConnected = true;

		MS_DEBUG_DEV("Transport connected [dataConsumerId:%s]", this->id.c_str());
	}

	void DataConsumer::TransportDisconnected()
	{
		MS_TRACE();

		this->transportConnected = false;

		MS_DEBUG_DEV("Transport disconnected [dataConsumerId:%s]", this->id.c_str());
	}

	void DataConsumer::DataProducerPaused()
	{
		MS_TRACE();

		if (this->dataProducerPaused)
		{
			return;
		}

		this->dataProducerPaused = true;

		MS_DEBUG_DEV("DataProducer paused [dataConsumerId:%s]", this->id.c_str());

		this->shared->channelNotifier->Emit(
		  this->id, FBS::Notification::Event::DATACONSUMER_DATAPRODUCER_PAUSE);
	}

	void DataConsumer::DataProducerResumed()
	{
		MS_TRACE();

		if (!this->dataProducerPaused)
		{
			return;
		}

		this->dataProducerPaused = false;

		MS_DEBUG_DEV("DataProducer resumed [dataConsumerId:%s]", this->id.c_str());

		this->shared->channelNotifier->Emit(
		  this->id, FBS::Notification::Event::DATACONSUMER_DATAPRODUCER_RESUME);
	}

	void DataConsumer::SctpAssociationConnected()
	{
		MS_TRACE();

		this->sctpAssociationConnected = true;

		MS_DEBUG_DEV("SctpAssociation connected [dataConsumerId:%s]", this->id.c_str());
	}

	void DataConsumer::SctpAssociationClosed()
	{
		MS_TRACE();

		this->sctpAssociationConnected = false;

		MS_DEBUG_DEV("SctpAssociation closed [dataConsumerId:%s]", this->id.c_str());
	}

	void DataConsumer::SctpAssociationBufferedAmount(uint32_t bufferedAmount)
	{
		MS_TRACE();

		auto previousBufferedAmount = this->bufferedAmount;

		this->bufferedAmount = bufferedAmount;

		// clang-format off
		if (
				(this->forceTriggerBufferedAmountLow || previousBufferedAmount > this->bufferedAmountLowThreshold) &&
				this->bufferedAmount <= this->bufferedAmountLowThreshold
		)
		// clang-format on
		{
			this->forceTriggerBufferedAmountLow = false;

			// Notify the Node DataConsumer.
			auto bufferedAmountLowOffset = FBS::DataConsumer::CreateBufferedAmountLowNotification(
			  this->shared->channelNotifier->GetBufferBuilder(), this->bufferedAmount);

			this->shared->channelNotifier->Emit(
			  this->id,
			  FBS::Notification::Event::DATACONSUMER_BUFFERED_AMOUNT_LOW,
			  FBS::Notification::Body::DataConsumer_BufferedAmountLowNotification,
			  bufferedAmountLowOffset);
		}
	}

	void DataConsumer::SctpAssociationSendBufferFull()
	{
		MS_TRACE();

		this->shared->channelNotifier->Emit(
		  this->id, FBS::Notification::Event::DATACONSUMER_SCTP_SENDBUFFER_FULL);
	}

	// The caller (Router) is supposed to proceed with the deletion of this DataConsumer
	// right after calling this method. Otherwise ugly things may happen.
	void DataConsumer::DataProducerClosed()
	{
		MS_TRACE();

		this->dataProducerClosed = true;

		MS_DEBUG_DEV("DataProducer closed [dataConsumerId:%s]", this->id.c_str());

		this->shared->channelNotifier->Emit(
		  this->id, FBS::Notification::Event::DATACONSUMER_DATAPRODUCER_CLOSE);

		this->listener->OnDataConsumerDataProducerClosed(this);
	}

	void DataConsumer::SendMessage(
	  const uint8_t* msg,
	  size_t len,
	  uint32_t ppid,
	  std::vector<uint16_t>& subchannels,
	  std::optional<uint16_t> requiredSubchannel,
	  onQueuedCallback* cb)
	{
		MS_TRACE();

		if (!IsActive())
		{
			if (cb)
			{
				(*cb)(false, false);
				delete cb;
			}

			return;
		}

		// If a required subchannel is given, verify that this data consumer is
		// subscribed to it.
		if (
		  requiredSubchannel.has_value() &&
		  this->subchannels.find(requiredSubchannel.value()) == this->subchannels.end())
		{
			if (cb)
			{
				(*cb)(false, false);
				delete cb;
			}

			return;
		}

		// If subchannels are given, verify that this data consumer is subscribed
		// to at least one of them.
		if (!subchannels.empty())
		{
			bool subchannelMatched{ false };

			for (const auto subchannel : subchannels)
			{
				if (this->subchannels.find(subchannel) != this->subchannels.end())
				{
					subchannelMatched = true;

					break;
				}
			}

			if (!subchannelMatched)
			{
				if (cb)
				{
					(*cb)(false, false);
					delete cb;
				}

				return;
			}
		}

		if (len > this->maxMessageSize)
		{
			MS_WARN_TAG(
			  message,
			  "given message exceeds maxMessageSize value [maxMessageSize:%zu, len:%zu]",
			  len,
			  this->maxMessageSize);

			if (cb)
			{
				(*cb)(false, false);
				delete cb;
			}

			return;
		}

		this->messagesSent++;
		this->bytesSent += len;

		this->listener->OnDataConsumerSendMessage(this, msg, len, ppid, cb);
	}
} // namespace RTC
