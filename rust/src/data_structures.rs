//! Miscellaneous data structures.

#[cfg(test)]
mod tests;

use mediasoup_sys::fbs::{
    common, producer, rtp_packet, sctp_association, transport, web_rtc_transport,
};
use serde::de::{MapAccess, Visitor};
use serde::ser::SerializeStruct;
use serde::{de, Deserialize, Deserializer, Serialize, Serializer};
use std::any::Any;
use std::borrow::Cow;
use std::fmt;
use std::net::IpAddr;
use std::ops::{Deref, DerefMut, RangeInclusive};
use std::sync::Arc;

/// Container for arbitrary data attached to mediasoup entities.
#[derive(Debug, Clone)]
pub struct AppData(Arc<dyn Any + Send + Sync>);

impl Default for AppData {
    fn default() -> Self {
        Self::new(())
    }
}

impl Deref for AppData {
    type Target = Arc<dyn Any + Send + Sync>;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl DerefMut for AppData {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0
    }
}

impl AppData {
    /// Construct app data from almost anything
    pub fn new<T: Any + Send + Sync>(app_data: T) -> Self {
        Self(Arc::new(app_data))
    }
}

/// Listening protocol, IP and port for [`WebRtcServer`](crate::webrtc_server::WebRtcServer) to listen on.
///
/// # Notes on usage
/// If you use "0.0.0.0" or "::" as ip value, then you need to also provide
/// `announced_address`.
#[derive(Debug, Clone, Eq, PartialEq, Hash, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct ListenInfo {
    /// Network protocol.
    pub protocol: Protocol,
    /// Listening IPv4 or IPv6.
    pub ip: IpAddr,
    /// Announced IPv4, IPv6 or hostname (useful when running mediasoup behind
    /// NAT with private IP).
    #[serde(skip_serializing_if = "Option::is_none")]
    pub announced_address: Option<String>,
    /// Listening port.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub port: Option<u16>,
    /// Listening port range. If given then |port| will be ignored.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub port_range: Option<RangeInclusive<u16>>,
    /// Socket flags.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub flags: Option<SocketFlags>,
    /// Send buffer size (bytes).
    #[serde(skip_serializing_if = "Option::is_none")]
    pub send_buffer_size: Option<u32>,
    /// Recv buffer size (bytes).
    #[serde(skip_serializing_if = "Option::is_none")]
    pub recv_buffer_size: Option<u32>,
}

impl ListenInfo {
    pub(crate) fn to_fbs(&self) -> transport::ListenInfo {
        transport::ListenInfo {
            protocol: match self.protocol {
                Protocol::Tcp => transport::Protocol::Tcp,
                Protocol::Udp => transport::Protocol::Udp,
            },
            ip: self.ip.to_string(),
            announced_address: self
                .announced_address
                .as_ref()
                .map(|address| address.to_string()),
            port: self.port.unwrap_or(0),
            port_range: match &self.port_range {
                Some(port_range) => Box::new(transport::PortRange {
                    min: *port_range.start(),
                    max: *port_range.end(),
                }),
                None => Box::new(transport::PortRange { min: 0, max: 0 }),
            },
            flags: Box::new(self.flags.unwrap_or_default().to_fbs()),
            send_buffer_size: self.send_buffer_size.unwrap_or(0),
            recv_buffer_size: self.recv_buffer_size.unwrap_or(0),
        }
    }
}

/// UDP/TCP socket flags.
#[derive(
    Default, Debug, Copy, Clone, Ord, PartialOrd, Eq, PartialEq, Hash, Deserialize, Serialize,
)]
#[serde(rename_all = "camelCase")]
pub struct SocketFlags {
    /// Disable dual-stack support so only IPv6 is used (only if ip is IPv6).
    /// Defaults to false.
    pub ipv6_only: bool,
    /// Make different transports bind to the same ip and port (only for UDP).
    /// Useful for multicast scenarios with plain transport. Use with caution.
    /// Defaults to false.
    pub udp_reuse_port: bool,
}

impl SocketFlags {
    pub(crate) fn to_fbs(self) -> transport::SocketFlags {
        transport::SocketFlags {
            ipv6_only: self.ipv6_only,
            udp_reuse_port: self.udp_reuse_port,
        }
    }
}

/// ICE role.
#[derive(Debug, Copy, Clone, Ord, PartialOrd, Eq, PartialEq, Hash, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
pub enum IceRole {
    /// The transport is the controlled agent.
    Controlled,
    /// The transport is the controlling agent.
    Controlling,
}

impl IceRole {
    pub(crate) fn from_fbs(role: web_rtc_transport::IceRole) -> Self {
        match role {
            web_rtc_transport::IceRole::Controlled => IceRole::Controlled,
            web_rtc_transport::IceRole::Controlling => IceRole::Controlling,
        }
    }
}

/// ICE parameters.
#[derive(Debug, Clone, Ord, PartialOrd, Eq, PartialEq, Hash, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct IceParameters {
    /// ICE username fragment.
    pub username_fragment: String,
    /// ICE password.
    pub password: String,
    /// ICE Lite.
    pub ice_lite: Option<bool>,
}

