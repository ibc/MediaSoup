#[cfg(test)]
mod tests;

use crate::data_structures::AppData;
use crate::messages::{
    RtpObserverAddProducerRequest, RtpObserverCloseRequest, RtpObserverPauseRequest,
    RtpObserverRemoveProducerRequest, RtpObserverResumeRequest,
};
use crate::producer::{Producer, ProducerId};
use crate::router::Router;
use crate::rtp_observer::{RtpObserver, RtpObserverAddProducerOptions, RtpObserverId};
use crate::worker::{Channel, NotificationParseError, RequestError, SubscriptionHandler};
use async_executor::Executor;
use async_trait::async_trait;
use event_listener_primitives::{Bag, BagOnce, HandlerId};
use log::{debug, error};
use mediasoup_sys::fbs::{audio_level_observer, notification};
use parking_lot::Mutex;
use serde::Deserialize;
use std::fmt;
use std::num::NonZeroU16;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Weak};

/// [`AudioLevelObserver`] options
#[derive(Debug, Clone)]
#[non_exhaustive]
pub struct AudioLevelObserverOptions {
    /// Maximum number of entries in the 'volumes' event.
    /// Default 1.
    pub max_entries: NonZeroU16,
    /// Minimum average volume (in dBvo from -127 to 0) for entries in the 'volumes' event.
    /// Default -80.
    pub threshold: i8,
    /// Interval in ms for checking audio volumes.
    /// Default 1000.
    pub interval: u16,
    /// Custom application data.
    pub app_data: AppData,
}

impl Default for AudioLevelObserverOptions {
    fn default() -> Self {
        Self {
            max_entries: NonZeroU16::new(1).unwrap(),
            threshold: -80,
            interval: 1000,
            app_data: AppData::default(),
        }
    }
}

/// Represents volume of one audio producer.
#[derive(Debug, Clone)]
pub struct AudioLevelObserverVolume {
    /// The audio producer instance.
    pub producer: Producer,
    /// The average volume (in dBvo from -127 to 0) of the audio producer in the last interval.
    pub volume: i8,
}

#[derive(Default)]
#[allow(clippy::type_complexity)]
struct Handlers {
    volumes: Bag<Arc<dyn Fn(&[AudioLevelObserverVolume]) + Send + Sync>>,
    silence: Bag<Arc<dyn Fn() + Send + Sync>>,
    pause: Bag<Arc<dyn Fn() + Send + Sync>>,
    resume: Bag<Arc<dyn Fn() + Send + Sync>>,
    add_producer: Bag<Arc<dyn Fn(&Producer) + Send + Sync>, Producer>,
    remove_producer: Bag<Arc<dyn Fn(&Producer) + Send + Sync>, Producer>,
    router_close: BagOnce<Box<dyn FnOnce() + Send>>,
    close: BagOnce<Box<dyn FnOnce() + Send>>,
}

#[derive(Debug, Deserialize)]
#[serde(rename_all = "camelCase")]
struct VolumeNotification {
    producer_id: ProducerId,
    volume: i8,
}

#[derive(Debug, Deserialize)]
#[serde(tag = "event", rename_all = "lowercase", content = "data")]
enum Notification {
    Volumes(Vec<VolumeNotification>),
    Silence,
}

impl Notification {
    pub(crate) fn from_fbs(
        notification: notification::NotificationRef<'_>,
    ) -> Result<Self, NotificationParseError> {
        match notification.event().unwrap() {
            notification::Event::AudiolevelobserverVolumes => {
                let Ok(Some(notification::BodyRef::AudioLevelObserverVolumesNotification(body))) =
                    notification.body()
                else {
                    panic!("Wrong message from worker: {notification:?}");
                };

                let volumes_fbs: Vec<_> = body
                    .volumes()
                    .unwrap()
                    .iter()
                    .map(|volume| audio_level_observer::Volume::try_from(volume.unwrap()).unwrap())
                    .collect();
                let volumes = volumes_fbs
                    .iter()
                    .map(|volume| VolumeNotification {
                        producer_id: volume.producer_id.parse().unwrap(),
                        volume: volume.volume,
                    })
                    .collect();

                Ok(Notification::Volumes(volumes))
            }
            notification::Event::AudiolevelobserverSilence => Ok(Notification::Silence),
            _ => Err(NotificationParseError::InvalidEvent),
        }
    }
}

struct Inner {
    id: RtpObserverId,
    executor: Arc<Executor<'static>>,
    channel: Channel,
    handlers: Arc<Handlers>,
    paused: AtomicBool,
    app_data: AppData,
    // Make sure router is not dropped until this audio level observer is not dropped
    router: Router,
    closed: AtomicBool,
    // Drop subscription to audio level observer-specific notifications when observer itself is
    // dropped
    _subscription_handler: Mutex<Option<SubscriptionHandler>>,
    _on_router_close_handler: Mutex<HandlerId>,
}

