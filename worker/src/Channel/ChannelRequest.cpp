#define MS_CLASS "Channel::ChannelRequest"
// #define MS_LOG_DEV_LEVEL 3

#include "Channel/ChannelRequest.hpp"
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"

namespace Channel
{
	/* Static variables. */

	thread_local flatbuffers::FlatBufferBuilder ChannelRequest::bufferBuilder{};

	/* Class variables. */

	// clang-format off
	absl::flat_hash_map<FBS::Request::Method, const char*> ChannelRequest::method2String =
	{
		{ FBS::Request::Method::WORKER_CLOSE,                                   "worker.close"                               },
		{ FBS::Request::Method::WORKER_DUMP,                                    "worker.dump"                                },
		{ FBS::Request::Method::WORKER_GET_RESOURCE_USAGE,                      "worker.getResourceUsage"                    },
		{ FBS::Request::Method::WORKER_UPDATE_SETTINGS,                         "worker.updateSettings"                      },
		{ FBS::Request::Method::WORKER_CREATE_WEBRTCSERVER,                     "worker.createWebRtcServer"                  },
		{ FBS::Request::Method::WORKER_CREATE_ROUTER,                           "worker.createRouter"                        },
		{ FBS::Request::Method::WORKER_WEBRTCSERVER_CLOSE,                      "worker.closeWebRtcServer"                   },
		{ FBS::Request::Method::WORKER_CLOSE_ROUTER,                            "worker.closeRouter"                         },
		{ FBS::Request::Method::WEBRTCSERVER_DUMP,                              "webRtcServer.dump"                          },
		{ FBS::Request::Method::ROUTER_DUMP,                                    "router.dump"                                },
		{ FBS::Request::Method::ROUTER_CREATE_WEBRTCTRANSPORT,                  "router.createWebRtcTransport"               },
		{ FBS::Request::Method::ROUTER_CREATE_WEBRTCTRANSPORT_WITH_SERVER,      "router.createWebRtcTransportWithServer"     },
		{ FBS::Request::Method::ROUTER_CREATE_PLAINTRANSPORT,                   "router.createPlainTransport"                },
		{ FBS::Request::Method::ROUTER_CREATE_PIPETRANSPORT,                    "router.createPipeTransport"                 },
		{ FBS::Request::Method::ROUTER_CREATE_DIRECTTRANSPORT,                  "router.createDirectTransport"               },
		{ FBS::Request::Method::ROUTER_CLOSE_TRANSPORT,                         "router.closeTransport"                      },
		{ FBS::Request::Method::ROUTER_CREATE_ACTIVESPEAKEROBSERVER,            "router.createActiveSpeakerObserver"         },
		{ FBS::Request::Method::ROUTER_CREATE_AUDIOLEVELOBSERVER,               "router.createAudioLevelObserver"            },
		{ FBS::Request::Method::ROUTER_CLOSE_RTPOBSERVER,                       "router.closeRtpObserver"                    },
		{ FBS::Request::Method::TRANSPORT_DUMP,                                 "transport.dump"                             },
		{ FBS::Request::Method::TRANSPORT_GET_STATS,                            "transport.getStats"                         },
		{ FBS::Request::Method::TRANSPORT_CONNECT,                              "transport.connect"                          },
		{ FBS::Request::Method::TRANSPORT_SET_MAX_INCOMING_BITRATE,             "transport.setMaxIncomingBitrate"            },
		{ FBS::Request::Method::TRANSPORT_SET_MAX_OUTGOING_BITRATE,             "transport.setMaxOutgoingBitrate"            },
		{ FBS::Request::Method::TRANSPORT_SET_MIN_OUTGOING_BITRATE,             "transport.setMinOutgoingBitrate"            },
		{ FBS::Request::Method::TRANSPORT_RESTART_ICE,                          "transport.restartIce"                       },
		{ FBS::Request::Method::TRANSPORT_PRODUCE,                              "transport.produce"                          },
		{ FBS::Request::Method::TRANSPORT_PRODUCE_DATA,                         "transport.produceData"                      },
		{ FBS::Request::Method::TRANSPORT_CONSUME,                              "transport.consume"                          },
		{ FBS::Request::Method::TRANSPORT_CONSUME_DATA,                         "transport.consumeData"                      },
		{ FBS::Request::Method::TRANSPORT_ENABLE_TRACE_EVENT,                   "transport.enableTraceEvent"                 },
		{ FBS::Request::Method::TRANSPORT_CLOSE_PRODUCER,                       "transport.closeProducer"                    },
		{ FBS::Request::Method::TRANSPORT_CLOSE_CONSUMER,                       "transport.closeConsumer"                    },
		{ FBS::Request::Method::TRANSPORT_CLOSE_DATAPRODUCER,                   "transport.closeDataProducer"                },
		{ FBS::Request::Method::TRANSPORT_CLOSE_DATACONSUMER,                   "transport.closeDataConsumer"                },
		{ FBS::Request::Method::PLAINTRANSPORT_CONNECT,                         "plainTransport.connect"                     },
		{ FBS::Request::Method::PIPETRANSPORT_CONNECT,                          "pipeTransport.connect"                      },
		{ FBS::Request::Method::WEBRTCTRANSPORT_CONNECT,                        "webRtcTransport.connect"                    },
		{ FBS::Request::Method::PRODUCER_DUMP,                                  "producer.dump"                              },
		{ FBS::Request::Method::PRODUCER_GET_STATS,                             "producer.getStats"                          },
		{ FBS::Request::Method::PRODUCER_PAUSE,                                 "producer.pause"                             },
		{ FBS::Request::Method::PRODUCER_RESUME,                                "producer.resume"                            },
		{ FBS::Request::Method::PRODUCER_ENABLE_TRACE_EVENT,                    "producer.enableTraceEvent"                  },
		{ FBS::Request::Method::CONSUMER_DUMP,                                  "consumer.dump"                              },
		{ FBS::Request::Method::CONSUMER_GET_STATS,                             "consumer.getStats"                          },
		{ FBS::Request::Method::CONSUMER_PAUSE,                                 "consumer.pause"                             },
		{ FBS::Request::Method::CONSUMER_RESUME,                                "consumer.resume"                            },
		{ FBS::Request::Method::CONSUMER_SET_PREFERRED_LAYERS,                  "consumer.setPreferredLayers"                },
		{ FBS::Request::Method::CONSUMER_SET_PRIORITY,                          "consumer.setPriority"                       },
		{ FBS::Request::Method::CONSUMER_REQUEST_KEY_FRAME,                     "consumer.requestKeyFrame"                   },
		{ FBS::Request::Method::CONSUMER_ENABLE_TRACE_EVENT,                    "consumer.enableTraceEvent"                  },
		{ FBS::Request::Method::DATAPRODUCER_DUMP,                              "dataProducer.dump"                          },
		{ FBS::Request::Method::DATAPRODUCER_GET_STATS,                         "dataProducer.getStats"                      },
		{ FBS::Request::Method::DATAPRODUCER_PAUSE,                             "dataProducer.pause"                         },
		{ FBS::Request::Method::DATAPRODUCER_RESUME,                            "dataProducer.resume"                        },
		{ FBS::Request::Method::DATACONSUMER_DUMP,                              "dataConsumer.dump"                          },
		{ FBS::Request::Method::DATACONSUMER_GET_STATS,                         "dataConsumer.getStats"                      },
		{ FBS::Request::Method::DATACONSUMER_PAUSE,                             "dataConsumer.pause"                         },
		{ FBS::Request::Method::DATACONSUMER_RESUME,                            "dataConsumer.resume"                        },
		{ FBS::Request::Method::DATACONSUMER_GET_BUFFERED_AMOUNT,               "dataConsumer.getBufferedAmount"             },
		{ FBS::Request::Method::DATACONSUMER_SET_BUFFERED_AMOUNT_LOW_THRESHOLD, "dataConsumer.setBufferedAmountLowThreshold" },
		{ FBS::Request::Method::DATACONSUMER_SEND,                              "dataConsumer.send"                          },
		{ FBS::Request::Method::DATACONSUMER_SET_SUBCHANNELS,                   "dataConsumer.setSubchannels"                },
		{ FBS::Request::Method::DATACONSUMER_ADD_SUBCHANNEL,                    "dataConsumer.addSubchannel"                 },
		{ FBS::Request::Method::DATACONSUMER_REMOVE_SUBCHANNEL,                 "dataConsumer.removeSubchannel"              },
		{ FBS::Request::Method::RTPOBSERVER_PAUSE,                              "rtpObserver.pause"                          },
		{ FBS::Request::Method::RTPOBSERVER_RESUME,                             "rtpObserver.resume"                         },
		{ FBS::Request::Method::RTPOBSERVER_ADD_PRODUCER,                       "rtpObserver.addProducer"                    },
		{ FBS::Request::Method::RTPOBSERVER_REMOVE_PRODUCER,                    "rtpObserver.removeProducer"                 },
	};
	// clang-format on

