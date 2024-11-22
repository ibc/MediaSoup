use crate::active_speaker_observer::ActiveSpeakerObserverOptions;
use crate::audio_level_observer::AudioLevelObserverOptions;
use crate::consumer::{
    ConsumerId, ConsumerLayers, ConsumerScore, ConsumerTraceEventType, ConsumerType,
};
use crate::data_consumer::{DataConsumerId, DataConsumerType};
use crate::data_producer::{DataProducerId, DataProducerType};
use crate::data_structures::{
    DtlsParameters, DtlsRole, DtlsState, IceCandidate, IceParameters, IceRole, IceState,
    ListenInfo, SctpState, TransportTuple,
};
use crate::direct_transport::DirectTransportOptions;
use crate::ortc::RtpMapping;
use crate::pipe_transport::PipeTransportOptions;
use crate::plain_transport::PlainTransportOptions;
use crate::producer::{ProducerId, ProducerTraceEventType, ProducerType};
use crate::router::consumer::ConsumerDump;
use crate::router::producer::ProducerDump;
use crate::router::{RouterDump, RouterId};
use crate::rtp_observer::RtpObserverId;
use crate::rtp_parameters::{MediaKind, RtpEncodingParameters, RtpParameters};
use crate::sctp_parameters::{NumSctpStreams, SctpParameters, SctpStreamParameters};
use crate::srtp_parameters::{SrtpCryptoSuite, SrtpParameters};
use crate::transport::{TransportId, TransportTraceEventType};
use crate::webrtc_server::{
    WebRtcServerDump, WebRtcServerIceUsernameFragment, WebRtcServerId, WebRtcServerIpPort,
    WebRtcServerListenInfos, WebRtcServerTupleHash,
};
use crate::webrtc_transport::{
    WebRtcTransportListen, WebRtcTransportListenInfos, WebRtcTransportOptions,
};
use crate::worker::{ChannelMessageHandlers, LibUringDump, WorkerDump, WorkerUpdateSettings};
use mediasoup_sys::fbs::{
    active_speaker_observer, audio_level_observer, consumer, data_consumer, data_producer,
    direct_transport, message, notification, pipe_transport, plain_transport, producer, request,
    response, router, rtp_observer, transport, web_rtc_server, web_rtc_transport, worker,
};
use parking_lot::Mutex;
use planus::Builder;
use serde::{Deserialize, Serialize};
use std::error::Error;
use std::fmt::{Debug, Display};
use std::net::IpAddr;
use std::num::NonZeroU16;

pub(crate) trait Request
where
    Self: Debug,
{
    /// Request method to call on worker.
    const METHOD: request::Method;
    type HandlerId: Display;
    type Response;

    /// Get a serialized message out of this request.
    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8>;

    /// Default response to return in case of soft error, such as channel already closed, entity
    /// doesn't exist on worker during closing.
    fn default_for_soft_error() -> Option<Self::Response> {
        None
    }

    /// Convert generic response into specific type of this request.
    fn convert_response(
        response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>>;
}

pub(crate) trait Notification: Debug {
    /// Notification event to call on worker.
    const EVENT: notification::Event;
    type HandlerId: Display;

    /// Get a serialized message out of this notification.
    fn into_bytes(self, handler_id: Self::HandlerId) -> Vec<u8>;
}

#[derive(Debug)]
pub(crate) struct WorkerCloseRequest {}

impl Request for WorkerCloseRequest {
    const METHOD: request::Method = request::Method::WorkerClose;
    type HandlerId = &'static str;
    type Response = ();

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            None::<request::Body>,
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        _response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        Ok(())
    }
}

#[derive(Debug)]
pub(crate) struct WorkerDumpRequest {}

impl Request for WorkerDumpRequest {
    const METHOD: request::Method = request::Method::WorkerDump;
    type HandlerId = &'static str;
    type Response = WorkerDump;

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            None::<request::Body>,
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        let Some(response::BodyRef::WorkerDumpResponse(data)) = response else {
            panic!("Wrong message from worker: {response:?}");
        };

        let data = worker::DumpResponse::try_from(data)?;

        Ok(WorkerDump {
            router_ids: data
                .router_ids
                .into_iter()
                .map(|id| id.parse())
                .collect::<Result<_, _>>()?,
            webrtc_server_ids: data
                .web_rtc_server_ids
                .into_iter()
                .map(|id| id.parse())
                .collect::<Result<_, _>>()?,
            channel_message_handlers: ChannelMessageHandlers {
                channel_request_handlers: data
                    .channel_message_handlers
                    .channel_request_handlers
                    .into_iter()
                    .map(|id| id.parse())
                    .collect::<Result<_, _>>()?,
                channel_notification_handlers: data
                    .channel_message_handlers
                    .channel_notification_handlers
                    .into_iter()
                    .map(|id| id.parse())
                    .collect::<Result<_, _>>()?,
            },
            liburing: data.liburing.map(|liburing| LibUringDump {
                sqe_process_count: liburing.sqe_process_count,
                sqe_miss_count: liburing.sqe_miss_count,
                user_data_miss_count: liburing.user_data_miss_count,
            }),
        })
    }
}

#[derive(Debug)]
pub(crate) struct WorkerUpdateSettingsRequest {
    pub(crate) data: WorkerUpdateSettings,
}

impl Request for WorkerUpdateSettingsRequest {
    const METHOD: request::Method = request::Method::WorkerUpdateSettings;
    type HandlerId = &'static str;
    type Response = ();

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();
        let data = worker::UpdateSettingsRequest::create(
            &mut builder,
            self.data
                .log_level
                .as_ref()
                .map(|log_level| log_level.as_str()),
            self.data.log_tags.as_ref().map(|log_tags| {
                log_tags
                    .iter()
                    .map(|log_tag| log_tag.as_str())
                    .collect::<Vec<_>>()
            }),
        );
        let request_body = request::Body::create_worker_update_settings_request(&mut builder, data);
        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        _response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        Ok(())
    }
}

#[derive(Debug)]
pub(crate) struct WorkerCreateWebRtcServerRequest {
    pub(crate) webrtc_server_id: WebRtcServerId,
    pub(crate) listen_infos: WebRtcServerListenInfos,
}

impl Request for WorkerCreateWebRtcServerRequest {
    const METHOD: request::Method = request::Method::WorkerCreateWebrtcserver;
    type HandlerId = &'static str;
    type Response = ();

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();
        let data = worker::CreateWebRtcServerRequest::create(
            &mut builder,
            self.webrtc_server_id.to_string(),
            self.listen_infos.to_fbs(),
        );
        let request_body =
            request::Body::create_worker_create_web_rtc_server_request(&mut builder, data);
        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        _response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        Ok(())
    }
}

#[derive(Debug)]
pub(crate) struct WebRtcServerCloseRequest {
    pub(crate) webrtc_server_id: WebRtcServerId,
}

impl Request for WebRtcServerCloseRequest {
    const METHOD: request::Method = request::Method::WorkerWebrtcserverClose;
    type HandlerId = &'static str;
    type Response = ();

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let data = worker::CloseWebRtcServerRequest::create(
            &mut builder,
            self.webrtc_server_id.to_string(),
        );
        let request_body =
            request::Body::create_worker_close_web_rtc_server_request(&mut builder, data);
        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        _response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        Ok(())
    }
}

#[derive(Debug)]
pub(crate) struct WebRtcServerDumpRequest {}

impl Request for WebRtcServerDumpRequest {
    const METHOD: request::Method = request::Method::WebrtcserverDump;
    type HandlerId = WebRtcServerId;
    type Response = WebRtcServerDump;

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            None::<request::Body>,
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        let Some(response::BodyRef::WebRtcServerDumpResponse(data)) = response else {
            panic!("Wrong message from worker: {response:?}");
        };

        let data = web_rtc_server::DumpResponse::try_from(data)?;

        Ok(WebRtcServerDump {
            id: data.id.parse()?,
            udp_sockets: data
                .udp_sockets
                .into_iter()
                .map(|ip_port| WebRtcServerIpPort {
                    ip: ip_port.ip.parse().unwrap(),
                    port: ip_port.port,
                })
                .collect(),
            tcp_servers: data
                .tcp_servers
                .into_iter()
                .map(|ip_port| WebRtcServerIpPort {
                    ip: ip_port.ip.parse().unwrap(),
                    port: ip_port.port,
                })
                .collect(),
            webrtc_transport_ids: data
                .web_rtc_transport_ids
                .into_iter()
                .map(|id| id.parse())
                .collect::<Result<_, _>>()?,
            local_ice_username_fragments: data
                .local_ice_username_fragments
                .into_iter()
                .map(|username_fragment| WebRtcServerIceUsernameFragment {
                    local_ice_username_fragment: username_fragment
                        .local_ice_username_fragment
                        .parse()
                        .unwrap(),
                    webrtc_transport_id: username_fragment.web_rtc_transport_id.parse().unwrap(),
                })
                .collect(),
            tuple_hashes: data
                .tuple_hashes
                .into_iter()
                .map(|tuple_hash| WebRtcServerTupleHash {
                    tuple_hash: tuple_hash.tuple_hash,
                    webrtc_transport_id: tuple_hash.web_rtc_transport_id.parse().unwrap(),
                })
                .collect(),
        })
    }
}

#[derive(Debug)]
pub(crate) struct WorkerCreateRouterRequest {
    pub(crate) router_id: RouterId,
}