impl Drop for Inner {
    fn drop(&mut self) {
        debug!("drop()");

        self.close(true);
    }
}

impl Inner {
    fn close(&self, close_request: bool) {
        if !self.closed.swap(true, Ordering::SeqCst) {
            debug!("close()");

            self.handlers.close.call_simple();

            if close_request {
                let channel = self.channel.clone();
                let router_id = self.router.id();
                let request = RtpObserverCloseRequest {
                    rtp_observer_id: self.id,
                };

                self.executor
                    .spawn(async move {
                        if let Err(error) = channel.request(router_id, request).await {
                            error!("audio level observer closing failed on drop: {}", error);
                        }
                    })
                    .detach();
            }
        }
    }
}

/// An audio level observer monitors the volume of the selected audio producers.
///
/// It just handles audio producers (if [`AudioLevelObserver::add_producer()`] is called with a
/// video producer it will fail).
///
/// Audio levels are read from an RTP header extension. No decoding of audio data is done. See
/// [RFC6464](https://tools.ietf.org/html/rfc6464) for more information.
#[derive(Clone)]
#[must_use = "Audio level observer will be closed on drop, make sure to keep it around for as long as needed"]
pub struct AudioLevelObserver {
    inner: Arc<Inner>,
}

impl fmt::Debug for AudioLevelObserver {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("AudioLevelObserver")
            .field("id", &self.inner.id)
            .field("paused", &self.inner.paused)
            .field("router", &self.inner.router)
            .field("closed", &self.inner.closed)
            .finish()
    }
}

#[async_trait]
impl RtpObserver for AudioLevelObserver {
    fn id(&self) -> RtpObserverId {
        self.inner.id
    }

    fn router(&self) -> &Router {
        &self.inner.router
    }

    fn paused(&self) -> bool {
        self.inner.paused.load(Ordering::SeqCst)
    }

    fn app_data(&self) -> &AppData {
        &self.inner.app_data
    }

    fn closed(&self) -> bool {
        self.inner.closed.load(Ordering::SeqCst)
    }

    async fn pause(&self) -> Result<(), RequestError> {
        debug!("pause()");

        self.inner
            .channel
            .request(self.id(), RtpObserverPauseRequest {})
            .await?;

        let was_paused = self.inner.paused.swap(true, Ordering::SeqCst);

        if !was_paused {
            self.inner.handlers.pause.call_simple();
        }

        Ok(())
    }

    async fn resume(&self) -> Result<(), RequestError> {
        debug!("resume()");

        self.inner
            .channel
            .request(self.id(), RtpObserverResumeRequest {})
            .await?;

        let was_paused = self.inner.paused.swap(false, Ordering::SeqCst);

        if !was_paused {
            self.inner.handlers.resume.call_simple();
        }

        Ok(())
    }

    async fn add_producer(
        &self,
        RtpObserverAddProducerOptions { producer_id }: RtpObserverAddProducerOptions,
    ) -> Result<(), RequestError> {
        let producer = match self.inner.router.get_producer(&producer_id) {
            Some(producer) => producer,
            None => {
                return Ok(());
            }
        };
        self.inner
            .channel
            .request(self.id(), RtpObserverAddProducerRequest { producer_id })
            .await?;

        self.inner.handlers.add_producer.call_simple(&producer);

        Ok(())
    }

    async fn remove_producer(&self, producer_id: ProducerId) -> Result<(), RequestError> {
        let producer = match self.inner.router.get_producer(&producer_id) {
            Some(producer) => producer,
            None => {
                return Ok(());
            }
        };
        self.inner
            .channel
            .request(self.id(), RtpObserverRemoveProducerRequest { producer_id })
            .await?;

        self.inner.handlers.remove_producer.call_simple(&producer);

        Ok(())
    }

    fn on_pause(&self, callback: Box<dyn Fn() + Send + Sync + 'static>) -> HandlerId {
        self.inner.handlers.pause.add(Arc::new(callback))
    }

    fn on_resume(&self, callback: Box<dyn Fn() + Send + Sync + 'static>) -> HandlerId {
        self.inner.handlers.resume.add(Arc::new(callback))
    }

    fn on_add_producer(
        &self,
        callback: Box<dyn Fn(&Producer) + Send + Sync + 'static>,
    ) -> HandlerId {
        self.inner.handlers.add_producer.add(Arc::new(callback))
    }

    fn on_remove_producer(
        &self,
        callback: Box<dyn Fn(&Producer) + Send + Sync + 'static>,
    ) -> HandlerId {
        self.inner.handlers.remove_producer.add(Arc::new(callback))
    }

