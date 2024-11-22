use actix::prelude::*;
use actix_web::web::{Data, Payload};
use actix_web::{web, App, Error, HttpRequest, HttpResponse, HttpServer};
use actix_web_actors::ws;
use mediasoup::prelude::*;
use mediasoup::worker::{WorkerLogLevel, WorkerLogTag};
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::net::{IpAddr, Ipv4Addr};
use std::num::{NonZeroU32, NonZeroU8};

/// List of codecs that SFU will accept from clients
fn media_codecs() -> Vec<RtpCodecCapability> {
    vec![
        RtpCodecCapability::Audio {
            mime_type: MimeTypeAudio::Opus,
            preferred_payload_type: None,
            clock_rate: NonZeroU32::new(48000).unwrap(),
            channels: NonZeroU8::new(2).unwrap(),
            parameters: RtpCodecParametersParameters::from([("useinbandfec", 1_u32.into())]),
            rtcp_feedback: vec![RtcpFeedback::TransportCc],
        },
        RtpCodecCapability::Video {
            mime_type: MimeTypeVideo::Vp8,
            preferred_payload_type: None,
            clock_rate: NonZeroU32::new(90000).unwrap(),
            parameters: RtpCodecParametersParameters::default(),
            rtcp_feedback: vec![
                RtcpFeedback::Nack,
                RtcpFeedback::NackPli,
                RtcpFeedback::CcmFir,
                RtcpFeedback::GoogRemb,
                RtcpFeedback::TransportCc,
            ],
        },
        RtpCodecCapability::Video {
            mime_type: MimeTypeVideo::Vp9,
            preferred_payload_type: None,
            clock_rate: NonZeroU32::new(90000).unwrap(),
            parameters: RtpCodecParametersParameters::default(),
            rtcp_feedback: vec![
                RtcpFeedback::Nack,
                RtcpFeedback::NackPli,
                RtcpFeedback::CcmFir,
                RtcpFeedback::GoogRemb,
                RtcpFeedback::TransportCc,
            ],
        },
    ]
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
#[allow(clippy::large_enum_variant)]
enum ServerMessage {
    /// Initialization message with consumer/producer transport options and Router's RTP
    /// capabilities necessary to establish WebRTC transport connection client-side
    #[serde(rename_all = "camelCase")]
    Init {
        consumer_transport_options: TransportOptions,
        producer_transport_options: TransportOptions,
        router_rtp_capabilities: RtpCapabilitiesFinalized,
    },
    /// Notification that producer transport was connected successfully (in case of error connection
    /// is just dropped, in real-world application you probably want to handle it better)
    ConnectedProducerTransport,
    /// Notification that producer was created on the server, in this simple example client will try
    /// to consume it right away
    #[serde(rename_all = "camelCase")]
    Produced { id: ProducerId },
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
    /// Request to connect producer transport with client-side DTLS parameters
    #[serde(rename_all = "camelCase")]
    ConnectProducerTransport { dtls_parameters: DtlsParameters },
    /// Request to produce a new audio or video track with specified RTP parameters
    #[serde(rename_all = "camelCase")]
    Produce {
        kind: MediaKind,
        rtp_parameters: RtpParameters,
    },
    /// Request to connect consumer transport with client-side DTLS parameters
    #[serde(rename_all = "camelCase")]
    ConnectConsumerTransport { dtls_parameters: DtlsParameters },
    /// Request to consume specified producer
    #[serde(rename_all = "camelCase")]
    Consume { producer_id: ProducerId },
    /// Request to resume consumer that was previously created
    #[serde(rename_all = "camelCase")]
    ConsumerResume { id: ConsumerId },
    /// Request to set preferred spatial and temporal layers
    #[serde(rename_all = "camelCase")]
    SetConsumerPreferredLayers {
        id: ConsumerId,
        preferred_layers: ConsumerLayers,
    },
}

/// Internal actor messages for convenience
#[derive(Message)]
#[rtype(result = "()")]
enum InternalMessage {
    /// Save producer in connection-specific hashmap to prevent it from being destroyed
    SaveProducer(Producer),
    /// Save consumer in connection-specific hashmap to prevent it from being destroyed
    SaveConsumer(Consumer),
    /// Stop/close the WebSocket connection
    Stop,
}

/// Consumer/producer transports pair for the client
struct Transports {
    consumer: WebRtcTransport,
    producer: WebRtcTransport,
}

/// Actor that will represent WebSocket connection from the client, it will handle inbound and
/// outbound WebSocket messages in JSON.
///
/// See https://actix.rs/docs/websockets/ for official `actix-web` documentation.
struct SvcSimulcastConnection {
    /// RTP capabilities received from the client
    client_rtp_capabilities: Option<RtpCapabilities>,
    /// Consumers associated with this client, preventing them from being destroyed
    consumers: HashMap<ConsumerId, Consumer>,
    /// Producers associated with this client, preventing them from being destroyed
    producers: Vec<Producer>,
    /// Router associated with this client, useful to get its RTP capabilities later
    router: Router,
    /// Consumer and producer transports associated with this client
    transports: Transports,
}

impl SvcSimulcastConnection {
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

        // We know that for svc-simulcast example we'll need 2 transports, so we can create both
        // right away.
        // This may not be the case for real-world applications or you may create this at a
        // different time and/or in different order.
        let transport_options =
            WebRtcTransportOptions::new(WebRtcTransportListenInfos::new(ListenInfo {
                protocol: Protocol::Udp,
                ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                announced_address: None,
                port: None,
                port_range: None,
                flags: None,
                send_buffer_size: None,
                recv_buffer_size: None,
            }));
        let producer_transport = router
            .create_webrtc_transport(transport_options.clone())
            .await
            .map_err(|error| format!("Failed to create producer transport: {error}"))?;

        let consumer_transport = router
            .create_webrtc_transport(transport_options)
            .await
            .map_err(|error| format!("Failed to create consumer transport: {error}"))?;

        Ok(Self {
            client_rtp_capabilities: None,
            consumers: HashMap::new(),
            producers: vec![],
            router,
            transports: Transports {
                consumer: consumer_transport,
                producer: producer_transport,
            },
        })
    }
}