impl Request for WorkerCreateRouterRequest {
    const METHOD: request::Method = request::Method::WorkerCreateRouter;
    type HandlerId = &'static str;
    type Response = ();

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();
        let data = worker::CreateRouterRequest::create(&mut builder, self.router_id.to_string());
        let request_body = request::Body::create_worker_create_router_request(&mut builder, data);
        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        _response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        Ok(())
    }
}

#[derive(Debug)]
pub(crate) struct RouterCloseRequest {
    pub(crate) router_id: RouterId,
}

impl Request for RouterCloseRequest {
    const METHOD: request::Method = request::Method::WorkerCloseRouter;
    type HandlerId = &'static str;
    type Response = ();

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let data = worker::CloseRouterRequest::create(&mut builder, self.router_id.to_string());
        let request_body = request::Body::create_worker_close_router_request(&mut builder, data);
        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        _response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        Ok(())
    }
}

#[derive(Debug)]
pub(crate) struct RouterDumpRequest {}

impl Request for RouterDumpRequest {
    const METHOD: request::Method = request::Method::RouterDump;
    type HandlerId = RouterId;
    type Response = RouterDump;

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            None::<request::Body>,
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        let Some(response::BodyRef::RouterDumpResponse(data)) = response else {
            panic!("Wrong message from worker: {response:?}");
        };

        let data = router::DumpResponse::try_from(data)?;

        Ok(RouterDump {
            id: data.id.parse()?,
            map_consumer_id_producer_id: data
                .map_consumer_id_producer_id
                .into_iter()
                .map(|key_value| Ok((key_value.key.parse()?, key_value.value.parse()?)))
                .collect::<Result<_, Box<dyn Error + Send + Sync>>>()?,
            map_data_consumer_id_data_producer_id: data
                .map_data_consumer_id_data_producer_id
                .into_iter()
                .map(|key_value| Ok((key_value.key.parse()?, key_value.value.parse()?)))
                .collect::<Result<_, Box<dyn Error + Send + Sync>>>()?,
            map_data_producer_id_data_consumer_ids: data
                .map_data_producer_id_data_consumer_ids
                .into_iter()
                .map(|key_values| {
                    Ok((
                        key_values.key.parse()?,
                        key_values
                            .values
                            .into_iter()
                            .map(|value| value.parse())
                            .collect::<Result<_, _>>()?,
                    ))
                })
                .collect::<Result<_, Box<dyn Error + Send + Sync>>>()?,
            map_producer_id_consumer_ids: data
                .map_producer_id_consumer_ids
                .into_iter()
                .map(|key_values| {
                    Ok((
                        key_values.key.parse()?,
                        key_values
                            .values
                            .into_iter()
                            .map(|value| value.parse())
                            .collect::<Result<_, _>>()?,
                    ))
                })
                .collect::<Result<_, Box<dyn Error + Send + Sync>>>()?,
            map_producer_id_observer_ids: data
                .map_producer_id_observer_ids
                .into_iter()
                .map(|key_values| {
                    Ok((
                        key_values.key.parse()?,
                        key_values
                            .values
                            .into_iter()
                            .map(|value| value.parse())
                            .collect::<Result<_, _>>()?,
                    ))
                })
                .collect::<Result<_, Box<dyn Error + Send + Sync>>>()?,
            rtp_observer_ids: data
                .rtp_observer_ids
                .into_iter()
                .map(|id| id.parse())
                .collect::<Result<_, _>>()?,
            transport_ids: data
                .transport_ids
                .into_iter()
                .map(|id| id.parse())
                .collect::<Result<_, _>>()?,
        })
    }
}

#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
pub(crate) struct RouterCreateDirectTransportData {
    transport_id: TransportId,
    direct: bool,
    max_message_size: u32,
}

impl RouterCreateDirectTransportData {
    pub(crate) fn from_options(
        transport_id: TransportId,
        direct_transport_options: &DirectTransportOptions,
    ) -> Self {
        Self {
            transport_id,
            direct: true,
            max_message_size: direct_transport_options.max_message_size,
        }
    }

    pub(crate) fn to_fbs(&self) -> direct_transport::DirectTransportOptions {
        direct_transport::DirectTransportOptions {
            base: Box::new(transport::Options {
                direct: true,
                max_message_size: Some(self.max_message_size),
                initial_available_outgoing_bitrate: None,
                enable_sctp: false,
                num_sctp_streams: None,
                max_sctp_message_size: 0,
                sctp_send_buffer_size: 0,
                is_data_channel: false,
            }),
        }
    }
}

#[derive(Debug)]
pub(crate) struct RouterCreateDirectTransportRequest {
    pub(crate) data: RouterCreateDirectTransportData,
}

impl Request for RouterCreateDirectTransportRequest {
    const METHOD: request::Method = request::Method::RouterCreateDirecttransport;
    type HandlerId = RouterId;
    type Response = ();

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();
        let data = router::CreateDirectTransportRequest::create(
            &mut builder,
            self.data.transport_id.to_string(),
            self.data.to_fbs(),
        );
        let request_body =
            request::Body::create_router_create_direct_transport_request(&mut builder, data);
        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        _response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        Ok(())
    }
}

#[derive(Debug, Serialize)]
#[serde(untagged)]
enum RouterCreateWebrtcTransportListen {
    #[serde(rename_all = "camelCase")]
    Individual {
        listen_infos: WebRtcTransportListenInfos,
    },
    Server {
        #[serde(rename = "webRtcServerId")]
        webrtc_server_id: WebRtcServerId,
    },
}

impl RouterCreateWebrtcTransportListen {
    pub(crate) fn to_fbs(&self) -> web_rtc_transport::Listen {
        match self {
            RouterCreateWebrtcTransportListen::Individual { listen_infos } => {
                web_rtc_transport::Listen::ListenIndividual(Box::new(
                    web_rtc_transport::ListenIndividual {
                        listen_infos: listen_infos
                            .iter()
                            .map(|listen_info| listen_info.to_fbs())
                            .collect(),
                    },
                ))
            }
            RouterCreateWebrtcTransportListen::Server { webrtc_server_id } => {
                web_rtc_transport::Listen::ListenServer(Box::new(web_rtc_transport::ListenServer {
                    web_rtc_server_id: webrtc_server_id.to_string(),
                }))
            }
        }
    }
}

#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
pub(crate) struct RouterCreateWebrtcTransportData {
    transport_id: TransportId,
    #[serde(flatten)]
    listen: RouterCreateWebrtcTransportListen,
    initial_available_outgoing_bitrate: u32,
    enable_udp: bool,
    enable_tcp: bool,
    prefer_udp: bool,
    prefer_tcp: bool,
    ice_consent_timeout: u8,
    enable_sctp: bool,
    num_sctp_streams: NumSctpStreams,
    max_sctp_message_size: u32,
    sctp_send_buffer_size: u32,
    is_data_channel: bool,
}

impl RouterCreateWebrtcTransportData {
    pub(crate) fn from_options(
        transport_id: TransportId,
        webrtc_transport_options: &WebRtcTransportOptions,
    ) -> Self {
        Self {
            transport_id,
            listen: match &webrtc_transport_options.listen {
                WebRtcTransportListen::Individual { listen_infos } => {
                    RouterCreateWebrtcTransportListen::Individual {
                        listen_infos: listen_infos.clone(),
                    }
                }
                WebRtcTransportListen::Server { webrtc_server } => {
                    RouterCreateWebrtcTransportListen::Server {
                        webrtc_server_id: webrtc_server.id(),
                    }
                }
            },
            initial_available_outgoing_bitrate: webrtc_transport_options
                .initial_available_outgoing_bitrate,
            enable_udp: webrtc_transport_options.enable_udp,
            enable_tcp: webrtc_transport_options.enable_tcp,
            prefer_udp: webrtc_transport_options.prefer_udp,
            prefer_tcp: webrtc_transport_options.prefer_tcp,
            ice_consent_timeout: webrtc_transport_options.ice_consent_timeout,
            enable_sctp: webrtc_transport_options.enable_sctp,
            num_sctp_streams: webrtc_transport_options.num_sctp_streams,
            max_sctp_message_size: webrtc_transport_options.max_sctp_message_size,
            sctp_send_buffer_size: webrtc_transport_options.sctp_send_buffer_size,
            is_data_channel: true,
        }
    }

    pub(crate) fn to_fbs(&self) -> web_rtc_transport::WebRtcTransportOptions {
        web_rtc_transport::WebRtcTransportOptions {
            base: Box::new(transport::Options {
                direct: false,
                max_message_size: None,
                initial_available_outgoing_bitrate: Some(self.initial_available_outgoing_bitrate),
                enable_sctp: self.enable_sctp,
                num_sctp_streams: Some(Box::new(self.num_sctp_streams.to_fbs())),
                max_sctp_message_size: self.max_sctp_message_size,
                sctp_send_buffer_size: self.sctp_send_buffer_size,
                is_data_channel: true,
            }),
            listen: self.listen.to_fbs(),
            enable_udp: self.enable_udp,
            enable_tcp: self.enable_tcp,
            prefer_udp: self.prefer_udp,
            prefer_tcp: self.prefer_tcp,
            ice_consent_timeout: self.ice_consent_timeout,
        }
    }
}

