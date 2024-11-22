use actix::prelude::*;
use actix_web::web::{Data, Payload};
use actix_web::{web, App, Error, HttpRequest, HttpResponse, HttpServer};
use actix_web_actors::ws;
use mediasoup::prelude::*;
use mediasoup::rtp_parameters::{RtpCodecParameters, RtpEncodingParameters};
use mediasoup::worker::{WorkerLogLevel, WorkerLogTag};
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::net::{IpAddr, Ipv4Addr};
use std::num::{NonZeroU32, NonZeroU8};

/// List of codecs that SFU will accept from clients
fn media_codecs() -> Vec<RtpCodecCapability> {
    vec![RtpCodecCapability::Audio {
        mime_type: MimeTypeAudio::MultiChannelOpus,
        preferred_payload_type: None,
        clock_rate: NonZeroU32::new(48000).unwrap(),
        channels: NonZeroU8::new(6).unwrap(),
        parameters: RtpCodecParametersParameters::from([
            ("useinbandfec", 1_u32.into()),
            ("channel_mapping", "0,4,1,2,3,5".into()),
            ("num_streams", 4_u32.into()),
            ("coupled_streams", 2_u32.into()),
        ]),
        rtcp_feedback: vec![RtcpFeedback::TransportCc],
    }]
}

/// Data structure containing all the necessary information about transport options required from
/// the server to establish transport connection on the client
#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct TransportOptions {
    id: TransportId,
    dtls_parameters: DtlsParameters,
    ice_candidates: Vec<IceCandidate>,
    ice_parameters: IceParameters,
}

/// Server messages sent to the client
#[derive(Serialize, Message)]
#[serde(tag = "action")]
#[rtype(result = "()")]
enum ServerMessage {
    /// Initialization message with consumer transport options and Router's RTP
    /// capabilities necessary to establish WebRTC transport connection client-side
    #[serde(rename_all = "camelCase")]
    Init {
        consumer_transport_options: TransportOptions,
        router_rtp_capabilities: RtpCapabilitiesFinalized,
        rtp_producer_id: ProducerId,
    },
    /// Notification that consumer transport was connected successfully (in case of error connection
    /// is just dropped, in real-world application you probably want to handle it better)
    ConnectedConsumerTransport,
    /// Notification that consumer was successfully created server-side, client can resume the
    /// consumer after this
    #[serde(rename_all = "camelCase")]
    Consumed {
        id: ConsumerId,
        producer_id: ProducerId,
        kind: MediaKind,
        rtp_parameters: RtpParameters,
    },
}

/// Client messages sent to the server
#[derive(Deserialize, Message)]
#[serde(tag = "action")]
#[rtype(result = "()")]
enum ClientMessage {
    /// Client-side initialization with its RTP capabilities, in this simple case we expect those to
    /// match server Router's RTP capabilities
    #[serde(rename_all = "camelCase")]
    Init { rtp_capabilities: RtpCapabilities },
    /// Request to connect consumer transport with client-side DTLS parameters
    #[serde(rename_all = "camelCase")]
    ConnectConsumerTransport { dtls_parameters: DtlsParameters },
    /// Request to consume specified producer
    #[serde(rename_all = "camelCase")]
    Consume { producer_id: ProducerId },
}

/// Internal actor messages for convenience
#[derive(Message)]
#[rtype(result = "()")]
enum InternalMessage {
    /// Save consumer in connection-specific hashmap to prevent it from being destroyed
    SaveConsumer(Consumer),
    /// Stop/close the WebSocket connection
    Stop,
}

/// Actor that will represent WebSocket connection from the client, it will handle inbound and
/// outbound WebSocket messages in JSON.
///
/// See https://actix.rs/docs/websockets/ for official `actix-web` documentation.
struct EchoConnection {
    /// RTP capabilities received from the client
    client_rtp_capabilities: Option<RtpCapabilities>,
    /// Consumers associated with this client, preventing them from being destroyed
    consumers: HashMap<ConsumerId, Consumer>,
    /// RTP producer associated with this client, preventing it from being destroyed and useful to
    /// get its ID later
    rtp_producer: Producer,
    /// Router associated with this client, useful to get its RTP capabilities later
    router: Router,
    /// Consumer transport associated with this client
    consumer_transport: WebRtcTransport,
}