impl IceParameters {
    pub(crate) fn from_fbs(parameters: web_rtc_transport::IceParameters) -> Self {
        Self {
            username_fragment: parameters.username_fragment.to_string(),
            password: parameters.password.to_string(),
            ice_lite: Some(parameters.ice_lite),
        }
    }
}

/// ICE candidate type.
#[derive(Debug, Copy, Clone, Ord, PartialOrd, Eq, PartialEq, Hash, Deserialize, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum IceCandidateType {
    /// The candidate is a host candidate, whose IP address as specified in the
    /// [`IceCandidate::address`] property is in fact the true address of the remote peer.
    Host,
    /// The candidate is a server reflexive candidate; the [`IceCandidate::address`] indicates an
    /// intermediary address assigned by the STUN server to represent the candidate's peer
    /// anonymously.
    Srflx,
    /// The candidate is a peer reflexive candidate; the [`IceCandidate::address`] is an intermediary
    /// address assigned by the STUN server to represent the candidate's peer anonymously.
    Prflx,
    /// The candidate is a relay candidate, obtained from a TURN server. The relay candidate's IP
    /// address is an address the [TURN](https://developer.mozilla.org/en-US/docs/Glossary/TURN)
    /// server uses to forward the media between the two peers.
    Relay,
}

impl IceCandidateType {
    pub(crate) fn from_fbs(candidate_type: web_rtc_transport::IceCandidateType) -> Self {
        match candidate_type {
            web_rtc_transport::IceCandidateType::Host => IceCandidateType::Host,
        }
    }
}

/// ICE candidate TCP type (always `Passive`).
#[derive(Debug, Copy, Clone, Ord, PartialOrd, Eq, PartialEq, Hash, Deserialize, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum IceCandidateTcpType {
    /// Passive.
    Passive,
}

impl IceCandidateTcpType {
    pub(crate) fn from_fbs(candidate_type: web_rtc_transport::IceCandidateTcpType) -> Self {
        match candidate_type {
            web_rtc_transport::IceCandidateTcpType::Passive => IceCandidateTcpType::Passive,
        }
    }
}

/// Transport protocol.
#[derive(Debug, Copy, Clone, Ord, PartialOrd, Eq, PartialEq, Hash, Deserialize, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum Protocol {
    /// TCP.
    Tcp,
    /// UDP.
    Udp,
}

impl Protocol {
    pub(crate) fn from_fbs(protocol: transport::Protocol) -> Self {
        match protocol {
            transport::Protocol::Tcp => Protocol::Tcp,
            transport::Protocol::Udp => Protocol::Udp,
        }
    }
}

/// ICE candidate
#[derive(Debug, Clone, Ord, PartialOrd, Eq, PartialEq, Hash, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct IceCandidate {
    /// Unique identifier that allows ICE to correlate candidates that appear on multiple
    /// transports.
    pub foundation: String,
    /// The assigned priority of the candidate.
    pub priority: u32,
    /// The IP address or hostname of the candidate.
    pub address: String,
    /// The protocol of the candidate.
    pub protocol: Protocol,
    /// The port for the candidate.
    pub port: u16,
    /// The type of candidate (always `Host`).
    pub r#type: IceCandidateType,
    /// The type of TCP candidate (always `Passive`).
    #[serde(skip_serializing_if = "Option::is_none")]
    pub tcp_type: Option<IceCandidateTcpType>,
}

impl IceCandidate {
    pub(crate) fn from_fbs(candidate: &web_rtc_transport::IceCandidate) -> Self {
        Self {
            foundation: candidate.foundation.clone(),
            priority: candidate.priority,
            address: candidate.address.clone(),
            protocol: Protocol::from_fbs(candidate.protocol),
            port: candidate.port,
            r#type: IceCandidateType::from_fbs(candidate.type_),
            tcp_type: candidate.tcp_type.map(IceCandidateTcpType::from_fbs),
        }
    }
}

/// ICE state.
#[derive(Debug, Copy, Clone, Ord, PartialOrd, Eq, PartialEq, Hash, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
pub enum IceState {
    /// No ICE Binding Requests have been received yet.
    New,
    /// Valid ICE Binding Request have been received, but none with USE-CANDIDATE attribute.
    /// Outgoing media is allowed.
    Connected,
    /// ICE Binding Request with USE_CANDIDATE attribute has been received. Media in both directions
    /// is now allowed.
    Completed,
    /// ICE was `Connected` or `Completed` but it has suddenly failed (this can just happen if the
    /// selected tuple has `Tcp` protocol).
    Disconnected,
}

impl IceState {
    pub(crate) fn from_fbs(state: web_rtc_transport::IceState) -> Self {
        match state {
            web_rtc_transport::IceState::New => IceState::New,
            web_rtc_transport::IceState::Connected => IceState::Connected,
            web_rtc_transport::IceState::Completed => IceState::Completed,
            web_rtc_transport::IceState::Disconnected => IceState::Disconnected,
        }
    }
}