#[derive(Debug, Deserialize)]
#[serde(rename_all = "camelCase")]
pub(crate) struct WebRtcTransportData {
    pub(crate) ice_role: IceRole,
    pub(crate) ice_parameters: IceParameters,
    pub(crate) ice_candidates: Vec<IceCandidate>,
    pub(crate) ice_state: Mutex<IceState>,
    pub(crate) ice_selected_tuple: Mutex<Option<TransportTuple>>,
    pub(crate) dtls_parameters: Mutex<DtlsParameters>,
    pub(crate) dtls_state: Mutex<DtlsState>,
    pub(crate) dtls_remote_cert: Mutex<Option<String>>,
    pub(crate) sctp_parameters: Option<SctpParameters>,
    pub(crate) sctp_state: Mutex<Option<SctpState>>,
}

#[derive(Debug)]
pub(crate) struct RouterCreateWebRtcTransportRequest {
    pub(crate) data: RouterCreateWebrtcTransportData,
}

impl Request for RouterCreateWebRtcTransportRequest {
    const METHOD: request::Method = request::Method::RouterCreateWebrtctransport;
    type HandlerId = RouterId;
    type Response = WebRtcTransportData;

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let RouterCreateWebrtcTransportListen::Individual { listen_infos: _ } = self.data.listen
        else {
            panic!("RouterCreateWebrtcTransportListen variant must be Individual");
        };

        let mut builder = Builder::new();
        let data = router::CreateWebRtcTransportRequest::create(
            &mut builder,
            self.data.transport_id.to_string(),
            self.data.to_fbs(),
        );
        let request_body =
            request::Body::create_router_create_web_rtc_transport_request(&mut builder, data);
        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        let Some(response::BodyRef::WebRtcTransportDumpResponse(data)) = response else {
            panic!("Wrong message from worker: {response:?}");
        };

        let data = web_rtc_transport::DumpResponse::try_from(data)?;

        Ok(WebRtcTransportData {
            ice_role: IceRole::from_fbs(data.ice_role),
            ice_parameters: IceParameters::from_fbs(*data.ice_parameters),
            ice_candidates: data
                .ice_candidates
                .iter()
                .map(IceCandidate::from_fbs)
                .collect(),
            ice_state: Mutex::new(IceState::from_fbs(data.ice_state)),
            ice_selected_tuple: Mutex::new(
                data.ice_selected_tuple
                    .map(|tuple| TransportTuple::from_fbs(tuple.as_ref())),
            ),
            dtls_parameters: Mutex::new(DtlsParameters::from_fbs(*data.dtls_parameters)),
            dtls_state: Mutex::new(DtlsState::from_fbs(data.dtls_state)),
            dtls_remote_cert: Mutex::new(None),
            sctp_parameters: data
                .base
                .sctp_parameters
                .map(|parameters| SctpParameters::from_fbs(parameters.as_ref())),
            sctp_state: Mutex::new(
                data.base
                    .sctp_state
                    .map(|state| SctpState::from_fbs(&state)),
            ),
        })
    }
}

#[derive(Debug)]
pub(crate) struct RouterCreateWebRtcTransportWithServerRequest {
    pub(crate) data: RouterCreateWebrtcTransportData,
}

impl Request for RouterCreateWebRtcTransportWithServerRequest {
    const METHOD: request::Method = request::Method::RouterCreateWebrtctransportWithServer;
    type HandlerId = RouterId;
    type Response = WebRtcTransportData;

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let RouterCreateWebrtcTransportListen::Server {
            webrtc_server_id: _,
        } = self.data.listen
        else {
            panic!("RouterCreateWebrtcTransportListen variant must be Server");
        };

        let mut builder = Builder::new();
        let data = router::CreateWebRtcTransportRequest::create(
            &mut builder,
            self.data.transport_id.to_string(),
            self.data.to_fbs(),
        );
        let request_body =
            request::Body::create_router_create_web_rtc_transport_request(&mut builder, data);
        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        let Some(response::BodyRef::WebRtcTransportDumpResponse(data)) = response else {
            panic!("Wrong message from worker: {response:?}");
        };

        let data = web_rtc_transport::DumpResponse::try_from(data)?;

        Ok(WebRtcTransportData {
            ice_role: IceRole::from_fbs(data.ice_role),
            ice_parameters: IceParameters::from_fbs(*data.ice_parameters),
            ice_candidates: data
                .ice_candidates
                .iter()
                .map(IceCandidate::from_fbs)
                .collect(),
            ice_state: Mutex::new(IceState::from_fbs(data.ice_state)),
            ice_selected_tuple: Mutex::new(
                data.ice_selected_tuple
                    .map(|tuple| TransportTuple::from_fbs(tuple.as_ref())),
            ),
            dtls_parameters: Mutex::new(DtlsParameters::from_fbs(*data.dtls_parameters)),
            dtls_state: Mutex::new(DtlsState::from_fbs(data.dtls_state)),
            dtls_remote_cert: Mutex::new(None),
            sctp_parameters: data
                .base
                .sctp_parameters
                .map(|parameters| SctpParameters::from_fbs(parameters.as_ref())),
            sctp_state: Mutex::new(
                data.base
                    .sctp_state
                    .map(|state| SctpState::from_fbs(&state)),
            ),
        })
    }
}

#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
pub(crate) struct RouterCreatePlainTransportData {
    transport_id: TransportId,
    listen_info: ListenInfo,
    rtcp_listen_info: Option<ListenInfo>,
    rtcp_mux: bool,
    comedia: bool,
    enable_sctp: bool,
    num_sctp_streams: NumSctpStreams,
    max_sctp_message_size: u32,
    sctp_send_buffer_size: u32,
    enable_srtp: bool,
    srtp_crypto_suite: SrtpCryptoSuite,
    is_data_channel: bool,
}

impl RouterCreatePlainTransportData {
    pub(crate) fn from_options(
        transport_id: TransportId,
        plain_transport_options: &PlainTransportOptions,
    ) -> Self {
        Self {
            transport_id,
            listen_info: plain_transport_options.listen_info.clone(),
            rtcp_listen_info: plain_transport_options.rtcp_listen_info.clone(),
            rtcp_mux: plain_transport_options.rtcp_mux,
            comedia: plain_transport_options.comedia,
            enable_sctp: plain_transport_options.enable_sctp,
            num_sctp_streams: plain_transport_options.num_sctp_streams,
            max_sctp_message_size: plain_transport_options.max_sctp_message_size,
            sctp_send_buffer_size: plain_transport_options.sctp_send_buffer_size,
            enable_srtp: plain_transport_options.enable_srtp,
            srtp_crypto_suite: plain_transport_options.srtp_crypto_suite,
            is_data_channel: false,
        }
    }

    pub(crate) fn to_fbs(&self) -> plain_transport::PlainTransportOptions {
        plain_transport::PlainTransportOptions {
            base: Box::new(transport::Options {
                direct: false,
                max_message_size: None,
                initial_available_outgoing_bitrate: None,
                enable_sctp: self.enable_sctp,
                num_sctp_streams: Some(Box::new(self.num_sctp_streams.to_fbs())),
                max_sctp_message_size: self.max_sctp_message_size,
                sctp_send_buffer_size: self.sctp_send_buffer_size,
                is_data_channel: self.is_data_channel,
            }),
            listen_info: Box::new(self.listen_info.clone().to_fbs()),
            rtcp_listen_info: self
                .rtcp_listen_info
                .clone()
                .map(|listen_info| Box::new(listen_info.to_fbs())),
            rtcp_mux: self.rtcp_mux,
            comedia: self.comedia,
            enable_srtp: self.enable_srtp,
            srtp_crypto_suite: Some(SrtpCryptoSuite::to_fbs(self.srtp_crypto_suite)),
        }
    }
}

#[derive(Debug)]
pub(crate) struct RouterCreatePlainTransportRequest {
    pub(crate) data: RouterCreatePlainTransportData,
}

impl Request for RouterCreatePlainTransportRequest {
    const METHOD: request::Method = request::Method::RouterCreatePlaintransport;
    type HandlerId = RouterId;
    type Response = PlainTransportData;

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();
        let data = router::CreatePlainTransportRequest::create(
            &mut builder,
            self.data.transport_id.to_string(),
            self.data.to_fbs(),
        );
        let request_body =
            request::Body::create_router_create_plain_transport_request(&mut builder, data);
        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        let Some(response::BodyRef::PlainTransportDumpResponse(data)) = response else {
            panic!("Wrong message from worker: {response:?}");
        };

        let data = plain_transport::DumpResponse::try_from(data)?;

        Ok(PlainTransportData {
            tuple: Mutex::new(TransportTuple::from_fbs(data.tuple.as_ref())),
            rtcp_tuple: Mutex::new(
                data.rtcp_tuple
                    .map(|tuple| TransportTuple::from_fbs(tuple.as_ref())),
            ),
            sctp_parameters: data
                .base
                .sctp_parameters
                .map(|parameters| SctpParameters::from_fbs(parameters.as_ref())),
            sctp_state: Mutex::new(
                data.base
                    .sctp_state
                    .map(|state| SctpState::from_fbs(&state)),
            ),
            srtp_parameters: Mutex::new(
                data.srtp_parameters
                    .map(|parameters| SrtpParameters::from_fbs(parameters.as_ref())),
            ),
        })
    }
}

pub(crate) struct PlainTransportData {
    // The following fields are present, but unused
    // rtcp_mux: bool,
    // comedia: bool,
    pub(crate) tuple: Mutex<TransportTuple>,
    pub(crate) rtcp_tuple: Mutex<Option<TransportTuple>>,
    pub(crate) sctp_parameters: Option<SctpParameters>,
    pub(crate) sctp_state: Mutex<Option<SctpState>>,
    pub(crate) srtp_parameters: Mutex<Option<SrtpParameters>>,
}