    fn on_router_close(&self, callback: Box<dyn FnOnce() + Send + 'static>) -> HandlerId {
        self.inner.handlers.router_close.add(Box::new(callback))
    }

    fn on_close(&self, callback: Box<dyn FnOnce() + Send + 'static>) -> HandlerId {
        let handler_id = self.inner.handlers.close.add(Box::new(callback));
        if self.inner.closed.load(Ordering::Relaxed) {
            self.inner.handlers.close.call_simple();
        }
        handler_id
    }
}

impl AudioLevelObserver {
    pub(super) fn new(
        id: RtpObserverId,
        executor: Arc<Executor<'static>>,
        channel: Channel,
        app_data: AppData,
        router: Router,
    ) -> Self {
        debug!("new()");

        let handlers = Arc::<Handlers>::default();
        let paused = AtomicBool::new(false);

        let subscription_handler = {
            let router = router.clone();
            let handlers = Arc::clone(&handlers);

            channel.subscribe_to_notifications(id.into(), move |notification| {
                match Notification::from_fbs(notification) {
                    Ok(notification) => match notification {
                        Notification::Volumes(volumes) => {
                            let volumes = volumes
                                .iter()
                                .filter_map(|notification| {
                                    let VolumeNotification {
                                        producer_id,
                                        volume,
                                    } = notification;
                                    router.get_producer(producer_id).map(|producer| {
                                        AudioLevelObserverVolume {
                                            producer,
                                            volume: *volume,
                                        }
                                    })
                                })
                                .collect::<Vec<_>>();

                            handlers.volumes.call(|callback| {
                                callback(&volumes);
                            });
                        }
                        Notification::Silence => {
                            handlers.silence.call_simple();
                        }
                    },
                    Err(error) => {
                        error!("Failed to parse notification: {}", error);
                    }
                }
            })
        };

        let inner_weak = Arc::<Mutex<Option<Weak<Inner>>>>::default();
        let on_router_close_handler = router.on_close({
            let inner_weak = Arc::clone(&inner_weak);

            move || {
                let maybe_inner = inner_weak.lock().as_ref().and_then(Weak::upgrade);
                if let Some(inner) = maybe_inner {
                    inner.handlers.router_close.call_simple();
                    inner.close(false);
                }
            }
        });
        let inner = Arc::new(Inner {
            id,
            executor,
            channel,
            handlers,
            paused,
            app_data,
            router,
            closed: AtomicBool::new(false),
            _subscription_handler: Mutex::new(subscription_handler),
            _on_router_close_handler: Mutex::new(on_router_close_handler),
        });

        inner_weak.lock().replace(Arc::downgrade(&inner));

        Self { inner }
    }

    /// Callback is called at most every interval (see [`AudioLevelObserverOptions`]).
    ///
    /// Audio volumes entries ordered by volume (louder ones go first).
    pub fn on_volumes<F: Fn(&[AudioLevelObserverVolume]) + Send + Sync + 'static>(
        &self,
        callback: F,
    ) -> HandlerId {
        self.inner.handlers.volumes.add(Arc::new(callback))
    }

    /// Callback is called when no one of the producers in this RTP observer is generating audio with a volume
    /// beyond the given threshold.
    pub fn on_silence<F: Fn() + Send + Sync + 'static>(&self, callback: F) -> HandlerId {
        self.inner.handlers.silence.add(Arc::new(callback))
    }

    /// Downgrade `AudioLevelObserver` to [`WeakAudioLevelObserver`] instance.
    #[must_use]
    pub fn downgrade(&self) -> WeakAudioLevelObserver {
        WeakAudioLevelObserver {
            inner: Arc::downgrade(&self.inner),
        }
    }
}

/// [`WeakAudioLevelObserver`] doesn't own audio level observer instance on mediasoup-worker and
/// will not prevent one from being destroyed once last instance of regular [`AudioLevelObserver`]
/// is dropped.
///
/// [`WeakAudioLevelObserver`] vs [`AudioLevelObserver`] is similar to [`Weak`] vs [`Arc`].
#[derive(Clone)]
pub struct WeakAudioLevelObserver {
    inner: Weak<Inner>,
}

impl fmt::Debug for WeakAudioLevelObserver {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("WeakAudioLevelObserver").finish()
    }
}

impl WeakAudioLevelObserver {
    /// Attempts to upgrade `WeakAudioLevelObserver` to [`AudioLevelObserver`] if last instance of one wasn't
    /// dropped yet.
    #[must_use]
    pub fn upgrade(&self) -> Option<AudioLevelObserver> {
        let inner = self.inner.upgrade()?;

        Some(AudioLevelObserver { inner })
    }
}