/// Tuple of local IP/port/protocol + optional remote IP/port.
///
/// # Notes on usage
/// Both `remote_ip` and `remote_port` are unset until the media address of the remote endpoint is
/// known, which happens after calling `transport.connect()` in `PlainTransport` and
/// `PipeTransport`, or via dynamic detection as it happens in `WebRtcTransport` (in which the
/// remote media address is detected by ICE means), or in `PlainTransport` (when using `comedia`
/// mode).
#[derive(Debug, Clone, Ord, PartialOrd, Eq, PartialEq, Hash, Deserialize, Serialize)]
#[serde(untagged)]
pub enum TransportTuple {
    /// Transport tuple with remote endpoint info.
    #[serde(rename_all = "camelCase")]
    WithRemote {
        /// Local IP address or hostname.
        local_address: String,
        /// Local port.
        local_port: u16,
        /// Remote IP address.
        remote_ip: IpAddr,
        /// Remote port.
        remote_port: u16,
        /// Protocol
        protocol: Protocol,
    },
    /// Transport tuple without remote endpoint info.
    #[serde(rename_all = "camelCase")]
    LocalOnly {
        /// Local IP address or hostname.
        local_address: String,
        /// Local port.
        local_port: u16,
        /// Protocol
        protocol: Protocol,
    },
}

impl TransportTuple {
    /// Local IP address or hostname.
    pub fn local_address(&self) -> &String {
        let (Self::WithRemote { local_address, .. } | Self::LocalOnly { local_address, .. }) = self;
        local_address
    }

    /// Local port.
    pub fn local_port(&self) -> u16 {
        let (Self::WithRemote { local_port, .. } | Self::LocalOnly { local_port, .. }) = self;
        *local_port
    }

    /// Protocol.
    pub fn protocol(&self) -> Protocol {
        let (Self::WithRemote { protocol, .. } | Self::LocalOnly { protocol, .. }) = self;
        *protocol
    }

    /// Remote IP address.
    pub fn remote_ip(&self) -> Option<IpAddr> {
        if let TransportTuple::WithRemote { remote_ip, .. } = self {
            Some(*remote_ip)
        } else {
            None
        }
    }

    /// Remote port.
    pub fn remote_port(&self) -> Option<u16> {
        if let TransportTuple::WithRemote { remote_port, .. } = self {
            Some(*remote_port)
        } else {
            None
        }
    }

    pub(crate) fn from_fbs(tuple: &transport::Tuple) -> TransportTuple {
        match &tuple.remote_ip {
            Some(_remote_ip) => TransportTuple::WithRemote {
                local_address: tuple
                    .local_address
                    .parse()
                    .expect("Error parsing local address"),
                local_port: tuple.local_port,
                remote_ip: tuple
                    .remote_ip
                    .as_ref()
                    .unwrap()
                    .parse()
                    .expect("Error parsing remote IP address"),
                remote_port: tuple.remote_port,
                protocol: Protocol::from_fbs(tuple.protocol),
            },
            None => TransportTuple::LocalOnly {
                local_address: tuple
                    .local_address
                    .parse()
                    .expect("Error parsing local address"),
                local_port: tuple.local_port,
                protocol: Protocol::from_fbs(tuple.protocol),
            },
        }
    }
}

/// DTLS state.
#[derive(Debug, Copy, Clone, Ord, PartialOrd, Eq, PartialEq, Hash, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
pub enum DtlsState {
    /// DTLS procedures not yet initiated.
    New,
    /// DTLS connecting.
    Connecting,
    /// DTLS successfully connected (SRTP keys already extracted).
    Connected,
    /// DTLS connection failed.
    Failed,
    /// DTLS state when the `transport` has been closed.
    Closed,
}

impl DtlsState {
    pub(crate) fn from_fbs(state: web_rtc_transport::DtlsState) -> Self {
        match state {
            web_rtc_transport::DtlsState::New => DtlsState::New,
            web_rtc_transport::DtlsState::Connecting => DtlsState::Connecting,
            web_rtc_transport::DtlsState::Connected => DtlsState::Connected,
            web_rtc_transport::DtlsState::Failed => DtlsState::Failed,
            web_rtc_transport::DtlsState::Closed => DtlsState::Closed,
        }
    }
}

/// SCTP state.
#[derive(Debug, Copy, Clone, Ord, PartialOrd, Eq, PartialEq, Hash, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
pub enum SctpState {
    /// SCTP procedures not yet initiated.
    New,
    /// SCTP connecting.
    Connecting,
    /// SCTP successfully connected.
    Connected,
    /// SCTP connection failed.
    Failed,
    /// SCTP state when the transport has been closed.
    Closed,
}

impl SctpState {
    pub(crate) fn from_fbs(state: &sctp_association::SctpState) -> Self {
        match state {
            sctp_association::SctpState::New => Self::New,
            sctp_association::SctpState::Connecting => Self::Connecting,
            sctp_association::SctpState::Connected => Self::Connected,
            sctp_association::SctpState::Failed => Self::Failed,
            sctp_association::SctpState::Closed => Self::Closed,
        }
    }
}