#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
pub(crate) struct RouterCreatePipeTransportData {
    transport_id: TransportId,
    listen_info: ListenInfo,
    enable_sctp: bool,
    num_sctp_streams: NumSctpStreams,
    max_sctp_message_size: u32,
    sctp_send_buffer_size: u32,
    enable_rtx: bool,
    enable_srtp: bool,
    is_data_channel: bool,
}

impl RouterCreatePipeTransportData {
    pub(crate) fn from_options(
        transport_id: TransportId,
        pipe_transport_options: &PipeTransportOptions,
    ) -> Self {
        Self {
            transport_id,
            listen_info: pipe_transport_options.listen_info.clone(),
            enable_sctp: pipe_transport_options.enable_sctp,
            num_sctp_streams: pipe_transport_options.num_sctp_streams,
            max_sctp_message_size: pipe_transport_options.max_sctp_message_size,
            sctp_send_buffer_size: pipe_transport_options.sctp_send_buffer_size,
            enable_rtx: pipe_transport_options.enable_rtx,
            enable_srtp: pipe_transport_options.enable_srtp,
            is_data_channel: false,
        }
    }

    pub(crate) fn to_fbs(&self) -> pipe_transport::PipeTransportOptions {
        pipe_transport::PipeTransportOptions {
            base: Box::new(transport::Options {
                direct: false,
                max_message_size: None,
                initial_available_outgoing_bitrate: None,
                enable_sctp: self.enable_sctp,
                num_sctp_streams: Some(Box::new(self.num_sctp_streams.to_fbs())),
                max_sctp_message_size: self.max_sctp_message_size,
                sctp_send_buffer_size: self.sctp_send_buffer_size,
                is_data_channel: self.is_data_channel,
            }),
            listen_info: Box::new(self.listen_info.clone().to_fbs()),
            enable_rtx: self.enable_rtx,
            enable_srtp: self.enable_srtp,
        }
    }
}

#[derive(Debug)]
pub(crate) struct RouterCreatePipeTransportRequest {
    pub(crate) data: RouterCreatePipeTransportData,
}

impl Request for RouterCreatePipeTransportRequest {
    const METHOD: request::Method = request::Method::RouterCreatePipetransport;
    type HandlerId = RouterId;
    type Response = PipeTransportData;

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();
        let data = router::CreatePipeTransportRequest::create(
            &mut builder,
            self.data.transport_id.to_string(),
            self.data.to_fbs(),
        );
        let request_body =
            request::Body::create_router_create_pipe_transport_request(&mut builder, data);
        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        let Some(response::BodyRef::PipeTransportDumpResponse(data)) = response else {
            panic!("Wrong message from worker: {response:?}");
        };

        let data = pipe_transport::DumpResponse::try_from(data)?;

        Ok(PipeTransportData {
            tuple: Mutex::new(TransportTuple::from_fbs(data.tuple.as_ref())),
            sctp_parameters: data
                .base
                .sctp_parameters
                .map(|parameters| SctpParameters::from_fbs(parameters.as_ref())),
            sctp_state: Mutex::new(
                data.base
                    .sctp_state
                    .map(|state| SctpState::from_fbs(&state)),
            ),
            rtx: data.rtx,
            srtp_parameters: Mutex::new(
                data.srtp_parameters
                    .map(|parameters| SrtpParameters::from_fbs(parameters.as_ref())),
            ),
        })
    }
}

pub(crate) struct PipeTransportData {
    pub(crate) tuple: Mutex<TransportTuple>,
    pub(crate) sctp_parameters: Option<SctpParameters>,
    pub(crate) sctp_state: Mutex<Option<SctpState>>,
    pub(crate) rtx: bool,
    pub(crate) srtp_parameters: Mutex<Option<SrtpParameters>>,
}

#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
pub(crate) struct RouterCreateAudioLevelObserverRequest {
    pub(crate) data: RouterCreateAudioLevelObserverData,
}

#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
pub(crate) struct RouterCreateAudioLevelObserverData {
    rtp_observer_id: RtpObserverId,
    max_entries: NonZeroU16,
    threshold: i8,
    interval: u16,
}

impl RouterCreateAudioLevelObserverData {
    pub(crate) fn from_options(
        rtp_observer_id: RtpObserverId,
        audio_level_observer_options: &AudioLevelObserverOptions,
    ) -> Self {
        Self {
            rtp_observer_id,
            max_entries: audio_level_observer_options.max_entries,
            threshold: audio_level_observer_options.threshold,
            interval: audio_level_observer_options.interval,
        }
    }
}

impl Request for RouterCreateAudioLevelObserverRequest {
    const METHOD: request::Method = request::Method::RouterCreateAudiolevelobserver;
    type HandlerId = RouterId;
    type Response = ();

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let options = audio_level_observer::AudioLevelObserverOptions::create(
            &mut builder,
            u16::from(self.data.max_entries),
            self.data.threshold,
            self.data.interval,
        );
        let data = router::CreateAudioLevelObserverRequest::create(
            &mut builder,
            self.data.rtp_observer_id.to_string(),
            options,
        );
        let request_body =
            request::Body::create_router_create_audio_level_observer_request(&mut builder, data);

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        _response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        Ok(())
    }
}

#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
pub(crate) struct RouterCreateActiveSpeakerObserverRequest {
    pub(crate) data: RouterCreateActiveSpeakerObserverData,
}

#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
pub(crate) struct RouterCreateActiveSpeakerObserverData {
    rtp_observer_id: RtpObserverId,
    interval: u16,
}

impl RouterCreateActiveSpeakerObserverData {
    pub(crate) fn from_options(
        rtp_observer_id: RtpObserverId,
        active_speaker_observer_options: &ActiveSpeakerObserverOptions,
    ) -> Self {
        Self {
            rtp_observer_id,
            interval: active_speaker_observer_options.interval,
        }
    }
}

impl Request for RouterCreateActiveSpeakerObserverRequest {
    const METHOD: request::Method = request::Method::RouterCreateActivespeakerobserver;
    type HandlerId = RouterId;
    type Response = ();

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let options = active_speaker_observer::ActiveSpeakerObserverOptions::create(
            &mut builder,
            self.data.interval,
        );
        let data = router::CreateActiveSpeakerObserverRequest::create(
            &mut builder,
            self.data.rtp_observer_id.to_string(),
            options,
        );
        let request_body =
            request::Body::create_router_create_active_speaker_observer_request(&mut builder, data);

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        _response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        Ok(())
    }
}

#[derive(Debug)]
pub(crate) struct TransportDumpRequest {}

impl Request for TransportDumpRequest {
    const METHOD: request::Method = request::Method::TransportDump;
    type HandlerId = TransportId;
    type Response = response::Body;

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            None::<request::Body>,
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        match response {
            Some(data) => Ok(data.try_into().unwrap()),
            _ => {
                panic!("Wrong message from worker: {response:?}");
            }
        }
    }
}
#[derive(Debug)]
pub(crate) struct TransportGetStatsRequest {}

impl Request for TransportGetStatsRequest {
    const METHOD: request::Method = request::Method::TransportGetStats;
    type HandlerId = TransportId;
    type Response = response::Body;

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            None::<request::Body>,
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        match response {
            Some(data) => Ok(data.try_into().unwrap()),
            _ => {
                panic!("Wrong message from worker: {response:?}");
            }
        }
    }
}

#[derive(Debug)]
pub(crate) struct TransportCloseRequest {
    pub(crate) transport_id: TransportId,
}

impl Request for TransportCloseRequest {
    const METHOD: request::Method = request::Method::RouterCloseTransport;
    type HandlerId = RouterId;
    type Response = ();

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();
        let data =
            router::CloseTransportRequest::create(&mut builder, self.transport_id.to_string());
        let request_body = request::Body::create_router_close_transport_request(&mut builder, data);

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        _response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        Ok(())
    }
}

#[derive(Debug)]
pub(crate) struct WebRtcTransportConnectResponse {
    pub(crate) dtls_local_role: DtlsRole,
}

#[derive(Debug)]
pub(crate) struct WebRtcTransportConnectRequest {
    pub(crate) dtls_parameters: DtlsParameters,
}

impl Request for WebRtcTransportConnectRequest {
    const METHOD: request::Method = request::Method::WebrtctransportConnect;
    type HandlerId = TransportId;
    type Response = WebRtcTransportConnectResponse;

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();
        let data =
            web_rtc_transport::ConnectRequest::create(&mut builder, self.dtls_parameters.to_fbs());
        let request_body =
            request::Body::create_web_rtc_transport_connect_request(&mut builder, data);
        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        let Some(response::BodyRef::WebRtcTransportConnectResponse(data)) = response else {
            panic!("Wrong message from worker: {response:?}");
        };

        let data = web_rtc_transport::ConnectResponse::try_from(data)?;

        Ok(WebRtcTransportConnectResponse {
            dtls_local_role: DtlsRole::from_fbs(data.dtls_local_role),
        })
    }
}

#[derive(Debug)]
pub(crate) struct PipeTransportConnectResponse {
    pub(crate) tuple: TransportTuple,
}

#[derive(Debug)]
pub(crate) struct PipeTransportConnectRequest {
    pub(crate) ip: IpAddr,
    pub(crate) port: u16,
    pub(crate) srtp_parameters: Option<SrtpParameters>,
}

