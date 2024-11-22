//! A WebRTC server brings the ability to listen on a single UDP/TCP port for multiple
//! `WebRtcTransport`s.
//!
//! A WebRTC server exists within the context of a [`Worker`], meaning that if your app launches N
//! workers it also needs to create N WebRTC servers listening on different ports (to not collide).
//! The WebRTC transport implementation of mediasoup is
//! [ICE Lite](https://tools.ietf.org/html/rfc5245#section-2.7), meaning that it does not initiate
//! ICE connections but expects ICE Binding Requests from endpoints.

#[cfg(test)]
mod tests;

use crate::data_structures::{AppData, ListenInfo};
use crate::messages::{WebRtcServerCloseRequest, WebRtcServerDumpRequest};
use crate::transport::TransportId;
use crate::uuid_based_wrapper_type;
use crate::webrtc_transport::WebRtcTransport;
use crate::worker::{Channel, RequestError, Worker};
use async_executor::Executor;
use event_listener_primitives::{BagOnce, HandlerId};
use hash_hasher::HashedSet;
use log::{debug, error};
use mediasoup_sys::fbs::transport;
use parking_lot::Mutex;
use serde::{Deserialize, Serialize};
use std::fmt;
use std::net::IpAddr;
use std::ops::Deref;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Weak};
use thiserror::Error;

uuid_based_wrapper_type!(
    /// [`WebRtcServer`] identifier.
    WebRtcServerId
);

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash, Deserialize, Serialize)]
#[doc(hidden)]
pub struct WebRtcServerIpPort {
    pub ip: IpAddr,
    pub port: u16,
}

#[derive(Debug, Clone, Eq, PartialEq, Hash, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
#[doc(hidden)]
pub struct WebRtcServerIceUsernameFragment {
    pub local_ice_username_fragment: String,
    #[serde(rename = "webRtcTransportId")]
    pub webrtc_transport_id: TransportId,
}

#[derive(Debug, Clone, Eq, PartialEq, Hash, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
#[doc(hidden)]
pub struct WebRtcServerTupleHash {
    pub tuple_hash: u64,
    #[serde(rename = "webRtcTransportId")]
    pub webrtc_transport_id: TransportId,
}

#[derive(Debug, Clone, Eq, PartialEq, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
#[doc(hidden)]
#[non_exhaustive]
pub struct WebRtcServerDump {
    pub id: WebRtcServerId,
    pub udp_sockets: Vec<WebRtcServerIpPort>,
    pub tcp_servers: Vec<WebRtcServerIpPort>,
    #[serde(rename = "webRtcTransportIds")]
    pub webrtc_transport_ids: HashedSet<TransportId>,
    pub local_ice_username_fragments: Vec<WebRtcServerIceUsernameFragment>,
    pub tuple_hashes: Vec<WebRtcServerTupleHash>,
}

/// Struct that protects an invariant of having non-empty list of listen infos.
#[derive(Debug, Clone, Eq, PartialEq, Serialize)]
pub struct WebRtcServerListenInfos(Vec<ListenInfo>);

impl WebRtcServerListenInfos {
    /// Create WebRTC server listen infos with given info populated initially.
    #[must_use]
    pub fn new(listen_info: ListenInfo) -> Self {
        Self(vec![listen_info])
    }

    /// Insert another listen info.
    #[must_use]
    pub fn insert(mut self, listen_info: ListenInfo) -> Self {
        self.0.push(listen_info);
        self
    }

    pub(crate) fn to_fbs(&self) -> Vec<transport::ListenInfo> {
        self.0
            .iter()
            .map(|listen_info| listen_info.to_fbs())
            .collect()
    }
}

impl Deref for WebRtcServerListenInfos {
    type Target = Vec<ListenInfo>;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

/// Empty list of listen infos provided, should have at least one element.
#[derive(Error, Debug, Eq, PartialEq)]
#[error("Empty list of listen infos provided, should have at least one element")]
pub struct EmptyListError;

impl TryFrom<Vec<ListenInfo>> for WebRtcServerListenInfos {
    type Error = EmptyListError;