impl EchoConnection {
    /// Create a new instance representing WebSocket connection
    async fn new(worker_manager: &WorkerManager) -> Result<Self, String> {
        let worker = worker_manager
            .create_worker({
                let mut settings = WorkerSettings::default();
                settings.log_level = WorkerLogLevel::Debug;
                settings.log_tags = vec![
                    WorkerLogTag::Info,
                    WorkerLogTag::Ice,
                    WorkerLogTag::Dtls,
                    WorkerLogTag::Rtp,
                    WorkerLogTag::Srtp,
                    WorkerLogTag::Rtcp,
                    WorkerLogTag::Rtx,
                    WorkerLogTag::Bwe,
                    WorkerLogTag::Score,
                    WorkerLogTag::Simulcast,
                    WorkerLogTag::Svc,
                    WorkerLogTag::Sctp,
                    WorkerLogTag::Message,
                ];

                settings
            })
            .await
            .map_err(|error| format!("Failed to create worker: {error}"))?;
        let router = worker
            .create_router(RouterOptions::new(media_codecs()))
            .await
            .map_err(|error| format!("Failed to create router: {error}"))?;

        // For simplicity we will create plain transport for audio producer right away
        let plain_transport = router
            .create_plain_transport({
                let mut options = PlainTransportOptions::new(ListenInfo {
                    protocol: Protocol::Udp,
                    ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                    announced_address: None,
                    port: None,
                    port_range: None,
                    flags: None,
                    send_buffer_size: None,
                    recv_buffer_size: None,
                });

                options.comedia = true;
                options.rtcp_mux = false;

                options
            })
            .await
            .map_err(|error| format!("Failed to create plain transport: {error}"))?;

        // And creating audio producer that will be consumed over WebRTC later
        let rtp_producer = plain_transport
            .produce(ProducerOptions::new(
                MediaKind::Audio,
                RtpParameters {
                    mid: Some("AUDIO".to_string()),
                    codecs: vec![RtpCodecParameters::Audio {
                        mime_type: MimeTypeAudio::MultiChannelOpus,
                        payload_type: 100,
                        clock_rate: NonZeroU32::new(48000).unwrap(),
                        channels: NonZeroU8::new(6).unwrap(),
                        parameters: RtpCodecParametersParameters::from([
                            ("useinbandfec", 1_u32.into()),
                            ("channel_mapping", "0,1,4,5,2,3".into()),
                            ("num_streams", 4_u32.into()),
                            ("coupled_streams", 2_u32.into()),
                        ]),
                        rtcp_feedback: vec![],
                    }],
                    encodings: vec![RtpEncodingParameters {
                        ssrc: Some(1111),
                        ..RtpEncodingParameters::default()
                    }],
                    ..RtpParameters::default()
                },
            ))
            .await
            .map_err(|error| format!("Failed to create audio producer: {error}"))?;

        println!(
            "Plain transport created:\n  \
            RTP listening on {}:{}\n  \
            RTCP listening on {}:{}\n  \
            PT=100\n  \
            SSRC=1111",
            plain_transport.tuple().local_address(),
            plain_transport.tuple().local_port(),
            plain_transport.rtcp_tuple().unwrap().local_address(),
            plain_transport.rtcp_tuple().unwrap().local_port(),
        );

        println!(
            "Use following command with GStreamer (1.20+) to play a sample audio:\n\
              gst-launch-1.0 \\\n  \
                rtpbin name=rtpbin \\\n  \
                souphttpsrc location=https://www2.iis.fraunhofer.de/AAC/ChID-BLITS-EBU-Narration.mp4 ! \\\n  \
                queue ! \\\n  \
                decodebin ! \\\n  \
                audioresample ! \\\n  \
                audioconvert ! \\\n  \
                opusenc inband-fec=true ! \\\n  \
                queue ! \\\n  \
                clocksync ! \\\n  \
                rtpopuspay pt=100 ssrc=1111 ! \\\n  \
                rtpbin.send_rtp_sink_0 \\\n  \
                rtpbin.send_rtp_src_0 ! udpsink host={} port={} sync=false async=false \\\n  \
                rtpbin.send_rtcp_src_0 ! udpsink host={} port={} sync=false async=false",
                plain_transport.tuple().local_address(),
                plain_transport.tuple().local_port(),
                plain_transport.rtcp_tuple().unwrap().local_address(),
                plain_transport.rtcp_tuple().unwrap().local_port(),
        );

        // We know that for multiopus example we'll need just consumer transport, so we can create
        // it right away. This may not be the case for real-world applications or you may create
        // this at a different time and/or in different order.
        let consumer_transport = router
            .create_webrtc_transport(WebRtcTransportOptions::new(
                WebRtcTransportListenInfos::new(ListenInfo {
                    protocol: Protocol::Udp,
                    ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                    announced_address: None,
                    port: None,
                    port_range: None,
                    flags: None,
                    send_buffer_size: None,
                    recv_buffer_size: None,
                }),
            ))
            .await
            .map_err(|error| format!("Failed to create consumer transport: {error}"))?;

        Ok(Self {
            client_rtp_capabilities: None,
            consumers: HashMap::new(),
            rtp_producer,
            router,
            consumer_transport,
        })
    }
}