impl Actor for SvcSimulcastConnection {
    type Context = ws::WebsocketContext<Self>;

    fn started(&mut self, ctx: &mut Self::Context) {
        println!("WebSocket connection created");

        // We know that both consumer and producer transports will be used, so we sent server
        // information about both in an initialization message alongside with router capabilities
        // to the client right after WebSocket connection is established
        let server_init_message = ServerMessage::Init {
            consumer_transport_options: TransportOptions {
                id: self.transports.consumer.id(),
                dtls_parameters: self.transports.consumer.dtls_parameters(),
                ice_candidates: self.transports.consumer.ice_candidates().clone(),
                ice_parameters: self.transports.consumer.ice_parameters().clone(),
            },
            producer_transport_options: TransportOptions {
                id: self.transports.producer.id(),
                dtls_parameters: self.transports.producer.dtls_parameters(),
                ice_candidates: self.transports.producer.ice_candidates().clone(),
                ice_parameters: self.transports.producer.ice_parameters().clone(),
            },
            router_rtp_capabilities: self.router.rtp_capabilities().clone(),
        };

        ctx.address().do_send(server_init_message);
    }

    fn stopped(&mut self, _ctx: &mut Self::Context) {
        println!("WebSocket connection closed");
    }
}

impl StreamHandler<Result<ws::Message, ws::ProtocolError>> for SvcSimulcastConnection {
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
                    eprintln!("Failed to parse client message: {text}\n{error}");
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

impl Handler<ClientMessage> for SvcSimulcastConnection {
    type Result = ();

