use crate::consumer::ConsumerOptions;
use crate::data_consumer::DataConsumerOptions;
use crate::data_producer::DataProducerOptions;
use crate::data_structures::{ListenInfo, Protocol};
use crate::producer::ProducerOptions;
use crate::router::{PipeToRouterOptions, Router, RouterOptions};
use crate::rtp_parameters::{
    MediaKind, MimeTypeVideo, RtpCapabilities, RtpCodecCapability, RtpCodecParameters,
    RtpCodecParametersParameters, RtpParameters,
};
use crate::sctp_parameters::SctpStreamParameters;
use crate::transport::Transport;
use crate::webrtc_transport::{
    WebRtcTransport, WebRtcTransportListenInfos, WebRtcTransportOptions,
};
use crate::worker::WorkerSettings;
use crate::worker_manager::WorkerManager;
use futures_lite::future;
use std::env;
use std::net::{IpAddr, Ipv4Addr};
use std::num::NonZeroU32;

fn media_codecs() -> Vec<RtpCodecCapability> {
    vec![RtpCodecCapability::Video {
        mime_type: MimeTypeVideo::Vp8,
        preferred_payload_type: None,
        clock_rate: NonZeroU32::new(90000).unwrap(),
        parameters: RtpCodecParametersParameters::default(),
        rtcp_feedback: vec![],
    }]
}

fn video_producer_options() -> ProducerOptions {
    ProducerOptions::new(
        MediaKind::Video,
        RtpParameters {
            mid: Some("VIDEO".to_string()),
            codecs: vec![RtpCodecParameters::Video {
                mime_type: MimeTypeVideo::Vp8,
                payload_type: 112,
                clock_rate: NonZeroU32::new(90000).unwrap(),
                parameters: RtpCodecParametersParameters::default(),
                rtcp_feedback: vec![],
            }],
            ..RtpParameters::default()
        },
    )
}

fn data_producer_options() -> DataProducerOptions {
    DataProducerOptions::new_sctp(SctpStreamParameters::new_unordered_with_life_time(
        666, 5000,
    ))
}

fn consumer_device_capabilities() -> RtpCapabilities {
    RtpCapabilities {
        codecs: vec![RtpCodecCapability::Video {
            mime_type: MimeTypeVideo::Vp8,
            preferred_payload_type: Some(101),
            clock_rate: NonZeroU32::new(90000).unwrap(),
            parameters: RtpCodecParametersParameters::default(),
            rtcp_feedback: vec![],
        }],
        header_extensions: vec![],
    }
}

async fn init() -> (Router, Router, WebRtcTransport, WebRtcTransport) {
    {
        let mut builder = env_logger::builder();
        if env::var(env_logger::DEFAULT_FILTER_ENV).is_err() {
            builder.filter_level(log::LevelFilter::Off);
        }
        let _ = builder.is_test(true).try_init();
    }

    let worker_manager = WorkerManager::new();

    let worker1 = worker_manager
        .create_worker(WorkerSettings::default())
        .await
        .expect("Failed to create worker");

    let worker2 = worker_manager
        .create_worker(WorkerSettings::default())
        .await
        .expect("Failed to create worker");

    let router1 = worker1
        .create_router(RouterOptions::new(media_codecs()))
        .await
        .expect("Failed to create router");

    let router2 = worker2
        .create_router(RouterOptions::new(media_codecs()))
        .await
        .expect("Failed to create router");

    let mut transport_options =
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
    transport_options.enable_sctp = true;

    let transport_1 = router1
        .create_webrtc_transport(transport_options.clone())
        .await
        .expect("Failed to create transport1");

    let transport_2 = router2
        .create_webrtc_transport(transport_options)
        .await
        .expect("Failed to create transport2");

    (router1, router2, transport_1, transport_2)
}

#[test]
fn producer_close_is_transmitted_to_pipe_consumer() {
    future::block_on(async move {
        let (router1, router2, transport1, transport2) = init().await;

        let video_producer = transport1
            .produce(video_producer_options())
            .await
            .expect("Failed to produce video");

        router1
            .pipe_producer_to_router(
                video_producer.id(),
                PipeToRouterOptions::new(router2.clone()),
            )
            .await
            .expect("Failed to pipe video producer to router");

        let video_consumer = transport2
            .consume(ConsumerOptions::new(
                video_producer.id(),
                consumer_device_capabilities(),
            ))
            .await
            .expect("Failed to consume video");

        let (mut producer_close_tx, producer_close_rx) = async_oneshot::oneshot::<()>();
        let _handler = video_consumer.on_producer_close(move || {
            let _ = producer_close_tx.send(());
        });

        drop(video_producer);

        producer_close_rx
            .await
            .expect("Failed to receive producer close event");
    });
}

#[test]
fn data_producer_close_is_transmitted_to_pipe_data_consumer() {
    future::block_on(async move {
        let (router1, router2, transport1, transport2) = init().await;

        let data_producer = transport1
            .produce_data(data_producer_options())
            .await
            .expect("Failed to produce data");

        router1
            .pipe_data_producer_to_router(
                data_producer.id(),
                PipeToRouterOptions::new(router2.clone()),
            )
            .await
            .expect("Failed to pipe data producer to router");

        let data_consumer = transport2
            .consume_data(DataConsumerOptions::new_sctp(data_producer.id()))
            .await
            .expect("Failed to create data consumer");

        let (mut data_producer_close_tx, data_producer_close_rx) = async_oneshot::oneshot::<()>();
        let _handler = data_consumer.on_data_producer_close(move || {
            let _ = data_producer_close_tx.send(());
        });

        data_producer.close();

        data_producer_close_rx
            .await
            .expect("Failed to receive data producer close event");
    });
}