impl Actor for EchoConnection {
    type Context = ws::WebsocketContext<Self>;

    fn started(&mut self, ctx: &mut Self::Context) {
        println!("WebSocket connection created");

        // We know that only consumer transport will be used, so we sent server information about it
        // in an initialization message alongside with router capabilities to the client right after
        // WebSocket connection is established
        let server_init_message = ServerMessage::Init {
            consumer_transport_options: TransportOptions {
                id: self.consumer_transport.id(),
                dtls_parameters: self.consumer_transport.dtls_parameters(),
                ice_candidates: self.consumer_transport.ice_candidates().clone(),
                ice_parameters: self.consumer_transport.ice_parameters().clone(),
            },
            router_rtp_capabilities: self.router.rtp_capabilities().clone(),
            rtp_producer_id: self.rtp_producer.id(),
        };

        ctx.address().do_send(server_init_message);
    }

    fn stopped(&mut self, _ctx: &mut Self::Context) {
        println!("WebSocket connection closed");
    }
}

impl StreamHandler<Result<ws::Message, ws::ProtocolError>> for EchoConnection {
    fn handle(&mut self, msg: Result<ws::Message, ws::ProtocolError>, ctx: &mut Self::Context) {
        // Here we handle incoming WebSocket messages, intentionally not handling continuation
        // messages since we know all messages will fit into a single frame, but in real-world apps
        // you need to handle continuation frames too (`ws::Message::Continuation`)
        match msg {
            Ok(ws::Message::Ping(msg)) => {
                ctx.pong(&msg);
            }
            Ok(ws::Message::Pong(_)) => {}
            Ok(ws::Message::Text(text)) => match serde_json::from_str::<ClientMessage>(&text) {
                Ok(message) => {
                    // Parse JSON into an enum and just send it back to the actor to be processed
                    // by another handler below, it is much more convenient to just parse it in one
                    // place and have typed data structure everywhere else
                    ctx.address().do_send(message);
                }
                Err(error) => {
                    eprintln!("Failed to parse client message: {error}\n{text}");
                }
            },
            Ok(ws::Message::Binary(bin)) => {
                eprintln!("Unexpected binary message: {bin:?}");
            }
            Ok(ws::Message::Close(reason)) => {
                ctx.close(reason);
                ctx.stop();
            }
            _ => ctx.stop(),
        }
    }
}

impl Handler<ClientMessage> for EchoConnection {
    type Result = ();