/// DTLS role.
#[derive(Debug, Copy, Clone, Ord, PartialOrd, Eq, PartialEq, Hash, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
pub enum DtlsRole {
    /// The DTLS role is determined based on the resolved ICE role (the `Controlled` role acts as
    /// DTLS client, the `Controlling` role acts as DTLS server).
    /// Since mediasoup is a ICE Lite implementation it always behaves as ICE `Controlled`.
    Auto,
    /// DTLS client role.
    Client,
    /// DTLS server role.
    Server,
}

impl DtlsRole {
    pub(crate) fn to_fbs(self) -> web_rtc_transport::DtlsRole {
        match self {
            DtlsRole::Auto => web_rtc_transport::DtlsRole::Auto,
            DtlsRole::Client => web_rtc_transport::DtlsRole::Client,
            DtlsRole::Server => web_rtc_transport::DtlsRole::Server,
        }
    }

    pub(crate) fn from_fbs(role: web_rtc_transport::DtlsRole) -> Self {
        match role {
            web_rtc_transport::DtlsRole::Auto => DtlsRole::Auto,
            web_rtc_transport::DtlsRole::Client => DtlsRole::Client,
            web_rtc_transport::DtlsRole::Server => DtlsRole::Server,
        }
    }
}

impl Default for DtlsRole {
    fn default() -> Self {
        Self::Auto
    }
}

/// The hash function algorithm (as defined in the "Hash function Textual Names" registry initially
/// specified in [RFC 4572](https://tools.ietf.org/html/rfc4572#section-8) Section 8) and its
/// corresponding certificate fingerprint value.
#[derive(Copy, Clone, PartialOrd, Eq, PartialEq)]
pub enum DtlsFingerprint {
    /// sha-1
    Sha1 {
        /// Certificate fingerprint value.
        value: [u8; 20],
    },
    /// sha-224
    Sha224 {
        /// Certificate fingerprint value.
        value: [u8; 28],
    },
    /// sha-256
    Sha256 {
        /// Certificate fingerprint value.
        value: [u8; 32],
    },
    /// sha-384
    Sha384 {
        /// Certificate fingerprint value.
        value: [u8; 48],
    },
    /// sha-512
    Sha512 {
        /// Certificate fingerprint value.
        value: [u8; 64],
    },
}

fn hex_as_bytes<const N: usize>(input: &str) -> [u8; N] {
    let mut output = [0_u8; N];
    for (i, o) in input.split(':').zip(&mut output.iter_mut()) {
        *o = u8::from_str_radix(i, 16).unwrap_or_else(|error| {
            panic!("Failed to parse value {i} as series of hex bytes: {error}")
        });
    }

    output
}

impl DtlsFingerprint {
    pub(crate) fn to_fbs(self) -> web_rtc_transport::Fingerprint {
        match self {
            DtlsFingerprint::Sha1 { .. } => web_rtc_transport::Fingerprint {
                algorithm: web_rtc_transport::FingerprintAlgorithm::Sha1,
                value: self.value_string(),
            },
            DtlsFingerprint::Sha224 { .. } => web_rtc_transport::Fingerprint {
                algorithm: web_rtc_transport::FingerprintAlgorithm::Sha224,
                value: self.value_string(),
            },
            DtlsFingerprint::Sha256 { .. } => web_rtc_transport::Fingerprint {
                algorithm: web_rtc_transport::FingerprintAlgorithm::Sha256,
                value: self.value_string(),
            },
            DtlsFingerprint::Sha384 { .. } => web_rtc_transport::Fingerprint {
                algorithm: web_rtc_transport::FingerprintAlgorithm::Sha384,
                value: self.value_string(),
            },
            DtlsFingerprint::Sha512 { .. } => web_rtc_transport::Fingerprint {
                algorithm: web_rtc_transport::FingerprintAlgorithm::Sha512,
                value: self.value_string(),
            },
        }
    }

    pub(crate) fn from_fbs(fingerprint: &web_rtc_transport::Fingerprint) -> DtlsFingerprint {
        match fingerprint.algorithm {
            web_rtc_transport::FingerprintAlgorithm::Sha1 => {
                let value_result = hex_as_bytes::<20>(fingerprint.value.as_str());

                DtlsFingerprint::Sha1 {
                    value: value_result,
                }
            }
            web_rtc_transport::FingerprintAlgorithm::Sha224 => {
                let value_result = hex_as_bytes::<28>(fingerprint.value.as_str());

                DtlsFingerprint::Sha224 {
                    value: value_result,
                }
            }
            web_rtc_transport::FingerprintAlgorithm::Sha256 => {
                let value_result = hex_as_bytes::<32>(fingerprint.value.as_str());

                DtlsFingerprint::Sha256 {
                    value: value_result,
                }
            }
            web_rtc_transport::FingerprintAlgorithm::Sha384 => {
                let value_result = hex_as_bytes::<48>(fingerprint.value.as_str());

                DtlsFingerprint::Sha384 {
                    value: value_result,
                }
            }
            web_rtc_transport::FingerprintAlgorithm::Sha512 => {
                let value_result = hex_as_bytes::<64>(fingerprint.value.as_str());

                DtlsFingerprint::Sha512 {
                    value: value_result,
                }
            }
        }
    }
}