impl Request for PipeTransportConnectRequest {
    const METHOD: request::Method = request::Method::PipetransportConnect;
    type HandlerId = TransportId;
    type Response = PipeTransportConnectResponse;

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();
        let data = pipe_transport::ConnectRequest::create(
            &mut builder,
            self.ip.to_string(),
            self.port,
            self.srtp_parameters.map(|parameters| parameters.to_fbs()),
        );
        let request_body = request::Body::create_pipe_transport_connect_request(&mut builder, data);
        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        let Some(response::BodyRef::PipeTransportConnectResponse(data)) = response else {
            panic!("Wrong message from worker: {response:?}");
        };

        let data = pipe_transport::ConnectResponse::try_from(data)?;

        Ok(PipeTransportConnectResponse {
            tuple: TransportTuple::from_fbs(data.tuple.as_ref()),
        })
    }
}

#[derive(Debug)]
pub(crate) struct PlainTransportConnectResponse {
    pub(crate) tuple: TransportTuple,
    pub(crate) rtcp_tuple: Option<TransportTuple>,
    pub(crate) srtp_parameters: Option<SrtpParameters>,
}

#[derive(Debug)]
pub(crate) struct TransportConnectPlainRequest {
    pub(crate) ip: Option<IpAddr>,
    pub(crate) port: Option<u16>,
    pub(crate) rtcp_port: Option<u16>,
    pub(crate) srtp_parameters: Option<SrtpParameters>,
}

impl Request for TransportConnectPlainRequest {
    const METHOD: request::Method = request::Method::PlaintransportConnect;
    type HandlerId = TransportId;
    type Response = PlainTransportConnectResponse;

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();
        let data = plain_transport::ConnectRequest::create(
            &mut builder,
            self.ip.map(|ip| ip.to_string()),
            self.port,
            self.rtcp_port,
            self.srtp_parameters.map(|parameters| parameters.to_fbs()),
        );
        let request_body =
            request::Body::create_plain_transport_connect_request(&mut builder, data);
        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        let Some(response::BodyRef::PlainTransportConnectResponse(data)) = response else {
            panic!("Wrong message from worker: {response:?}");
        };

        let data = plain_transport::ConnectResponse::try_from(data)?;

        Ok(PlainTransportConnectResponse {
            tuple: TransportTuple::from_fbs(data.tuple.as_ref()),
            rtcp_tuple: data
                .rtcp_tuple
                .map(|tuple| TransportTuple::from_fbs(tuple.as_ref())),
            srtp_parameters: data
                .srtp_parameters
                .map(|parameters| SrtpParameters::from_fbs(parameters.as_ref())),
        })
    }
}

#[derive(Debug)]
pub(crate) struct TransportSetMaxIncomingBitrateRequest {
    pub(crate) bitrate: u32,
}

impl Request for TransportSetMaxIncomingBitrateRequest {
    const METHOD: request::Method = request::Method::TransportSetMaxIncomingBitrate;
    type HandlerId = TransportId;
    type Response = ();

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let data = transport::SetMaxIncomingBitrateRequest::create(&mut builder, self.bitrate);
        let request_body =
            request::Body::create_transport_set_max_incoming_bitrate_request(&mut builder, data);
        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        _response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        Ok(())
    }
}

#[derive(Debug)]
pub(crate) struct TransportSetMaxOutgoingBitrateRequest {
    pub(crate) bitrate: u32,
}

impl Request for TransportSetMaxOutgoingBitrateRequest {
    const METHOD: request::Method = request::Method::TransportSetMaxOutgoingBitrate;
    type HandlerId = TransportId;
    type Response = ();

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let data = transport::SetMaxOutgoingBitrateRequest::create(&mut builder, self.bitrate);
        let request_body =
            request::Body::create_transport_set_max_outgoing_bitrate_request(&mut builder, data);
        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        _response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        Ok(())
    }
}

#[derive(Debug)]
pub(crate) struct TransportSetMinOutgoingBitrateRequest {
    pub(crate) bitrate: u32,
}

impl Request for TransportSetMinOutgoingBitrateRequest {
    const METHOD: request::Method = request::Method::TransportSetMinOutgoingBitrate;
    type HandlerId = TransportId;
    type Response = ();

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let data = transport::SetMinOutgoingBitrateRequest::create(&mut builder, self.bitrate);
        let request_body =
            request::Body::create_transport_set_min_outgoing_bitrate_request(&mut builder, data);
        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        _response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        Ok(())
    }
}

#[derive(Debug)]
pub(crate) struct TransportRestartIceRequest {}

impl Request for TransportRestartIceRequest {
    const METHOD: request::Method = request::Method::TransportRestartIce;
    type HandlerId = TransportId;
    type Response = IceParameters;

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();
        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            None::<request::Body>,
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        let Some(response::BodyRef::TransportRestartIceResponse(data)) = response else {
            panic!("Wrong message from worker: {response:?}");
        };

        let data = transport::RestartIceResponse::try_from(data)?;

        Ok(IceParameters::from_fbs(web_rtc_transport::IceParameters {
            username_fragment: data.username_fragment,
            password: data.password,
            ice_lite: data.ice_lite,
        }))
    }
}

#[derive(Debug)]
pub(crate) struct TransportProduceRequest {
    pub(crate) producer_id: ProducerId,
    pub(crate) kind: MediaKind,
    pub(crate) rtp_parameters: RtpParameters,
    pub(crate) rtp_mapping: RtpMapping,
    pub(crate) key_frame_request_delay: u32,
    pub(crate) paused: bool,
}

#[derive(Debug)]
pub(crate) struct TransportProduceResponse {
    pub(crate) r#type: ProducerType,
}

impl Request for TransportProduceRequest {
    const METHOD: request::Method = request::Method::TransportProduce;
    type HandlerId = TransportId;
    type Response = TransportProduceResponse;

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();
        let data = transport::ProduceRequest::create(
            &mut builder,
            self.producer_id.to_string(),
            self.kind.to_fbs(),
            Box::new(self.rtp_parameters.into_fbs()),
            Box::new(self.rtp_mapping.to_fbs()),
            self.key_frame_request_delay,
            self.paused,
        );
        let request_body = request::Body::create_transport_produce_request(&mut builder, data);
        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        let Some(response::BodyRef::TransportProduceResponse(data)) = response else {
            panic!("Wrong message from worker: {response:?}");
        };

        let data = transport::ProduceResponse::try_from(data)?;

        Ok(TransportProduceResponse {
            r#type: ProducerType::from_fbs(data.type_),
        })
    }
}

#[derive(Debug)]
pub(crate) struct TransportConsumeRequest {
    pub(crate) consumer_id: ConsumerId,
    pub(crate) producer_id: ProducerId,
    pub(crate) kind: MediaKind,
    pub(crate) rtp_parameters: RtpParameters,
    pub(crate) r#type: ConsumerType,
    pub(crate) consumable_rtp_encodings: Vec<RtpEncodingParameters>,
    pub(crate) paused: bool,
    pub(crate) preferred_layers: Option<ConsumerLayers>,
    pub(crate) ignore_dtx: bool,
}

#[derive(Debug)]
pub(crate) struct TransportConsumeResponse {
    pub(crate) paused: bool,
    pub(crate) producer_paused: bool,
    pub(crate) score: ConsumerScore,
    pub(crate) preferred_layers: Option<ConsumerLayers>,
}

impl Request for TransportConsumeRequest {
    const METHOD: request::Method = request::Method::TransportConsume;
    type HandlerId = TransportId;
    type Response = TransportConsumeResponse;

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();
        let data = transport::ConsumeRequest::create(
            &mut builder,
            self.consumer_id.to_string(),
            self.producer_id.to_string(),
            self.kind.to_fbs(),
            Box::new(self.rtp_parameters.into_fbs()),
            self.r#type.to_fbs(),
            self.consumable_rtp_encodings
                .iter()
                .map(RtpEncodingParameters::to_fbs)
                .collect::<Vec<_>>(),
            self.paused,
            self.preferred_layers.map(ConsumerLayers::to_fbs),
            self.ignore_dtx,
        );
        let request_body = request::Body::create_transport_consume_request(&mut builder, data);
        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        let Some(response::BodyRef::TransportConsumeResponse(data)) = response else {
            panic!("Wrong message from worker: {response:?}");
        };

        let data = transport::ConsumeResponse::try_from(data)?;

        Ok(TransportConsumeResponse {
            paused: data.paused,
            producer_paused: data.producer_paused,
            score: ConsumerScore::from_fbs(*data.score),
            preferred_layers: data
                .preferred_layers
                .map(|preferred_layers| ConsumerLayers::from_fbs(*preferred_layers)),
        })
    }
}

#[derive(Debug)]
pub(crate) struct TransportProduceDataRequest {
    pub(crate) data_producer_id: DataProducerId,
    pub(crate) r#type: DataProducerType,
    // #[serde(skip_serializing_if = "Option::is_none")]
    pub(crate) sctp_stream_parameters: Option<SctpStreamParameters>,
    pub(crate) label: String,
    pub(crate) protocol: String,
    pub(crate) paused: bool,
}

#[derive(Debug)]
pub(crate) struct TransportProduceDataResponse {
    pub(crate) r#type: DataProducerType,
    pub(crate) sctp_stream_parameters: Option<SctpStreamParameters>,
    pub(crate) label: String,
    pub(crate) protocol: String,
    pub(crate) paused: bool,
}