    fn handle(&mut self, message: ClientMessage, ctx: &mut Self::Context) {
        match message {
            ClientMessage::Init { rtp_capabilities } => {
                // We need to know client's RTP capabilities, those are sent using initialization
                // message and are stored in connection struct for future use
                self.client_rtp_capabilities.replace(rtp_capabilities);
            }
            ClientMessage::ConnectConsumerTransport { dtls_parameters } => {
                let address = ctx.address();
                let transport = self.consumer_transport.clone();
                // The same as producer transport, but for consumer transport
                actix::spawn(async move {
                    match transport
                        .connect(WebRtcTransportRemoteParameters { dtls_parameters })
                        .await
                    {
                        Ok(_) => {
                            address.do_send(ServerMessage::ConnectedConsumerTransport);
                            println!("Consumer transport connected");
                        }
                        Err(error) => {
                            eprintln!("Failed to connect consumer transport: {error}");
                            address.do_send(InternalMessage::Stop);
                        }
                    }
                });
            }
            ClientMessage::Consume { producer_id } => {
                let address = ctx.address();
                let transport = self.consumer_transport.clone();
                let rtp_capabilities = match self.client_rtp_capabilities.clone() {
                    Some(rtp_capabilities) => rtp_capabilities,
                    None => {
                        eprintln!("Client should send RTP capabilities before consuming");
                        return;
                    }
                };
                // Create consumer for given producer ID, while first making sure that RTP
                // capabilities were sent by the client prior to that
                actix::spawn(async move {
                    match transport
                        .consume(ConsumerOptions::new(producer_id, rtp_capabilities))
                        .await
                    {
                        Ok(consumer) => {
                            let id = consumer.id();
                            let kind = consumer.kind();
                            let rtp_parameters = consumer.rtp_parameters().clone();
                            address.do_send(ServerMessage::Consumed {
                                id,
                                producer_id,
                                kind,
                                rtp_parameters,
                            });
                            // Consumer is stored in a hashmap since if we don't do it, it will get
                            // destroyed as soon as its instance goes out out scope
                            address.do_send(InternalMessage::SaveConsumer(consumer));
                            println!("{kind:?} consumer created: {id}");
                        }
                        Err(error) => {
                            eprintln!("Failed to create consumer: {error}");
                            address.do_send(InternalMessage::Stop);
                        }
                    }
                });
            }
        }
    }
}

/// Simple handler that will transform typed server messages into JSON and send them over to the
/// client over WebSocket connection
impl Handler<ServerMessage> for EchoConnection {
    type Result = ();

    fn handle(&mut self, message: ServerMessage, ctx: &mut Self::Context) {
        ctx.text(serde_json::to_string(&message).unwrap());
    }
}

/// Convenience handler for internal messages, these actions require mutable access to the
/// connection struct and having such message handler makes it easy to use from background tasks
/// where otherwise Mutex would have to be used instead
impl Handler<InternalMessage> for EchoConnection {
    type Result = ();

    fn handle(&mut self, message: InternalMessage, ctx: &mut Self::Context) {
        match message {
            InternalMessage::Stop => {
                ctx.stop();
            }
            InternalMessage::SaveConsumer(consumer) => {
                self.consumers.insert(consumer.id(), consumer);
            }
        }
    }
}

/// Function that receives HTTP request on WebSocket route and upgrades it to WebSocket connection.
///
/// See https://actix.rs/docs/websockets/ for official `actix-web` documentation.
async fn ws_index(
    request: HttpRequest,
    worker_manager: Data<WorkerManager>,
    stream: Payload,
) -> Result<HttpResponse, Error> {
    match EchoConnection::new(&worker_manager).await {
        Ok(echo_server) => ws::start(echo_server, &request, stream),
        Err(error) => {
            eprintln!("{error}");

            Ok(HttpResponse::InternalServerError().finish())
        }
    }
}

#[actix_web::main]
async fn main() -> std::io::Result<()> {
    env_logger::init();

    // We will reuse the same worker manager across all connections, this is more than enough for
    // this use case
    let worker_manager = Data::new(WorkerManager::new());
    HttpServer::new(move || {
        App::new()
            .app_data(worker_manager.clone())
            .route("/ws", web::get().to(ws_index))
    })
    // 2 threads is plenty for this example, default is to have as many threads as CPU cores
    .workers(2)
    .bind("127.0.0.1:3000")?
    .run()
    .await
}