impl fmt::Debug for DtlsFingerprint {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let name = match self {
            DtlsFingerprint::Sha1 { .. } => "Sha1",
            DtlsFingerprint::Sha224 { .. } => "Sha224",
            DtlsFingerprint::Sha256 { .. } => "Sha256",
            DtlsFingerprint::Sha384 { .. } => "Sha384",
            DtlsFingerprint::Sha512 { .. } => "Sha512",
        };
        f.debug_struct(name)
            .field("value", &self.value_string())
            .finish()
    }
}

impl Serialize for DtlsFingerprint {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let mut rtcp_feedback = serializer.serialize_struct("DtlsFingerprint", 2)?;
        rtcp_feedback.serialize_field("algorithm", self.algorithm_str())?;
        rtcp_feedback.serialize_field("value", &self.value_string())?;
        rtcp_feedback.end()
    }
}

impl<'de> Deserialize<'de> for DtlsFingerprint {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        #[derive(Deserialize)]
        #[serde(field_identifier, rename_all = "lowercase")]
        enum Field {
            Algorithm,
            Value,
        }

        struct DtlsFingerprintVisitor;

        impl<'de> Visitor<'de> for DtlsFingerprintVisitor {
            type Value = DtlsFingerprint;

            fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
                formatter.write_str(
                    r#"DTLS fingerprint algorithm and value like {"algorithm": "sha-256", "value": "1B:EA:BF:33:B8:11:26:6D:91:AD:1B:A0:16:FD:5D:60:59:33:F7:46:A3:BA:99:2A:1D:04:99:A6:F2:C6:2D:43"}"#,
                )
            }

            fn visit_map<V>(self, mut map: V) -> Result<Self::Value, V::Error>
            where
                V: MapAccess<'de>,
            {
                fn parse_as_bytes(input: &str, output: &mut [u8]) -> Result<(), String> {
                    for (i, v) in output.iter_mut().enumerate() {
                        *v = u8::from_str_radix(&input[i * 3..(i * 3) + 2], 16).map_err(
                            |error| {
                                format!(
                                    "Failed to parse value {input} as series of hex bytes: {error}"
                                )
                            },
                        )?;
                    }

                    Ok(())
                }

                let mut algorithm = None::<Cow<'_, str>>;
                let mut value = None::<Cow<'_, str>>;
                while let Some(key) = map.next_key()? {
                    match key {
                        Field::Algorithm => {
                            if algorithm.is_some() {
                                return Err(de::Error::duplicate_field("algorithm"));
                            }
                            algorithm = Some(map.next_value()?);
                        }
                        Field::Value => {
                            if value.is_some() {
                                return Err(de::Error::duplicate_field("value"));
                            }
                            value = map.next_value()?;
                        }
                    }
                }
                let algorithm = algorithm.ok_or_else(|| de::Error::missing_field("algorithm"))?;
                let value = value.ok_or_else(|| de::Error::missing_field("value"))?;

                match algorithm.as_ref() {
                    "sha-1" => {
                        if value.len() == (20 * 3 - 1) {
                            let mut value_result = [0_u8; 20];
                            parse_as_bytes(value.as_ref(), &mut value_result)
                                .map_err(de::Error::custom)?;

                            Ok(DtlsFingerprint::Sha1 {
                                value: value_result,
                            })
                        } else {
                            Err(de::Error::custom(
                                "Value doesn't have correct length for SHA-1",
                            ))
                        }
                    }
                    "sha-224" => {
                        if value.len() == (28 * 3 - 1) {
                            let mut value_result = [0_u8; 28];
                            parse_as_bytes(value.as_ref(), &mut value_result)
                                .map_err(de::Error::custom)?;

                            Ok(DtlsFingerprint::Sha224 {
                                value: value_result,
                            })
                        } else {
                            Err(de::Error::custom(
                                "Value doesn't have correct length for SHA-224",
                            ))
                        }
                    }
                    "sha-256" => {
                        if value.len() == (32 * 3 - 1) {
                            let mut value_result = [0_u8; 32];
                            parse_as_bytes(value.as_ref(), &mut value_result)
                                .map_err(de::Error::custom)?;

                            Ok(DtlsFingerprint::Sha256 {
                                value: value_result,
                            })
                        } else {
                            Err(de::Error::custom(
                                "Value doesn't have correct length for SHA-256",
                            ))
                        }
                    }
                    "sha-384" => {
                        if value.len() == (48 * 3 - 1) {
                            let mut value_result = [0_u8; 48];
                            parse_as_bytes(value.as_ref(), &mut value_result)
                                .map_err(de::Error::custom)?;

                            Ok(DtlsFingerprint::Sha384 {
                                value: value_result,
                            })
                        } else {
                            Err(de::Error::custom(
                                "Value doesn't have correct length for SHA-384",
                            ))
                        }
                    }
                    "sha-512" => {
                        if value.len() == (64 * 3 - 1) {
                            let mut value_result = [0_u8; 64];
                            parse_as_bytes(value.as_ref(), &mut value_result)
                                .map_err(de::Error::custom)?;

                            Ok(DtlsFingerprint::Sha512 {
                                value: value_result,
                            })
                        } else {
                            Err(de::Error::custom(
                                "Value doesn't have correct length for SHA-512",
                            ))
                        }
                    }
                    algorithm => Err(de::Error::unknown_variant(
                        algorithm,
                        &["sha-1", "sha-224", "sha-256", "sha-384", "sha-512"],
                    )),
                }
            }
        }

        const FIELDS: &[&str] = &["algorithm", "value"];
        deserializer.deserialize_struct("DtlsFingerprint", FIELDS, DtlsFingerprintVisitor)
    }
}