    fn try_from(listen_infos: Vec<ListenInfo>) -> Result<Self, Self::Error> {
        if listen_infos.is_empty() {
            Err(EmptyListError)
        } else {
            Ok(Self(listen_infos))
        }
    }
}

/// [`WebRtcServer`] options.
#[derive(Debug)]
#[non_exhaustive]
pub struct WebRtcServerOptions {
    /// Listening infos in order of preference (first one is the preferred one).
    pub listen_infos: WebRtcServerListenInfos,
    /// Custom application data.
    pub app_data: AppData,
}

impl WebRtcServerOptions {
    /// Create [`WebRtcServer`] options with given listen infos.
    pub fn new(listen_infos: WebRtcServerListenInfos) -> Self {
        Self {
            listen_infos,
            app_data: AppData::default(),
        }
    }
}

#[derive(Default)]
#[allow(clippy::type_complexity)]
struct Handlers {
    new_webrtc_transport: BagOnce<Box<dyn Fn(&WebRtcTransport) + Send>>,
    worker_close: BagOnce<Box<dyn FnOnce() + Send>>,
    close: BagOnce<Box<dyn FnOnce() + Send>>,
}

struct Inner {
    id: WebRtcServerId,
    executor: Arc<Executor<'static>>,
    channel: Channel,
    handlers: Arc<Handlers>,
    app_data: AppData,
    worker: Worker,
    closed: AtomicBool,
    _on_worker_close_handler: Mutex<HandlerId>,
}

impl Drop for Inner {
    fn drop(&mut self) {
        debug!("drop()");

        self.close();
    }
}

impl Inner {
    fn close(&self) {
        if !self.closed.swap(true, Ordering::SeqCst) {
            self.handlers.close.call_simple();

            {
                let channel = self.channel.clone();
                let request = WebRtcServerCloseRequest {
                    webrtc_server_id: self.id,
                };
                self.executor
                    .spawn(async move {
                        if let Err(error) = channel.request("", request).await {
                            error!("WebRTC server closing failed on drop: {}", error);
                        }
                    })
                    .detach();
            }
        }
    }
}

/// A WebRTC server brings the ability to listen on a single UDP/TCP port for multiple
/// `WebRtcTransport`s.
///
/// A WebRTC server exists within the context of a [`Worker`], meaning that if your app launches N
/// workers it also needs to create N WebRTC servers listening on different ports (to not collide).
/// The WebRTC transport implementation of mediasoup is
/// [ICE Lite](https://tools.ietf.org/html/rfc5245#section-2.7), meaning that it does not initiate
/// ICE connections but expects ICE Binding Requests from endpoints.
#[derive(Clone)]
pub struct WebRtcServer {
    inner: Arc<Inner>,
}

impl fmt::Debug for WebRtcServer {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("WebRtcServer")
            .field("id", &self.inner.id)
            .field("worker", &self.inner.worker)
            .field("closed", &self.inner.closed)
            .finish()
    }
}

impl WebRtcServer {
    pub(crate) fn new(
        id: WebRtcServerId,
        executor: Arc<Executor<'static>>,
        channel: Channel,
        app_data: AppData,
        worker: Worker,
    ) -> Self {
        let handlers = Arc::<Handlers>::default();
        let inner_weak = Arc::<Mutex<Option<Weak<Inner>>>>::default();
        let on_worker_close_handler = worker.on_close({
            let inner_weak = Arc::clone(&inner_weak);

            move || {
                let maybe_inner = inner_weak.lock().as_ref().and_then(Weak::upgrade);
                if let Some(inner) = maybe_inner {
                    inner.handlers.worker_close.call_simple();
                    if !inner.closed.swap(true, Ordering::SeqCst) {
                        inner.handlers.close.call_simple();
                    }
                }
            }
        });
        let inner = Arc::new(Inner {
            id,
            executor,
            channel,
            handlers,
            app_data,
            worker,
            closed: AtomicBool::new(false),
            _on_worker_close_handler: Mutex::new(on_worker_close_handler),
        });

        inner_weak.lock().replace(Arc::downgrade(&inner));

        Self { inner }
    }

    /// Router id.
    #[must_use]
    pub fn id(&self) -> WebRtcServerId {
        self.inner.id
    }

    /// Worker to which WebRTC server belongs.
    pub fn worker(&self) -> &Worker {
        &self.inner.worker
    }

    /// Custom application data.
    #[must_use]
    pub fn app_data(&self) -> &AppData {
        &self.inner.app_data
    }

    /// Whether WebRTC server is closed.
    #[must_use]
    pub fn closed(&self) -> bool {
        self.inner.closed.load(Ordering::SeqCst)
    }

    /// Dump WebRTC server.
    #[doc(hidden)]
    pub async fn dump(&self) -> Result<WebRtcServerDump, RequestError> {
        debug!("dump()");

        self.inner
            .channel
            .request(self.id(), WebRtcServerDumpRequest {})
            .await
    }

    /// Callback is called when the worker this WebRTC server belongs to is closed for whatever
    /// reason.
    /// The WebRtc server itself is also closed. A `on_webrtc_server_close` callbacks are
    /// triggered in all relevant WebRTC transports.
    pub fn on_worker_close<F: FnOnce() + Send + 'static>(&self, callback: F) -> HandlerId {
        self.inner.handlers.worker_close.add(Box::new(callback))
    }

    /// Callback is called when new [`WebRtcTransport`] is added that uses this WebRTC server.
    pub fn on_new_webrtc_transport<F>(&self, callback: F) -> HandlerId
    where
        F: Fn(&WebRtcTransport) + Send + 'static,
    {
        self.inner
            .handlers
            .new_webrtc_transport
            .add(Box::new(callback))
    }

    /// Callback is called when the WebRTC server is closed for whatever reason.
    ///
    /// NOTE: Callback will be called in place if WebRTC server is already closed.
    pub fn on_close<F: FnOnce() + Send + 'static>(&self, callback: F) -> HandlerId {
        let handler_id = self.inner.handlers.close.add(Box::new(callback));
        if self.inner.closed.load(Ordering::Relaxed) {
            self.inner.handlers.close.call_simple();
        }
        handler_id
    }

    pub(crate) fn notify_new_webrtc_transport(&self, webrtc_transport: &WebRtcTransport) {
        self.inner
            .handlers
            .new_webrtc_transport
            .call(|callback| callback(webrtc_transport));
    }

    #[cfg(test)]
    pub(crate) fn close(&self) {
        self.inner.close();
    }
}
