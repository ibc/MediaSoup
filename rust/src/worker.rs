// TODO: This is Unix-specific and doesn't support Windows in any way
mod utils;

use crate::worker::utils::WorkerChannels;
use async_executor::Executor;
use async_process::{Child, Command, Stdio};
use log::debug;
use std::ffi::OsString;
use std::path::PathBuf;
use std::sync::Arc;
use std::{env, io};

#[derive(Debug, Copy, Clone)]
pub enum WorkerLogLevel {
    Debug,
    Warn,
    Error,
    None,
}

impl Default for WorkerLogLevel {
    fn default() -> Self {
        Self::Error
    }
}

impl WorkerLogLevel {
    fn as_str(&self) -> &'static str {
        match self {
            Self::Debug => "debug",
            Self::Warn => "warn",
            Self::Error => "error",
            Self::None => "none",
        }
    }
}

#[derive(Debug, Copy, Clone)]
pub enum WorkerLogTag {
    Info,
    Ice,
    Dtls,
    Rtp,
    Srtp,
    Rtcp,
    Rtx,
    Bwe,
    Score,
    Simulcast,
    Svc,
    Sctp,
    Message,
}

impl WorkerLogTag {
    fn as_str(&self) -> &'static str {
        match self {
            Self::Info => "info",
            Self::Ice => "ice",
            Self::Dtls => "dtls",
            Self::Rtp => "rtp",
            Self::Srtp => "srtp",
            Self::Rtcp => "rtcp",
            Self::Rtx => "rtx",
            Self::Bwe => "bwe",
            Self::Score => "score",
            Self::Simulcast => "simulcast",
            Self::Svc => "svc",
            Self::Sctp => "sctp",
            Self::Message => "message",
        }
    }
}

#[derive(Debug, Clone)]
pub struct WorkerSettings {
    /// Logging level for logs generated by the media worker subprocesses (check the Debugging
    /// documentation). Default `WorkerLogLevel::Error`
    pub log_level: WorkerLogLevel,
    /// Log tags for debugging. Check the meaning of each available tag in the Debugging
    /// documentation.
    pub log_tags: Vec<WorkerLogTag>,
    /// Minimum RTC port for ICE, DTLS, RTP, etc. Default 10000.
    pub rtc_min_port: u16,
    /// Maximum RTC port for ICE, DTLS, RTP, etc. Default 59999.
    pub rtc_max_port: u16,
    // TODO: Should these 2 be combined together?
    /// Path to the DTLS public certificate file in PEM format. If unset, a certificate is
    /// dynamically created.
    pub dtls_certificate_file: Option<PathBuf>,
    /// Path to the DTLS certificate private key file in PEM format. If unset, a certificate is
    /// dynamically created.
    pub dtls_private_key_file: Option<PathBuf>,
}

impl Default for WorkerSettings {
    fn default() -> Self {
        Self {
            log_level: WorkerLogLevel::default(),
            log_tags: Vec::new(),
            rtc_min_port: 10000,
            rtc_max_port: 59999,
            dtls_certificate_file: None,
            dtls_private_key_file: None,
        }
    }
}

#[derive(Debug, Copy, Clone)]
struct WorkerResourceUsage {
    /// User CPU time used (in ms).
    pub ru_utime: u64,
    /// System CPU time used (in ms).
    pub ru_stime: u64,
    /// Maximum resident set size.
    pub ru_maxrss: u64,
    /// Integral shared memory size.
    pub ru_ixrss: u64,
    /// Integral unshared data size.
    pub ru_idrss: u64,
    /// Integral unshared stack size.
    pub ru_isrss: u64,
    /// Page reclaims (soft page faults).
    pub ru_minflt: u64,
    /// Page faults (hard page faults).
    pub ru_majflt: u64,
    /// Swaps.
    pub ru_nswap: u64,
    /// Block input operations.
    pub ru_inblock: u64,
    /// Block output operations.
    pub ru_oublock: u64,
    /// IPC messages sent.
    pub ru_msgsnd: u64,
    /// IPC messages received.
    pub ru_msgrcv: u64,
    /// Signals received.
    pub ru_nsignals: u64,
    /// Voluntary context switches.
    pub ru_nvcsw: u64,
    /// Involuntary context switches.
    pub ru_nivcsw: u64,
}

pub struct Worker {
    child: Child,
    executor: Arc<Executor>,
    pid: u32,
}

impl Worker {
    pub(super) fn new(
        executor: Arc<Executor>,
        worker_binary: PathBuf,
        WorkerSettings {
            log_level,
            log_tags,
            rtc_min_port,
            rtc_max_port,
            dtls_certificate_file,
            dtls_private_key_file,
        }: WorkerSettings,
    ) -> io::Result<Self> {
        debug!("new()");

        let mut spawn_args: Vec<OsString> = Vec::new();
        let spawn_bin: PathBuf = match env::var("MEDIASOUP_USE_VALGRIND") {
            Ok(value) if value.as_str() == "true" => {
                let binary = match env::var("MEDIASOUP_VALGRIND_BIN") {
                    Ok(binary) => binary.into(),
                    _ => "valgrind".into(),
                };

                spawn_args.push(worker_binary.into_os_string());

                binary
            }
            _ => worker_binary,
        };

        spawn_args.push(format!("--logLevel={}", log_level.as_str()).into());
        if !log_tags.is_empty() {
            let log_tags = log_tags
                .iter()
                .map(|log_tag| log_tag.as_str())
                .collect::<Vec<_>>()
                .join(",");
            spawn_args.push(format!("--logTags={}", log_tags).into());
        }
        spawn_args.push(format!("--rtcMinPort={}", rtc_min_port).into());
        spawn_args.push(format!("--rtcMaxPort={}", rtc_max_port).into());

        if let Some(dtls_certificate_file) = dtls_certificate_file {
            let mut arg = OsString::new();
            arg.push("--dtlsCertificateFile=");
            arg.push(dtls_certificate_file);
            spawn_args.push(arg);
        }
        if let Some(dtls_private_key_file) = dtls_private_key_file {
            let mut arg = OsString::new();
            arg.push("--dtlsPrivateKeyFile=");
            arg.push(dtls_private_key_file);
            spawn_args.push(arg);
        }

        debug!(
            "spawning worker process: {} {}",
            spawn_bin.to_string_lossy(),
            spawn_args
                .iter()
                .map(|arg| arg.to_string_lossy())
                .collect::<Vec<_>>()
                .join(" ")
        );

        // TODO: Spawn a child process

        let mut command = Command::new(spawn_bin);
        command
            .args(spawn_args)
            .stdin(Stdio::null())
            .stdout(Stdio::piped())
            .stderr(Stdio::piped());

        let WorkerChannels {
            channel,
            payload_channel,
        } = utils::setup_worker_channels(&executor, &mut command);

        let child = command.spawn()?;
        let pid = child.id();

        Ok(Self {
            child,
            executor,
            pid,
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn init() {
        let _ = env_logger::builder().is_test(true).try_init();
    }

    #[test]
    fn worker_test() {
        init();

        let executor = Arc::new(Executor::new());
        let worker_settings = WorkerSettings::default();
        let worker = Worker::new(
            executor,
            env::var("MEDIASOUP_WORKER_BIN")
                .map(|path| path.into())
                .unwrap_or_else(|_| "../worker/out/Release/mediasoup-worker".into()),
            worker_settings,
        )
        .unwrap();
    }
}
