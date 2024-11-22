#ifndef MS_RTC_DATA_PRODUCER_HPP
#define MS_RTC_DATA_PRODUCER_HPP

#include "common.hpp"
#include "Channel/ChannelRequest.hpp"
#include "Channel/ChannelSocket.hpp"
#include "RTC/RTCP/Packet.hpp"
#include "RTC/SctpDictionaries.hpp"
#include "RTC/Shared.hpp"
#include <string>
#include <vector>

namespace RTC
{
	class DataProducer : public Channel::ChannelSocket::RequestHandler,
	                     public Channel::ChannelSocket::NotificationHandler
	{
	public:
		class Listener
		{
		public:
			virtual ~Listener() = default;

		public:
			virtual void OnDataProducerReceiveData(RTC::DataProducer* producer, size_t len) = 0;
			virtual void OnDataProducerMessageReceived(
			  RTC::DataProducer* dataProducer,
			  const uint8_t* msg,
			  size_t len,
			  uint32_t ppid,
			  std::vector<uint16_t>& subchannels,
			  std::optional<uint16_t> requiredSubchannel)                       = 0;
			virtual void OnDataProducerPaused(RTC::DataProducer* dataProducer)  = 0;
			virtual void OnDataProducerResumed(RTC::DataProducer* dataProducer) = 0;
		};

	public:
		enum class Type : uint8_t
		{
			SCTP = 0,
			DIRECT
		};

	public:
		DataProducer(
		  RTC::Shared* shared,
		  const std::string& id,
		  size_t maxMessageSize,
		  RTC::DataProducer::Listener* listener,
		  const FBS::Transport::ProduceDataRequest* data);
		~DataProducer() override;

	public:
		flatbuffers::Offset<FBS::DataProducer::DumpResponse> FillBuffer(
		  flatbuffers::FlatBufferBuilder& builder) const;
		flatbuffers::Offset<FBS::DataProducer::GetStatsResponse> FillBufferStats(
		  flatbuffers::FlatBufferBuilder& builder) const;
		Type GetType() const
		{
			return this->type;
		}
		const RTC::SctpStreamParameters& GetSctpStreamParameters() const
		{
			return this->sctpStreamParameters;
		}
		bool IsPaused() const
		{
			return this->paused;
		}
		void ReceiveMessage(
		  const uint8_t* msg,
		  size_t len,
		  uint32_t ppid,
		  std::vector<uint16_t>& subchannels,
		  std::optional<uint16_t> requiredSubchannel);

		/* Methods inherited from Channel::ChannelSocket::RequestHandler. */
	public:
		void HandleRequest(Channel::ChannelRequest* request) override;

		/* Methods inherited from Channel::ChannelSocket::NotificationHandler. */
	public:
		void HandleNotification(Channel::ChannelNotification* notification) override;

	public:
		// Passed by argument.
		const std::string id;

	private:
		// Passed by argument.
		RTC::Shared* shared{ nullptr };
		size_t maxMessageSize{ 0u };
		RTC::DataProducer::Listener* listener{ nullptr };
		// Others.
		Type type;
		RTC::SctpStreamParameters sctpStreamParameters;
		std::string label;
		std::string protocol;
		bool paused{ false };
		size_t messagesReceived{ 0u };
		size_t bytesReceived{ 0u };
	};
} // namespace RTC

#endif