impl DtlsFingerprint {
    fn value_string(&self) -> String {
        match self {
            DtlsFingerprint::Sha1 { value } => {
                format!(
                    "{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:\
                    {:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}",
                    value[0],
                    value[1],
                    value[2],
                    value[3],
                    value[4],
                    value[5],
                    value[6],
                    value[7],
                    value[8],
                    value[9],
                    value[10],
                    value[11],
                    value[12],
                    value[13],
                    value[14],
                    value[15],
                    value[16],
                    value[17],
                    value[18],
                    value[19],
                )
            }
            DtlsFingerprint::Sha224 { value } => {
                format!(
                    "{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:\
                    {:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:\
                    {:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}",
                    value[0],
                    value[1],
                    value[2],
                    value[3],
                    value[4],
                    value[5],
                    value[6],
                    value[7],
                    value[8],
                    value[9],
                    value[10],
                    value[11],
                    value[12],
                    value[13],
                    value[14],
                    value[15],
                    value[16],
                    value[17],
                    value[18],
                    value[19],
                    value[20],
                    value[21],
                    value[22],
                    value[23],
                    value[24],
                    value[25],
                    value[26],
                    value[27],
                )
            }
            DtlsFingerprint::Sha256 { value } => {
                format!(
                    "{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:\
                    {:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:\
                    {:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:\
                    {:02X}:{:02X}",
                    value[0],
                    value[1],
                    value[2],
                    value[3],
                    value[4],
                    value[5],
                    value[6],
                    value[7],
                    value[8],
                    value[9],
                    value[10],
                    value[11],
                    value[12],
                    value[13],
                    value[14],
                    value[15],
                    value[16],
                    value[17],
                    value[18],
                    value[19],
                    value[20],
                    value[21],
                    value[22],
                    value[23],
                    value[24],
                    value[25],
                    value[26],
                    value[27],
                    value[28],
                    value[29],
                    value[30],
                    value[31],
                )
            }
            DtlsFingerprint::Sha384 { value } => {
                format!(
                    "{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:\
                    {:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:\
                    {:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:\
                    {:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:\
                    {:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}",
                    value[0],
                    value[1],
                    value[2],
                    value[3],
                    value[4],
                    value[5],
                    value[6],
                    value[7],
                    value[8],
                    value[9],
                    value[10],
                    value[11],
                    value[12],
                    value[13],
                    value[14],
                    value[15],
                    value[16],
                    value[17],
                    value[18],
                    value[19],
                    value[20],
                    value[21],
                    value[22],
                    value[23],
                    value[24],
                    value[25],
                    value[26],
                    value[27],
                    value[28],
                    value[29],
                    value[30],
                    value[31],
                    value[32],
                    value[33],
                    value[34],
                    value[35],
                    value[36],
                    value[37],
                    value[38],
                    value[39],
                    value[40],
                    value[41],
                    value[42],
                    value[43],
                    value[44],
                    value[45],
                    value[46],
                    value[47],
                )
            }
            DtlsFingerprint::Sha512 { value } => {
                format!(
                    "{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:\
                    {:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:\
                    {:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:\
                    {:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:\
                    {:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:\
                    {:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:\
                    {:02X}:{:02X}:{:02X}:{:02X}",
                    value[0],
                    value[1],
                    value[2],
                    value[3],
                    value[4],
                    value[5],
                    value[6],
                    value[7],
                    value[8],
                    value[9],
                    value[10],
                    value[11],
                    value[12],
                    value[13],
                    value[14],
                    value[15],
                    value[16],
                    value[17],
                    value[18],
                    value[19],
                    value[20],
                    value[21],
                    value[22],
                    value[23],
                    value[24],
                    value[25],
                    value[26],
                    value[27],
                    value[28],
                    value[29],
                    value[30],
                    value[31],
                    value[32],
                    value[33],
                    value[34],
                    value[35],
                    value[36],
                    value[37],
                    value[38],
                    value[39],
                    value[40],
                    value[41],
                    value[42],
                    value[43],
                    value[44],
                    value[45],
                    value[46],
                    value[47],
                    value[48],
                    value[49],
                    value[50],
                    value[51],
                    value[52],
                    value[53],
                    value[54],
                    value[55],
                    value[56],
                    value[57],
                    value[58],
                    value[59],
                    value[60],
                    value[61],
                    value[62],
                    value[63],
                )
            }
        }
    }