    fn handle(&mut self, message: ClientMessage, ctx: &mut Self::Context) {
        match message {
            ClientMessage::Init { rtp_capabilities } => {
                // We need to know client's RTP capabilities, those are sent using initialization
                // message and are stored in connection struct for future use
                self.client_rtp_capabilities.replace(rtp_capabilities);
            }
            ClientMessage::SetConsumerPreferredLayers {
                id,
                preferred_layers,
            } => {
                if let Some(consumer) = self.consumers.get(&id).cloned() {
                    actix::spawn(async move {
                        match consumer.set_preferred_layers(preferred_layers).await {
                            Ok(_) => {
                                println!(
                                    "Successfully set preferred layers {:?} consumer {}",
                                    preferred_layers,
                                    consumer.id(),
                                );
                            }
                            Err(error) => {
                                println!(
                                    "Failed to set preferred layers {:?} consumer {}: {}",
                                    preferred_layers,
                                    consumer.id(),
                                    error,
                                );
                            }
                        }
                    });
                }
            }
            ClientMessage::ConnectProducerTransport { dtls_parameters } => {
                let address = ctx.address();
                let transport = self.transports.producer.clone();
                // Establish connection for producer transport using DTLS parameters received
                // from the client, but doing so in a background task since this handler is
                // synchronous
                actix::spawn(async move {
                    match transport
                        .connect(WebRtcTransportRemoteParameters { dtls_parameters })
                        .await
                    {
                        Ok(_) => {
                            address.do_send(ServerMessage::ConnectedProducerTransport);
                            println!("Producer transport connected");
                        }
                        Err(error) => {
                            eprintln!("Failed to connect producer transport: {error}");
                            address.do_send(InternalMessage::Stop);
                        }
                    }
                });
            }
            ClientMessage::Produce {
                kind,
                rtp_parameters,
            } => {
                let address = ctx.address();
                let transport = self.transports.producer.clone();
                // Use producer transport to create a new producer on the server with given RTP
                // parameters
                actix::spawn(async move {
                    match transport
                        .produce(ProducerOptions::new(kind, rtp_parameters))
                        .await
                    {
                        Ok(producer) => {
                            let id = producer.id();
                            address.do_send(ServerMessage::Produced { id });
                            // Producer is stored in a hashmap since if we don't do it, it will get
                            // destroyed as soon as its instance goes out out scope
                            address.do_send(InternalMessage::SaveProducer(producer));
                            println!("{kind:?} producer created: {id}");
                        }
                        Err(error) => {
                            eprintln!("Failed to create {kind:?} producer: {error}");
                            address.do_send(InternalMessage::Stop);
                        }
                    }
                });
            }
            ClientMessage::ConnectConsumerTransport { dtls_parameters } => {
                let address = ctx.address();
                let transport = self.transports.consumer.clone();
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
                let transport = self.transports.consumer.clone();
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
                    let mut options = ConsumerOptions::new(producer_id, rtp_capabilities);
                    options.paused = true;

                    match transport.consume(options).await {
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
            ClientMessage::ConsumerResume { id } => {
                if let Some(consumer) = self.consumers.get(&id).cloned() {
                    actix::spawn(async move {
                        match consumer.resume().await {
                            Ok(_) => {
                                println!(
                                    "Successfully resumed {:?} consumer {}",
                                    consumer.kind(),
                                    consumer.id(),
                                );
                            }
                            Err(error) => {
                                println!(
                                    "Failed to resume {:?} consumer {}: {}",
                                    consumer.kind(),
                                    consumer.id(),
                                    error,
                                );
                            }
                        }
                    });
                }
            }
        }
    }
}

/// Simple handler that will transform typed server messages into JSON and send them over to the
/// client over WebSocket connection
impl Handler<ServerMessage> for SvcSimulcastConnection {
    type Result = ();

    fn handle(&mut self, message: ServerMessage, ctx: &mut Self::Context) {
        ctx.text(serde_json::to_string(&message).unwrap());
    }
}

/// Convenience handler for internal messages, these actions require mutable access to the
/// connection struct and having such message handler makes it easy to use from background tasks
/// where otherwise Mutex would have to be used instead
impl Handler<InternalMessage> for SvcSimulcastConnection {
    type Result = ();

    fn handle(&mut self, message: InternalMessage, ctx: &mut Self::Context) {
        match message {
            InternalMessage::Stop => {
                ctx.stop();
            }
            InternalMessage::SaveProducer(producer) => {
                // Retain producer to prevent it from being destroyed
                self.producers.push(producer);
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
    match SvcSimulcastConnection::new(&worker_manager).await {
        Ok(svc_simulcast_connection) => ws::start(svc_simulcast_connection, &request, stream),
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
