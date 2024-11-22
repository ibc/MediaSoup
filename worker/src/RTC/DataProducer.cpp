#define MS_CLASS "RTC::DataProducer"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/DataProducer.hpp"
#include "DepLibUV.hpp"
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include <vector>

namespace RTC
{
	/* Instance methods. */

	DataProducer::DataProducer(
	  RTC::Shared* shared,
	  const std::string& id,
	  size_t maxMessageSize,
	  RTC::DataProducer::Listener* listener,
	  const FBS::Transport::ProduceDataRequest* data)
	  : id(id), shared(shared), maxMessageSize(maxMessageSize), listener(listener)
	{
		MS_TRACE();

		switch (data->type())
		{
			case FBS::DataProducer::Type::SCTP:
			{
				this->type = DataProducer::Type::SCTP;

				break;
			}

			case FBS::DataProducer::Type::DIRECT:
			{
				this->type = DataProducer::Type::DIRECT;

				break;
			}
		}

		if (this->type == DataProducer::Type::SCTP)
		{
			if (!flatbuffers::IsFieldPresent(
			      data, FBS::Transport::ProduceDataRequest::VT_SCTPSTREAMPARAMETERS))
			{
				MS_THROW_TYPE_ERROR("missing sctpStreamParameters");
			}

			// This may throw.
			this->sctpStreamParameters = RTC::SctpStreamParameters(data->sctpStreamParameters());
		}

		if (flatbuffers::IsFieldPresent(data, FBS::Transport::ProduceDataRequest::VT_LABEL))
		{
			this->label = data->label()->str();
		}

		if (flatbuffers::IsFieldPresent(data, FBS::Transport::ProduceDataRequest::VT_PROTOCOL))
		{
			this->protocol = data->protocol()->str();
		}

		this->paused = data->paused();

		// NOTE: This may throw.
		this->shared->channelMessageRegistrator->RegisterHandler(
		  this->id,
		  /*channelRequestHandler*/ this,
		  /*channelNotificationHandler*/ this);
	}

	DataProducer::~DataProducer()
	{
		MS_TRACE();

		this->shared->channelMessageRegistrator->UnregisterHandler(this->id);
	}

	flatbuffers::Offset<FBS::DataProducer::DumpResponse> DataProducer::FillBuffer(
	  flatbuffers::FlatBufferBuilder& builder) const
	{
		MS_TRACE();

		flatbuffers::Offset<FBS::SctpParameters::SctpStreamParameters> sctpStreamParametersOffset;

		// Add sctpStreamParameters.
		if (this->type == DataProducer::Type::SCTP)
		{
			sctpStreamParametersOffset = this->sctpStreamParameters.FillBuffer(builder);
		}

		return FBS::DataProducer::CreateDumpResponseDirect(
		  builder,
		  this->id.c_str(),
		  this->type == DataProducer::Type::SCTP ? FBS::DataProducer::Type::SCTP
		                                         : FBS::DataProducer::Type::DIRECT,
		  sctpStreamParametersOffset,
		  this->label.c_str(),
		  this->protocol.c_str(),
		  this->paused);
	}

	flatbuffers::Offset<FBS::DataProducer::GetStatsResponse> DataProducer::FillBufferStats(
	  flatbuffers::FlatBufferBuilder& builder) const
	{
		MS_TRACE();

		return FBS::DataProducer::CreateGetStatsResponseDirect(
		  builder,
		  // timestamp.
		  DepLibUV::GetTimeMs(),
		  // label.
		  this->label.c_str(),
		  // protocol.
		  this->protocol.c_str(),
		  // messagesReceived.
		  this->messagesReceived,
		  // bytesReceived.
		  this->bytesReceived);
	}

	void DataProducer::HandleRequest(Channel::ChannelRequest* request)
	{
		MS_TRACE();

		switch (request->method)
		{
			case Channel::ChannelRequest::Method::DATAPRODUCER_DUMP:
			{
				auto dumpOffset = FillBuffer(request->GetBufferBuilder());

				request->Accept(FBS::Response::Body::DataProducer_DumpResponse, dumpOffset);

				break;
			}

			case Channel::ChannelRequest::Method::DATAPRODUCER_GET_STATS:
			{
				auto responseOffset = FillBufferStats(request->GetBufferBuilder());

				request->Accept(FBS::Response::Body::DataProducer_GetStatsResponse, responseOffset);

				break;
			}

			case Channel::ChannelRequest::Method::DATAPRODUCER_PAUSE:
			{
				if (this->paused)
				{
					request->Accept();

					break;
				}

				this->paused = true;

				MS_DEBUG_DEV("DataProducer paused [dataProducerId:%s]", this->id.c_str());

				this->listener->OnDataProducerPaused(this);

				request->Accept();

				break;
			}

			case Channel::ChannelRequest::Method::DATAPRODUCER_RESUME:
			{
				if (!this->paused)
				{
					request->Accept();

					break;
				}

				this->paused = false;

				MS_DEBUG_DEV("DataProducer resumed [dataProducerId:%s]", this->id.c_str());

				this->listener->OnDataProducerResumed(this);

				request->Accept();

				break;
			}

			default:
			{
				MS_THROW_ERROR("unknown method '%s'", request->methodCStr);
			}
		}
	}

	void DataProducer::HandleNotification(Channel::ChannelNotification* notification)
	{
		MS_TRACE();

		switch (notification->event)
		{
			case Channel::ChannelNotification::Event::DATAPRODUCER_SEND:
			{
				const auto* body    = notification->data->body_as<FBS::DataProducer::SendNotification>();
				const uint8_t* data = body->data()->Data();
				const size_t len    = body->data()->size();

				if (len > this->maxMessageSize)
				{
					MS_THROW_TYPE_ERROR(
					  "given message exceeds maxMessageSize value [maxMessageSize:%zu, len:%zu]",
					  this->maxMessageSize,
					  len);
				}

				std::vector<uint16_t> subchannels;

				if (flatbuffers::IsFieldPresent(body, FBS::DataProducer::SendNotification::VT_SUBCHANNELS))
				{
					subchannels.reserve(body->subchannels()->size());

					for (const auto subchannel : *body->subchannels())
					{
						subchannels.emplace_back(subchannel);
					}
				}

				std::optional<uint16_t> requiredSubchannel{ std::nullopt };

				if (body->requiredSubchannel().has_value())
				{
					requiredSubchannel = body->requiredSubchannel().value();
				}

				ReceiveMessage(data, len, body->ppid(), subchannels, requiredSubchannel);

				// Increase receive transmission.
				this->listener->OnDataProducerReceiveData(this, len);

				break;
			}

			default:
			{
				MS_ERROR("unknown event '%s'", notification->eventCStr);
			}
		}
	}

	void DataProducer::ReceiveMessage(
	  const uint8_t* msg,
	  size_t len,
	  uint32_t ppid,
	  std::vector<uint16_t>& subchannels,
	  std::optional<uint16_t> requiredSubchannel)
	{
		MS_TRACE();

		this->messagesReceived++;
		this->bytesReceived += len;

		// If paused stop here.
		if (this->paused)
		{
			return;
		}

		this->listener->OnDataProducerMessageReceived(
		  this, msg, len, ppid, subchannels, requiredSubchannel);
	}
} // namespace RTC
