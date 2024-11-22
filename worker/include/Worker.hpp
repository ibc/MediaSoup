#ifndef MS_WORKER_HPP
#define MS_WORKER_HPP

#include "common.hpp"
#include "Channel/ChannelRequest.hpp"
#include "Channel/ChannelSocket.hpp"
#include "FBS/worker.h"
#include "RTC/Router.hpp"
#include "RTC/Shared.hpp"
#include "RTC/WebRtcServer.hpp"
#include "handles/SignalHandle.hpp"
#include <flatbuffers/flatbuffer_builder.h>
#include <absl/container/flat_hash_map.h>
#include <string>

class Worker : public Channel::ChannelSocket::Listener,
               public SignalHandle::Listener,
               public RTC::Router::Listener
{
public:
	explicit Worker(Channel::ChannelSocket* channel);
	~Worker();

private:
	void Close();
	flatbuffers::Offset<FBS::Worker::DumpResponse> FillBuffer(flatbuffers::FlatBufferBuilder& builder) const;
	flatbuffers::Offset<FBS::Worker::ResourceUsageResponse> FillBufferResourceUsage(
	  flatbuffers::FlatBufferBuilder& builder) const;
	void SetNewRouterId(std::string& routerId) const;
	RTC::WebRtcServer* GetWebRtcServer(const std::string& webRtcServerId) const;
	RTC::Router* GetRouter(const std::string& routerId) const;
	void CheckNoWebRtcServer(const std::string& webRtcServerId) const;
	void CheckNoRouter(const std::string& routerId) const;

	/* Methods inherited from Channel::ChannelSocket::RequestHandler. */
public:
	void HandleRequest(Channel::ChannelRequest* request) override;
	void HandleNotification(Channel::ChannelNotification* notification) override;

	/* Methods inherited from Channel::ChannelSocket::Listener. */
public:
	void OnChannelClosed(Channel::ChannelSocket* channel) override;

	/* Methods inherited from SignalHandle::Listener. */
public:
	void OnSignal(SignalHandle* signalsHandler, int signum) override;

	/* Pure virtual methods inherited from RTC::Router::Listener. */
public:
	RTC::WebRtcServer* OnRouterNeedWebRtcServer(RTC::Router* router, std::string& webRtcServerId) override;

private:
	// Passed by argument.
	Channel::ChannelSocket* channel{ nullptr };
	// Allocated by this.
	SignalHandle* signalHandle{ nullptr };
	RTC::Shared* shared{ nullptr };
	absl::flat_hash_map<std::string, RTC::WebRtcServer*> mapWebRtcServers;
	absl::flat_hash_map<std::string, RTC::Router*> mapRouters;
	// Others.
	bool closed{ false };
};

#endif