impl Request for TransportProduceDataRequest {
    const METHOD: request::Method = request::Method::TransportProduceData;
    type HandlerId = TransportId;
    type Response = TransportProduceDataResponse;

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();
        let data = transport::ProduceDataRequest::create(
            &mut builder,
            self.data_producer_id.to_string(),
            match self.r#type {
                DataProducerType::Sctp => data_producer::Type::Sctp,
                DataProducerType::Direct => data_producer::Type::Direct,
            },
            self.sctp_stream_parameters
                .map(SctpStreamParameters::to_fbs),
            if self.label.is_empty() {
                None
            } else {
                Some(self.label)
            },
            if self.protocol.is_empty() {
                None
            } else {
                Some(self.protocol)
            },
            self.paused,
        );
        let request_body = request::Body::create_transport_produce_data_request(&mut builder, data);
        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        let Some(response::BodyRef::DataProducerDumpResponse(data)) = response else {
            panic!("Wrong message from worker: {response:?}");
        };

        let data = data_producer::DumpResponse::try_from(data)?;

        Ok(TransportProduceDataResponse {
            r#type: match data.type_ {
                data_producer::Type::Sctp => DataProducerType::Sctp,
                data_producer::Type::Direct => DataProducerType::Direct,
            },
            sctp_stream_parameters: data
                .sctp_stream_parameters
                .map(|stream_parameters| SctpStreamParameters::from_fbs(*stream_parameters)),
            label: data.label.to_string(),
            protocol: data.protocol.to_string(),
            paused: data.paused,
        })
    }
}

#[derive(Debug)]
pub(crate) struct TransportConsumeDataRequest {
    pub(crate) data_consumer_id: DataConsumerId,
    pub(crate) data_producer_id: DataProducerId,
    pub(crate) r#type: DataConsumerType,
    pub(crate) sctp_stream_parameters: Option<SctpStreamParameters>,
    pub(crate) label: String,
    pub(crate) protocol: String,
    pub(crate) paused: bool,
    pub(crate) subchannels: Option<Vec<u16>>,
}

#[derive(Debug)]
pub(crate) struct TransportConsumeDataResponse {
    pub(crate) r#type: DataConsumerType,
    pub(crate) sctp_stream_parameters: Option<SctpStreamParameters>,
    pub(crate) label: String,
    pub(crate) protocol: String,
    pub(crate) paused: bool,
    pub(crate) data_producer_paused: bool,
    pub(crate) subchannels: Vec<u16>,
}

impl Request for TransportConsumeDataRequest {
    const METHOD: request::Method = request::Method::TransportConsumeData;
    type HandlerId = TransportId;
    type Response = TransportConsumeDataResponse;

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();
        let data = transport::ConsumeDataRequest::create(
            &mut builder,
            self.data_consumer_id.to_string(),
            self.data_producer_id.to_string(),
            match self.r#type {
                DataConsumerType::Sctp => data_producer::Type::Sctp,
                DataConsumerType::Direct => data_producer::Type::Direct,
            },
            self.sctp_stream_parameters
                .map(SctpStreamParameters::to_fbs),
            if self.label.is_empty() {
                None
            } else {
                Some(self.label)
            },
            if self.protocol.is_empty() {
                None
            } else {
                Some(self.protocol)
            },
            self.paused,
            self.subchannels,
        );
        let request_body = request::Body::create_transport_consume_data_request(&mut builder, data);
        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        let Some(response::BodyRef::DataConsumerDumpResponse(data)) = response else {
            panic!("Wrong message from worker: {response:?}");
        };

        let data = data_consumer::DumpResponse::try_from(data)?;

        Ok(TransportConsumeDataResponse {
            r#type: match data.type_ {
                data_producer::Type::Sctp => DataConsumerType::Sctp,
                data_producer::Type::Direct => DataConsumerType::Direct,
            },
            sctp_stream_parameters: data
                .sctp_stream_parameters
                .map(|stream_parameters| SctpStreamParameters::from_fbs(*stream_parameters)),
            label: data.label.to_string(),
            protocol: data.protocol.to_string(),
            paused: data.paused,
            data_producer_paused: data.data_producer_paused,
            subchannels: data.subchannels,
        })
    }
}

#[derive(Debug)]
pub(crate) struct TransportEnableTraceEventRequest {
    pub(crate) types: Vec<TransportTraceEventType>,
}

impl Request for TransportEnableTraceEventRequest {
    const METHOD: request::Method = request::Method::TransportEnableTraceEvent;
    type HandlerId = TransportId;
    type Response = ();

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let data = transport::EnableTraceEventRequest {
            events: self
                .types
                .into_iter()
                .map(TransportTraceEventType::to_fbs)
                .collect(),
        };

        let request_body = request::Body::TransportEnableTraceEventRequest(Box::new(data));
        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        _response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        Ok(())
    }

    fn default_for_soft_error() -> Option<Self::Response> {
        None
    }
}

#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
pub(crate) struct TransportSendRtcpNotification {
    pub(crate) rtcp_packet: Vec<u8>,
}

impl Notification for TransportSendRtcpNotification {
    const EVENT: notification::Event = notification::Event::TransportSendRtcp;
    type HandlerId = TransportId;

    fn into_bytes(self, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let data = transport::SendRtcpNotification::create(&mut builder, self.rtcp_packet);
        let notification_body =
            notification::Body::create_transport_send_rtcp_notification(&mut builder, data);

        let notification = notification::Notification::create(
            &mut builder,
            handler_id.to_string(),
            Self::EVENT,
            Some(notification_body),
        );
        let message_body = message::Body::create_notification(&mut builder, notification);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }
}

#[derive(Debug)]
pub(crate) struct ProducerCloseRequest {
    pub(crate) producer_id: ProducerId,
}

impl Request for ProducerCloseRequest {
    const METHOD: request::Method = request::Method::TransportCloseProducer;
    type HandlerId = TransportId;
    type Response = ();

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();
        let data =
            transport::CloseProducerRequest::create(&mut builder, self.producer_id.to_string());
        let request_body =
            request::Body::create_transport_close_producer_request(&mut builder, data);

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        _response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        Ok(())
    }
}

#[derive(Debug)]
pub(crate) struct ProducerDumpRequest {}

impl Request for ProducerDumpRequest {
    const METHOD: request::Method = request::Method::ProducerDump;
    type HandlerId = ProducerId;
    type Response = ProducerDump;

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            None::<request::Body>,
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        let Some(response::BodyRef::ProducerDumpResponse(data)) = response else {
            panic!("Wrong message from worker: {response:?}");
        };

        ProducerDump::from_fbs_ref(data)
    }
}

#[derive(Debug)]
pub(crate) struct ProducerGetStatsRequest {}

impl Request for ProducerGetStatsRequest {
    const METHOD: request::Method = request::Method::ProducerGetStats;
    type HandlerId = ProducerId;
    type Response = response::Body;

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            None::<request::Body>,
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        match response {
            Some(data) => Ok(data.try_into().unwrap()),
            _ => {
                panic!("Wrong message from worker: {response:?}");
            }
        }
    }
}

#[derive(Debug, Serialize)]
pub(crate) struct ProducerPauseRequest {}

impl Request for ProducerPauseRequest {
    const METHOD: request::Method = request::Method::ProducerPause;
    type HandlerId = ProducerId;
    type Response = ();

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            None::<request::Body>,
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        _response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        Ok(())
    }
}

#[derive(Debug, Serialize)]
pub(crate) struct ProducerResumeRequest {}

impl Request for ProducerResumeRequest {
    const METHOD: request::Method = request::Method::ProducerResume;
    type HandlerId = ProducerId;
    type Response = ();

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            None::<request::Body>,
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        _response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        Ok(())
    }
}

#[derive(Debug)]
pub(crate) struct ProducerEnableTraceEventRequest {
    pub(crate) types: Vec<ProducerTraceEventType>,
}

impl Request for ProducerEnableTraceEventRequest {
    const METHOD: request::Method = request::Method::ProducerEnableTraceEvent;
    type HandlerId = ProducerId;
    type Response = ();

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let data = producer::EnableTraceEventRequest {
            events: self
                .types
                .into_iter()
                .map(ProducerTraceEventType::to_fbs)
                .collect(),
        };

        let request_body = request::Body::ProducerEnableTraceEventRequest(Box::new(data));
        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        _response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        Ok(())
    }

    fn default_for_soft_error() -> Option<Self::Response> {
        None
    }
}

#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
pub(crate) struct ProducerSendNotification {
    pub(crate) rtp_packet: Vec<u8>,
}

impl Notification for ProducerSendNotification {
    const EVENT: notification::Event = notification::Event::ProducerSend;
    type HandlerId = ProducerId;

    fn into_bytes(self, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let data = producer::SendNotification::create(&mut builder, self.rtp_packet);
        let notification_body =
            notification::Body::create_producer_send_notification(&mut builder, data);

        let notification = notification::Notification::create(
            &mut builder,
            handler_id.to_string(),
            Self::EVENT,
            Some(notification_body),
        );
        let message_body = message::Body::create_notification(&mut builder, notification);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }
}

#[derive(Debug)]
pub(crate) struct ConsumerCloseRequest {
    pub(crate) consumer_id: ConsumerId,
}

impl Request for ConsumerCloseRequest {
    const METHOD: request::Method = request::Method::TransportCloseConsumer;
    type HandlerId = TransportId;
    type Response = ();

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();
        let data =
            transport::CloseConsumerRequest::create(&mut builder, self.consumer_id.to_string());
        let request_body =
            request::Body::create_transport_close_consumer_request(&mut builder, data);

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        _response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        Ok(())
    }
}