    fn algorithm_str(&self) -> &'static str {
        match self {
            DtlsFingerprint::Sha1 { .. } => "sha-1",
            DtlsFingerprint::Sha224 { .. } => "sha-224",
            DtlsFingerprint::Sha256 { .. } => "sha-256",
            DtlsFingerprint::Sha384 { .. } => "sha-384",
            DtlsFingerprint::Sha512 { .. } => "sha-512",
        }
    }
}

/// DTLS parameters.
#[derive(Debug, Clone, PartialOrd, Eq, PartialEq, Deserialize, Serialize)]
pub struct DtlsParameters {
    /// DTLS role.
    pub role: DtlsRole,
    /// DTLS fingerprints.
    pub fingerprints: Vec<DtlsFingerprint>,
}

impl DtlsParameters {
    pub(crate) fn to_fbs(&self) -> web_rtc_transport::DtlsParameters {
        web_rtc_transport::DtlsParameters {
            role: self.role.to_fbs(),
            fingerprints: self
                .fingerprints
                .iter()
                .map(|fingerprint| fingerprint.to_fbs())
                .collect(),
        }
    }

    pub(crate) fn from_fbs(parameters: web_rtc_transport::DtlsParameters) -> DtlsParameters {
        DtlsParameters {
            role: DtlsRole::from_fbs(parameters.role),
            fingerprints: parameters
                .fingerprints
                .iter()
                .map(DtlsFingerprint::from_fbs)
                .collect(),
        }
    }
}

/// Trace event direction
#[derive(Debug, Copy, Clone, Deserialize, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum TraceEventDirection {
    /// In
    In,
    /// Out
    Out,
}

impl TraceEventDirection {
    pub(crate) fn from_fbs(event_type: common::TraceDirection) -> Self {
        match event_type {
            common::TraceDirection::DirectionIn => TraceEventDirection::In,
            common::TraceDirection::DirectionOut => TraceEventDirection::Out,
        }
    }
}

/// Container used for sending/receiving messages using `DirectTransport` data producers and data
/// consumers.
#[derive(Debug, Clone)]
pub enum WebRtcMessage<'a> {
    /// String
    String(Cow<'a, [u8]>),
    /// Binary
    Binary(Cow<'a, [u8]>),
    /// EmptyString
    EmptyString,
    /// EmptyBinary
    EmptyBinary,
}

impl<'a> WebRtcMessage<'a> {
    // +------------------------------------+-----------+
    // | Value                              | SCTP PPID |
    // +------------------------------------+-----------+
    // | WebRTC String                      | 51        |
    // | WebRTC Binary Partial (Deprecated) | 52        |
    // | WebRTC Binary                      | 53        |
    // | WebRTC String Partial (Deprecated) | 54        |
    // | WebRTC String Empty                | 56        |
    // | WebRTC Binary Empty                | 57        |
    // +------------------------------------+-----------+

    pub(crate) fn new(ppid: u32, payload: Cow<'a, [u8]>) -> Result<Self, u32> {
        match ppid {
            51 => Ok(WebRtcMessage::String(payload)),
            53 => Ok(WebRtcMessage::Binary(payload)),
            56 => Ok(WebRtcMessage::EmptyString),
            57 => Ok(WebRtcMessage::EmptyBinary),
            ppid => Err(ppid),
        }
    }

    pub(crate) fn into_ppid_and_payload(self) -> (u32, Cow<'a, [u8]>) {
        match self {
            WebRtcMessage::String(binary) => (51_u32, binary),
            WebRtcMessage::Binary(binary) => (53_u32, binary),
            WebRtcMessage::EmptyString => (56_u32, Cow::from(vec![0_u8])),
            WebRtcMessage::EmptyBinary => (57_u32, Cow::from(vec![0_u8])),
        }
    }

    /// Convert to owned message
    pub fn into_owned(self) -> OwnedWebRtcMessage {
        match self {
            WebRtcMessage::String(binary) => OwnedWebRtcMessage::String(binary.into_owned()),
            WebRtcMessage::Binary(binary) => OwnedWebRtcMessage::Binary(binary.into_owned()),
            WebRtcMessage::EmptyString => OwnedWebRtcMessage::EmptyString,
            WebRtcMessage::EmptyBinary => OwnedWebRtcMessage::EmptyBinary,
        }
    }
}

/// Similar to WebRtcMessage but represents
/// messages that have ownership over the data
#[derive(Debug, Clone)]
pub enum OwnedWebRtcMessage {
    /// String
    String(Vec<u8>),
    /// Binary
    Binary(Vec<u8>),
    /// EmptyString
    EmptyString,
    /// EmptyBinary
    EmptyBinary,
}

/// RTP packet info in trace event.
#[derive(Debug, Clone, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct RtpPacketTraceInfo {
    /// RTP payload type.
    pub payload_type: u8,
    /// Sequence number.
    pub sequence_number: u16,
    /// Timestamp.
    pub timestamp: u32,
    /// Whether packet has marker or not.
    pub marker: bool,
    /// RTP stream SSRC.
    pub ssrc: u32,
    /// Whether packet contains a key frame.
    pub is_key_frame: bool,
    /// Packet size.
    pub size: u64,
    /// Payload size.
    pub payload_size: u64,
    /// The spatial layer index (from 0 to N).
    pub spatial_layer: u8,
    /// The temporal layer index (from 0 to N).
    pub temporal_layer: u8,
    /// The MID RTP extension value as defined in the BUNDLE specification
    pub mid: Option<String>,
    /// RTP stream RID value.
    pub rid: Option<String>,
    /// RTP stream RRID value.
    pub rrid: Option<String>,
    /// Transport-wide sequence number.
    pub wide_sequence_number: Option<u16>,
    /// Whether this is an RTX packet.
    #[serde(default)]
    pub is_rtx: bool,
}