	/* Instance methods. */

	/**
	 * msg contains the request flatbuffer.
	 */
	ChannelRequest::ChannelRequest(Channel::ChannelSocket* channel, const FBS::Request::Request* request)
	  : channel(channel)
	{
		MS_TRACE();

		this->data   = request;
		this->id     = request->id();
		this->method = request->method();

		auto methodCStrIt = ChannelRequest::method2String.find(this->method);

		if (methodCStrIt == ChannelRequest::method2String.end())
		{
			Error("unknown method");

			MS_THROW_ERROR("unknown method '%" PRIu8 "'", static_cast<uint8_t>(this->method));
		}

		this->methodCStr = methodCStrIt->second;
		this->handlerId  = this->data->handlerId()->str();
	}

	void ChannelRequest::Accept()
	{
		MS_TRACE();

		MS_ASSERT(!this->replied, "request already replied");

		this->replied = true;

		auto& builder = ChannelRequest::bufferBuilder;
		auto response =
		  FBS::Response::CreateResponse(builder, this->id, true, FBS::Response::Body::NONE, 0);

		this->SendResponse(response);
	}

	void ChannelRequest::Error(const char* reason)
	{
		MS_TRACE();

		MS_ASSERT(!this->replied, "request already replied");

		this->replied = true;

		auto& builder = ChannelRequest::bufferBuilder;
		auto response = FBS::Response::CreateResponseDirect(
		  builder, this->id, false /*accepted*/, FBS::Response::Body::NONE, 0, "Error" /*Error*/, reason);

		this->SendResponse(response);
	}

	void ChannelRequest::TypeError(const char* reason)
	{
		MS_TRACE();

		MS_ASSERT(!this->replied, "request already replied");

		this->replied = true;

		auto& builder = ChannelRequest::bufferBuilder;
		auto response = FBS::Response::CreateResponseDirect(
		  builder, this->id, false /*accepted*/, FBS::Response::Body::NONE, 0, "TypeError" /*Error*/, reason);

		this->SendResponse(response);
	}

	void ChannelRequest::Send(uint8_t* buffer, size_t size) const
	{
		this->channel->Send(buffer, size);
	}

	void ChannelRequest::SendResponse(const flatbuffers::Offset<FBS::Response::Response>& response)
	{
		auto& builder = ChannelRequest::bufferBuilder;
		auto message =
		  FBS::Message::CreateMessage(builder, FBS::Message::Body::Response, response.Union());

		builder.FinishSizePrefixed(message);
		this->Send(builder.GetBufferPointer(), builder.GetSize());
		builder.Clear();
	}
} // namespace Channel