#[derive(Debug)]
pub(crate) struct ConsumerDumpRequest {}

impl Request for ConsumerDumpRequest {
    const METHOD: request::Method = request::Method::ConsumerDump;
    type HandlerId = ConsumerId;
    type Response = ConsumerDump;

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            None::<request::Body>,
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        let Some(response::BodyRef::ConsumerDumpResponse(data)) = response else {
            panic!("Wrong message from worker: {response:?}");
        };

        ConsumerDump::from_fbs_ref(data)
    }
}

#[derive(Debug)]
pub(crate) struct ConsumerGetStatsRequest {}

impl Request for ConsumerGetStatsRequest {
    const METHOD: request::Method = request::Method::ConsumerGetStats;
    type HandlerId = ConsumerId;
    type Response = response::Body;

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            None::<request::Body>,
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        match response {
            Some(data) => Ok(data.try_into().unwrap()),
            _ => {
                panic!("Wrong message from worker: {response:?}");
            }
        }
    }
}

#[derive(Debug)]
pub(crate) struct ConsumerPauseRequest {}

impl Request for ConsumerPauseRequest {
    const METHOD: request::Method = request::Method::ConsumerPause;
    type HandlerId = ConsumerId;
    type Response = ();

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            None::<request::Body>,
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        _response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        Ok(())
    }
}

#[derive(Debug, Serialize)]
pub(crate) struct ConsumerResumeRequest {}

impl Request for ConsumerResumeRequest {
    const METHOD: request::Method = request::Method::ConsumerResume;
    type HandlerId = ConsumerId;
    type Response = ();

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            None::<request::Body>,
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        _response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        Ok(())
    }
}

#[derive(Debug, Serialize)]
pub(crate) struct ConsumerSetPreferredLayersRequest {
    pub(crate) data: ConsumerLayers,
}

impl Request for ConsumerSetPreferredLayersRequest {
    const METHOD: request::Method = request::Method::ConsumerSetPreferredLayers;
    type HandlerId = ConsumerId;
    type Response = Option<ConsumerLayers>;

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let data = consumer::SetPreferredLayersRequest::create(
            &mut builder,
            ConsumerLayers::to_fbs(self.data),
        );
        let request_body =
            request::Body::create_consumer_set_preferred_layers_request(&mut builder, data);

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        let Some(response::BodyRef::ConsumerSetPreferredLayersResponse(data)) = response else {
            panic!("Wrong message from worker: {response:?}");
        };

        let data = consumer::SetPreferredLayersResponse::try_from(data)?;

        match data.preferred_layers {
            Some(preferred_layers) => Ok(Some(ConsumerLayers::from_fbs(*preferred_layers))),
            None => Ok(None),
        }
    }
}

#[derive(Debug, Serialize)]
pub(crate) struct ConsumerSetPriorityRequest {
    pub(crate) priority: u8,
}

#[derive(Debug, Serialize)]
pub(crate) struct ConsumerSetPriorityResponse {
    pub(crate) priority: u8,
}

impl Request for ConsumerSetPriorityRequest {
    const METHOD: request::Method = request::Method::ConsumerSetPriority;
    type HandlerId = ConsumerId;
    type Response = ConsumerSetPriorityResponse;

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let data = consumer::SetPriorityRequest::create(&mut builder, self.priority);
        let request_body = request::Body::create_consumer_set_priority_request(&mut builder, data);

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        let Some(response::BodyRef::ConsumerSetPriorityResponse(data)) = response else {
            panic!("Wrong message from worker: {response:?}");
        };

        let data = consumer::SetPriorityResponse::try_from(data)?;

        Ok(ConsumerSetPriorityResponse {
            priority: data.priority,
        })
    }
}

#[derive(Debug)]
pub(crate) struct ConsumerRequestKeyFrameRequest {}

impl Request for ConsumerRequestKeyFrameRequest {
    const METHOD: request::Method = request::Method::ConsumerRequestKeyFrame;
    type HandlerId = ConsumerId;
    type Response = ();

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();
        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            None::<request::Body>,
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        _response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        Ok(())
    }
}

#[derive(Debug)]
pub(crate) struct ConsumerEnableTraceEventRequest {
    pub(crate) types: Vec<ConsumerTraceEventType>,
}

impl Request for ConsumerEnableTraceEventRequest {
    const METHOD: request::Method = request::Method::ConsumerEnableTraceEvent;
    type HandlerId = ConsumerId;
    type Response = ();

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let data = consumer::EnableTraceEventRequest {
            events: self
                .types
                .into_iter()
                .map(ConsumerTraceEventType::to_fbs)
                .collect(),
        };

        let request_body = request::Body::ConsumerEnableTraceEventRequest(Box::new(data));
        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        _response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        Ok(())
    }

    fn default_for_soft_error() -> Option<Self::Response> {
        None
    }
}

#[derive(Debug)]
pub(crate) struct DataProducerCloseRequest {
    pub(crate) data_producer_id: DataProducerId,
}

impl Request for DataProducerCloseRequest {
    const METHOD: request::Method = request::Method::TransportCloseDataproducer;
    type HandlerId = TransportId;
    type Response = ();

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();
        let data = transport::CloseDataProducerRequest::create(
            &mut builder,
            self.data_producer_id.to_string(),
        );
        let request_body =
            request::Body::create_transport_close_data_producer_request(&mut builder, data);

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        _response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        Ok(())
    }
}

#[derive(Debug)]
pub(crate) struct DataProducerDumpRequest {}

impl Request for DataProducerDumpRequest {
    const METHOD: request::Method = request::Method::DataproducerDump;
    type HandlerId = DataProducerId;
    type Response = response::Body;

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            None::<request::Body>,
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        match response {
            Some(data) => Ok(data.try_into().unwrap()),
            _ => {
                panic!("Wrong message from worker: {response:?}");
            }
        }
    }
}

#[derive(Debug)]
pub(crate) struct DataProducerGetStatsRequest {}

impl Request for DataProducerGetStatsRequest {
    const METHOD: request::Method = request::Method::DataproducerGetStats;
    type HandlerId = DataProducerId;
    type Response = response::Body;

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            None::<request::Body>,
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        match response {
            Some(data) => Ok(data.try_into().unwrap()),
            _ => {
                panic!("Wrong message from worker: {response:?}");
            }
        }
    }
}

#[derive(Debug, Serialize)]
pub(crate) struct DataProducerPauseRequest {}

impl Request for DataProducerPauseRequest {
    const METHOD: request::Method = request::Method::DataproducerPause;
    type HandlerId = DataProducerId;
    type Response = ();

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            None::<request::Body>,
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        _response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        Ok(())
    }
}

#[derive(Debug, Serialize)]
pub(crate) struct DataProducerResumeRequest {}

impl Request for DataProducerResumeRequest {
    const METHOD: request::Method = request::Method::DataproducerResume;
    type HandlerId = DataProducerId;
    type Response = ();

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            None::<request::Body>,
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        _response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        Ok(())
    }
}

#[derive(Debug)]
pub(crate) struct DataProducerSendNotification {
    pub(crate) ppid: u32,
    pub(crate) payload: Vec<u8>,
    pub(crate) subchannels: Option<Vec<u16>>,
    pub(crate) required_subchannel: Option<u16>,
}

impl Notification for DataProducerSendNotification {
    const EVENT: notification::Event = notification::Event::DataproducerSend;
    type HandlerId = DataProducerId;

    fn into_bytes(self, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let data = data_producer::SendNotification::create(
            &mut builder,
            self.ppid,
            self.payload,
            self.subchannels,
            self.required_subchannel,
        );
        let notification_body =
            notification::Body::create_data_producer_send_notification(&mut builder, data);

        let notification = notification::Notification::create(
            &mut builder,
            handler_id.to_string(),
            Self::EVENT,
            Some(notification_body),
        );
        let message_body = message::Body::create_notification(&mut builder, notification);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }
}

#[derive(Debug)]
pub(crate) struct DataConsumerCloseRequest {
    pub(crate) data_consumer_id: DataConsumerId,
}

impl Request for DataConsumerCloseRequest {
    const METHOD: request::Method = request::Method::TransportCloseDataconsumer;
    type HandlerId = TransportId;
    type Response = ();

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();
        let data = transport::CloseDataConsumerRequest::create(
            &mut builder,
            self.data_consumer_id.to_string(),
        );
        let request_body =
            request::Body::create_transport_close_data_consumer_request(&mut builder, data);

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        _response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        Ok(())
    }
}

#[derive(Debug)]
pub(crate) struct DataConsumerDumpRequest {}

impl Request for DataConsumerDumpRequest {
    const METHOD: request::Method = request::Method::DataconsumerDump;
    type HandlerId = DataConsumerId;
    type Response = response::Body;

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            None::<request::Body>,
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        match response {
            Some(data) => Ok(data.try_into().unwrap()),
            _ => {
                panic!("Wrong message from worker: {response:?}");
            }
        }
    }
}

#[derive(Debug)]
pub(crate) struct DataConsumerGetStatsRequest {}

impl Request for DataConsumerGetStatsRequest {
    const METHOD: request::Method = request::Method::DataconsumerGetStats;
    type HandlerId = DataConsumerId;
    type Response = response::Body;

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            None::<request::Body>,
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        match response {
            Some(data) => Ok(data.try_into().unwrap()),
            _ => {
                panic!("Wrong message from worker: {response:?}");
            }
        }
    }
}