impl RtpPacketTraceInfo {
    pub(crate) fn from_fbs(rtp_packet: rtp_packet::Dump, is_rtx: bool) -> Self {
        Self {
            payload_type: rtp_packet.payload_type,
            sequence_number: rtp_packet.sequence_number,
            timestamp: rtp_packet.timestamp,
            marker: rtp_packet.marker,
            ssrc: rtp_packet.ssrc,
            is_key_frame: rtp_packet.is_key_frame,
            size: rtp_packet.size,
            payload_size: rtp_packet.payload_size,
            spatial_layer: rtp_packet.spatial_layer,
            temporal_layer: rtp_packet.temporal_layer,
            mid: rtp_packet.mid,
            rid: rtp_packet.rid,
            rrid: rtp_packet.rrid,
            wide_sequence_number: rtp_packet.wide_sequence_number,
            is_rtx,
        }
    }
}

/// SSRC info in trace event.
#[derive(Debug, Copy, Clone, Deserialize, Serialize)]
pub struct SsrcTraceInfo {
    /// RTP stream SSRC.
    pub ssrc: u32,
}

/// Bandwidth estimation type.
#[derive(Debug, Copy, Clone, Deserialize, Serialize)]
pub enum BweType {
    /// Transport-wide Congestion Control.
    #[serde(rename = "transport-cc")]
    TransportCc,
    /// Receiver Estimated Maximum Bitrate.
    #[serde(rename = "remb")]
    Remb,
}

impl BweType {
    pub(crate) fn from_fbs(info: transport::BweType) -> Self {
        match info {
            transport::BweType::TransportCc => BweType::TransportCc,
            transport::BweType::Remb => BweType::Remb,
        }
    }
}

/// BWE info in trace event.
#[derive(Debug, Copy, Clone, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct BweTraceInfo {
    /// Bandwidth estimation type.
    r#type: BweType,
    /// Desired bitrate
    desired_bitrate: u32,
    /// Effective desired bitrate.
    effective_desired_bitrate: u32,
    /// Min bitrate.
    min_bitrate: u32,
    /// Max bitrate.
    max_bitrate: u32,
    /// Start bitrate.
    start_bitrate: u32,
    /// Max padding bitrate.
    max_padding_bitrate: u32,
    /// Available bitrate.
    available_bitrate: u32,
}

impl BweTraceInfo {
    pub(crate) fn from_fbs(info: transport::BweTraceInfo) -> Self {
        Self {
            r#type: BweType::from_fbs(info.bwe_type),
            desired_bitrate: info.desired_bitrate,
            effective_desired_bitrate: info.effective_desired_bitrate,
            min_bitrate: info.min_bitrate,
            max_bitrate: info.max_bitrate,
            start_bitrate: info.start_bitrate,
            max_padding_bitrate: info.max_padding_bitrate,
            available_bitrate: info.available_bitrate,
        }
    }
}

/// RTCP Sender Report info in trace event.
#[derive(Debug, Copy, Clone, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct SrTraceInfo {
    /// Stream SSRC
    ssrc: u32,
    /// NTP : most significant word
    ntp_sec: u32,
    /// NTP : least significant word
    ntp_frac: u32,
    /// RTP timestamp
    rtp_ts: u32,
    /// Sender packet count
    packet_count: u32,
    /// Sender octet count
    octet_count: u32,
}

impl SrTraceInfo {
    pub(crate) fn from_fbs(info: producer::SrTraceInfo) -> Self {
        Self {
            ssrc: info.ssrc,
            ntp_sec: info.ntp_sec,
            ntp_frac: info.ntp_frac,
            rtp_ts: info.rtp_ts,
            packet_count: info.packet_count,
            octet_count: info.octet_count,
        }
    }
}
