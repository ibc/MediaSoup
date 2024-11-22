use crate::consumer::ConsumerOptions;
use crate::data_structures::{ListenInfo, Protocol};
use crate::producer::ProducerOptions;
use crate::router::{Router, RouterOptions};
use crate::rtp_parameters::{
    MediaKind, MimeTypeAudio, RtpCapabilities, RtpCodecCapability, RtpCodecParameters,
    RtpCodecParametersParameters, RtpParameters,
};
use crate::transport::Transport;
use crate::webrtc_transport::{
    WebRtcTransport, WebRtcTransportListenInfos, WebRtcTransportOptions,
};
use crate::worker::WorkerSettings;
use crate::worker_manager::WorkerManager;
use futures_lite::future;
use std::env;
use std::net::{IpAddr, Ipv4Addr};
use std::num::{NonZeroU32, NonZeroU8};

fn media_codecs() -> Vec<RtpCodecCapability> {
    vec![RtpCodecCapability::Audio {
        mime_type: MimeTypeAudio::Opus,
        preferred_payload_type: None,
        clock_rate: NonZeroU32::new(48000).unwrap(),
        channels: NonZeroU8::new(2).unwrap(),
        parameters: RtpCodecParametersParameters::default(),
        rtcp_feedback: vec![],
    }]
}

fn audio_producer_options() -> ProducerOptions {
    ProducerOptions::new(
        MediaKind::Audio,
        RtpParameters {
            mid: Some("AUDIO".to_string()),
            codecs: vec![RtpCodecParameters::Audio {
                mime_type: MimeTypeAudio::Opus,
                payload_type: 111,
                clock_rate: NonZeroU32::new(48000).unwrap(),
                channels: NonZeroU8::new(2).unwrap(),
                parameters: RtpCodecParametersParameters::default(),
                rtcp_feedback: vec![],
            }],
            ..RtpParameters::default()
        },
    )
}

fn consumer_device_capabilities() -> RtpCapabilities {
    RtpCapabilities {
        codecs: vec![RtpCodecCapability::Audio {
            mime_type: MimeTypeAudio::Opus,
            preferred_payload_type: Some(100),
            clock_rate: NonZeroU32::new(48000).unwrap(),
            channels: NonZeroU8::new(2).unwrap(),
            parameters: RtpCodecParametersParameters::default(),
            rtcp_feedback: vec![],
        }],
        ..RtpCapabilities::default()
    }
}

async fn init() -> (Router, WebRtcTransport, WebRtcTransport) {
    {
        let mut builder = env_logger::builder();
        if env::var(env_logger::DEFAULT_FILTER_ENV).is_err() {
            builder.filter_level(log::LevelFilter::Off);
        }
        let _ = builder.is_test(true).try_init();
    }

    let worker_manager = WorkerManager::new();

    let worker = worker_manager
        .create_worker(WorkerSettings::default())
        .await
        .expect("Failed to create worker");

    let router = worker
        .create_router(RouterOptions::new(media_codecs()))
        .await
        .expect("Failed to create router");

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

    let transport_1 = router
        .create_webrtc_transport(transport_options.clone())
        .await
        .expect("Failed to create transport1");

    let transport_2 = router
        .create_webrtc_transport(transport_options)
        .await
        .expect("Failed to create transport2");

    (router, transport_1, transport_2)
}

#[test]
fn producer_close_event() {
    future::block_on(async move {
        let (_router, transport_1, transport_2) = init().await;

        let audio_producer = transport_1
            .produce(audio_producer_options())
            .await
            .expect("Failed to produce audio");

        let audio_consumer = transport_2
            .consume(ConsumerOptions::new(
                audio_producer.id(),
                consumer_device_capabilities(),
            ))
            .await
            .expect("Failed to consume audio");

        let (mut close_tx, close_rx) = async_oneshot::oneshot::<()>();
        let _handler = audio_consumer.on_close(move || {
            let _ = close_tx.send(());
        });

        let (mut producer_close_tx, producer_close_rx) = async_oneshot::oneshot::<()>();
        let _handler = audio_consumer.on_producer_close(move || {
            let _ = producer_close_tx.send(());
        });

        drop(audio_producer);

        producer_close_rx
            .await
            .expect("Failed to receive producer_close event");

        close_rx.await.expect("Failed to receive close event");

        assert!(audio_consumer.closed());
    });
}

#[test]
fn transport_close_event() {
    future::block_on(async move {
        let (router, transport_1, transport_2) = init().await;

        let audio_producer = transport_1
            .produce(audio_producer_options())
            .await
            .expect("Failed to produce audio");

        let audio_consumer = transport_2
            .consume(ConsumerOptions::new(
                audio_producer.id(),
                consumer_device_capabilities(),
            ))
            .await
            .expect("Failed to consume audio");

        let (mut close_tx, close_rx) = async_oneshot::oneshot::<()>();
        let _handler = audio_consumer.on_close(move || {
            let _ = close_tx.send(());
        });

        let (mut transport_close_tx, transport_close_rx) = async_oneshot::oneshot::<()>();
        let _handler = audio_consumer.on_transport_close(move || {
            let _ = transport_close_tx.send(());
        });

        router.close();

        transport_close_rx
            .await
            .expect("Failed to receive transport_close event");
        close_rx.await.expect("Failed to receive close event");

        assert!(audio_consumer.closed());
    });
}