#[derive(Debug, Serialize)]
pub(crate) struct DataConsumerPauseRequest {}

impl Request for DataConsumerPauseRequest {
    const METHOD: request::Method = request::Method::DataconsumerPause;
    type HandlerId = DataConsumerId;
    type Response = ();

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            None::<request::Body>,
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        _response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        Ok(())
    }
}

#[derive(Debug, Serialize)]
pub(crate) struct DataConsumerResumeRequest {}

impl Request for DataConsumerResumeRequest {
    const METHOD: request::Method = request::Method::DataconsumerResume;
    type HandlerId = DataConsumerId;
    type Response = ();

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            None::<request::Body>,
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        _response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        Ok(())
    }
}

#[derive(Debug, Serialize)]
pub(crate) struct DataConsumerGetBufferedAmountRequest {}

#[derive(Debug, Serialize)]
pub(crate) struct DataConsumerGetBufferedAmountResponse {
    pub(crate) buffered_amount: u32,
}

impl Request for DataConsumerGetBufferedAmountRequest {
    const METHOD: request::Method = request::Method::DataconsumerGetBufferedAmount;
    type HandlerId = DataConsumerId;
    type Response = DataConsumerGetBufferedAmountResponse;

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            None::<request::Body>,
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        let Some(response::BodyRef::DataConsumerGetBufferedAmountResponse(data)) = response else {
            panic!("Wrong message from worker: {response:?}");
        };

        let data = data_consumer::GetBufferedAmountResponse::try_from(data)?;

        Ok(DataConsumerGetBufferedAmountResponse {
            buffered_amount: data.buffered_amount,
        })
    }
}

#[derive(Debug, Serialize)]
pub(crate) struct DataConsumerSetBufferedAmountLowThresholdRequest {
    pub(crate) threshold: u32,
}

impl Request for DataConsumerSetBufferedAmountLowThresholdRequest {
    const METHOD: request::Method = request::Method::DataconsumerSetBufferedAmountLowThreshold;
    type HandlerId = DataConsumerId;
    type Response = ();

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let data = data_consumer::SetBufferedAmountLowThresholdRequest::create(
            &mut builder,
            self.threshold,
        );
        let request_body =
            request::Body::create_data_consumer_set_buffered_amount_low_threshold_request(
                &mut builder,
                data,
            );

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        _response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        Ok(())
    }
}

#[derive(Debug, Clone, Serialize)]
#[serde(into = "u32")]
pub(crate) struct DataConsumerSendRequest {
    pub(crate) ppid: u32,
    pub(crate) payload: Vec<u8>,
}

impl Request for DataConsumerSendRequest {
    const METHOD: request::Method = request::Method::DataconsumerSend;
    type HandlerId = DataConsumerId;
    type Response = ();

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let data = data_consumer::SendRequest::create(&mut builder, self.ppid, self.payload);
        let request_body = request::Body::create_data_consumer_send_request(&mut builder, data);

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        _response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        Ok(())
    }
}

impl From<DataConsumerSendRequest> for u32 {
    fn from(request: DataConsumerSendRequest) -> Self {
        request.ppid
    }
}

#[derive(Debug, Clone, Serialize)]
pub(crate) struct DataConsumerSetSubchannelsRequest {
    pub(crate) subchannels: Vec<u16>,
}

#[derive(Debug, Clone, Serialize)]
pub(crate) struct DataConsumerSetSubchannelsResponse {
    pub(crate) subchannels: Vec<u16>,
}

impl Request for DataConsumerSetSubchannelsRequest {
    const METHOD: request::Method = request::Method::DataconsumerSetSubchannels;
    type HandlerId = DataConsumerId;
    type Response = DataConsumerSetSubchannelsResponse;

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let data = data_consumer::SetSubchannelsRequest::create(&mut builder, self.subchannels);
        let request_body =
            request::Body::create_data_consumer_set_subchannels_request(&mut builder, data);

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        let Some(response::BodyRef::DataConsumerSetSubchannelsResponse(data)) = response else {
            panic!("Wrong message from worker: {response:?}");
        };

        let data = data_consumer::SetSubchannelsResponse::try_from(data)?;

        Ok(DataConsumerSetSubchannelsResponse {
            subchannels: data.subchannels,
        })
    }
}

#[derive(Debug, Clone, Serialize)]
pub(crate) struct DataConsumerAddSubchannelRequest {
    pub(crate) subchannel: u16,
}

#[derive(Debug, Clone, Serialize)]
pub(crate) struct DataConsumerAddSubchannelResponse {
    pub(crate) subchannels: Vec<u16>,
}

impl Request for DataConsumerAddSubchannelRequest {
    const METHOD: request::Method = request::Method::DataconsumerAddSubchannel;
    type HandlerId = DataConsumerId;
    type Response = DataConsumerAddSubchannelResponse;

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let data = data_consumer::AddSubchannelRequest::create(&mut builder, self.subchannel);
        let request_body =
            request::Body::create_data_consumer_add_subchannel_request(&mut builder, data);

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        let Some(response::BodyRef::DataConsumerAddSubchannelResponse(data)) = response else {
            panic!("Wrong message from worker: {response:?}");
        };

        let data = data_consumer::AddSubchannelResponse::try_from(data)?;

        Ok(DataConsumerAddSubchannelResponse {
            subchannels: data.subchannels,
        })
    }
}

#[derive(Debug, Clone, Serialize)]
pub(crate) struct DataConsumerRemoveSubchannelRequest {
    pub(crate) subchannel: u16,
}

#[derive(Debug, Clone, Serialize)]
pub(crate) struct DataConsumerRemoveSubchannelResponse {
    pub(crate) subchannels: Vec<u16>,
}

impl Request for DataConsumerRemoveSubchannelRequest {
    const METHOD: request::Method = request::Method::DataconsumerRemoveSubchannel;
    type HandlerId = DataConsumerId;
    type Response = DataConsumerRemoveSubchannelResponse;

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let data = data_consumer::RemoveSubchannelRequest::create(&mut builder, self.subchannel);
        let request_body =
            request::Body::create_data_consumer_remove_subchannel_request(&mut builder, data);

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        let Some(response::BodyRef::DataConsumerRemoveSubchannelResponse(data)) = response else {
            panic!("Wrong message from worker: {response:?}");
        };

        let data = data_consumer::RemoveSubchannelResponse::try_from(data)?;

        Ok(DataConsumerRemoveSubchannelResponse {
            subchannels: data.subchannels,
        })
    }
}

#[derive(Debug)]
pub(crate) struct RtpObserverCloseRequest {
    pub(crate) rtp_observer_id: RtpObserverId,
}

impl Request for RtpObserverCloseRequest {
    const METHOD: request::Method = request::Method::RouterCloseRtpobserver;
    type HandlerId = RouterId;
    type Response = ();

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();
        let data =
            router::CloseRtpObserverRequest::create(&mut builder, self.rtp_observer_id.to_string());
        let request_body =
            request::Body::create_router_close_rtp_observer_request(&mut builder, data);

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        _response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        Ok(())
    }
}

#[derive(Debug)]
pub(crate) struct RtpObserverPauseRequest {}

impl Request for RtpObserverPauseRequest {
    const METHOD: request::Method = request::Method::RtpobserverPause;
    type HandlerId = RtpObserverId;
    type Response = ();

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();
        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            None::<request::Body>,
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        _response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        Ok(())
    }
}

#[derive(Debug)]
pub(crate) struct RtpObserverResumeRequest {}

impl Request for RtpObserverResumeRequest {
    const METHOD: request::Method = request::Method::RtpobserverResume;
    type HandlerId = RtpObserverId;
    type Response = ();

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();
        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            None::<request::Body>,
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        _response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        Ok(())
    }
}

#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
pub(crate) struct RtpObserverAddProducerRequest {
    pub(crate) producer_id: ProducerId,
}

impl Request for RtpObserverAddProducerRequest {
    const METHOD: request::Method = request::Method::RtpobserverAddProducer;
    type HandlerId = RtpObserverId;
    type Response = ();

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let data =
            rtp_observer::AddProducerRequest::create(&mut builder, self.producer_id.to_string());
        let request_body =
            request::Body::create_rtp_observer_add_producer_request(&mut builder, data);

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        _response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        Ok(())
    }
}

#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
pub(crate) struct RtpObserverRemoveProducerRequest {
    pub(crate) producer_id: ProducerId,
}

impl Request for RtpObserverRemoveProducerRequest {
    const METHOD: request::Method = request::Method::RtpobserverRemoveProducer;
    type HandlerId = RtpObserverId;
    type Response = ();

    fn into_bytes(self, id: u32, handler_id: Self::HandlerId) -> Vec<u8> {
        let mut builder = Builder::new();

        let data =
            rtp_observer::RemoveProducerRequest::create(&mut builder, self.producer_id.to_string());
        let request_body =
            request::Body::create_rtp_observer_remove_producer_request(&mut builder, data);

        let request = request::Request::create(
            &mut builder,
            id,
            Self::METHOD,
            handler_id.to_string(),
            Some(request_body),
        );
        let message_body = message::Body::create_request(&mut builder, request);
        let message = message::Message::create(&mut builder, message_body);

        builder.finish(message, None).to_vec()
    }

    fn convert_response(
        _response: Option<response::BodyRef<'_>>,
    ) -> Result<Self::Response, Box<dyn Error + Send + Sync>> {
        Ok(())
    }
}
