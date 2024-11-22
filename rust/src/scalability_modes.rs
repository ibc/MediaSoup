//! Scalability mode.

#[cfg(test)]
mod tests;

use once_cell::sync::OnceCell;
use regex::Regex;
use serde::{de, Deserialize, Deserializer, Serialize, Serializer};
use std::fmt;
use std::num::NonZeroU8;
use std::str::FromStr;
use thiserror::Error;

/// Scalability mode.
///
/// Most modes match [webrtc-svc](https://w3c.github.io/webrtc-svc/), but custom ones are also
/// supported by mediasoup.
#[derive(Debug, Clone, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub enum ScalabilityMode {
    /// No scalability used, there is just one spatial and one temporal layer.
    None,
    /// L1T2.
    L1T2,
    /// L1T2h.
    L1T2h,
    /// L1T3.
    L1T3,
    /// L1T3h.
    L1T3h,
    /// L2T1.
    L2T1,
    /// L2T1h.
    L2T1h,
    /// L2T1_KEY.
    L2T1Key,
    /// L2T2.
    L2T2,
    /// L2T2h.
    L2T2h,
    /// L2T2_KEY.
    L2T2Key,
    /// L2T2_KEY_SHIFT.
    L2T2KeyShift,
    /// L2T3.
    L2T3,
    /// L2T3h.
    L2T3h,
    /// L2T3_KEY.
    L2T3Key,
    /// L2T3_KEY_SHIFT.
    L2T3KeyShift,
    /// L3T1.
    L3T1,
    /// L3T1h.
    L3T1h,
    /// L3T1_KEY.
    L3T1Key,
    /// L3T2.
    L3T2,
    /// L3T2h.
    L3T2h,
    /// L3T2_KEY.
    L3T2Key,
    /// L3T2_KEY_SHIFT.
    L3T2KeyShift,
    /// L3T3.
    L3T3,
    /// L3T3h.
    L3T3h,
    /// L3T3_KEY.
    L3T3Key,
    /// L3T3_KEY_SHIFT.
    L3T3KeyShift,
    /// S2T1.
    S2T1,
    /// S2T1h.
    S2T1h,
    /// S2T2.
    S2T2,
    /// S2T2h.
    S2T2h,
    /// S2T3.
    S2T3,
    /// S2T3h.
    S2T3h,
    /// S3T1.
    S3T1,
    /// S3T1h.
    S3T1h,
    /// S3T2.
    S3T2,
    /// S3T2h.
    S3T2h,
    /// S3T3.
    S3T3,
    /// S3T3h.
    S3T3h,
    /// Custom scalability mode not defined in [webrtc-svc](https://w3c.github.io/webrtc-svc/).
    Custom {
        /// Scalability mode as string
        scalability_mode: String,
        /// Number of spatial layers.
        spatial_layers: NonZeroU8,
        /// Number of temporal layers.
        temporal_layers: NonZeroU8,
        /// K-SVC mode.
        ksvc: bool,
    },
}

impl Default for ScalabilityMode {
    fn default() -> Self {
        Self::None
    }
}

/// Error that caused [`ScalabilityMode`] parsing error.
#[derive(Debug, Error, Eq, PartialEq)]
pub enum ParseScalabilityModeError {
    /// Invalid input string
    #[error("Invalid Scalability Mode input string")]
    InvalidInput,
}

impl FromStr for ScalabilityMode {
    type Err = ParseScalabilityModeError;

    fn from_str(scalability_mode: &str) -> Result<Self, Self::Err> {
        Ok(match scalability_mode {
            "S1T1" => Self::None,
            "L1T2" => Self::L1T2,
            "L1T2h" => Self::L1T2h,
            "L1T3" => Self::L1T3,
            "L1T3h" => Self::L1T3h,
            "L2T1" => Self::L2T1,
            "L2T1h" => Self::L2T1h,
            "L2T1_KEY" => Self::L2T1Key,
            "L2T2" => Self::L2T2,
            "L2T2h" => Self::L2T2h,
            "L2T2_KEY" => Self::L2T2Key,
            "L2T2_KEY_SHIFT" => Self::L2T2KeyShift,
            "L2T3" => Self::L2T3,
            "L2T3h" => Self::L2T3h,
            "L2T3_KEY" => Self::L2T3Key,
            "L2T3_KEY_SHIFT" => Self::L2T3KeyShift,
            "L3T1" => Self::L3T1,
            "L3T1h" => Self::L3T1h,
            "L3T1_KEY" => Self::L3T1Key,
            "L3T2" => Self::L3T2,
            "L3T2h" => Self::L3T2h,
            "L3T2_KEY" => Self::L3T2Key,
            "L3T2_KEY_SHIFT" => Self::L3T2KeyShift,
            "L3T3" => Self::L3T3,
            "L3T3h" => Self::L3T3h,
            "L3T3_KEY" => Self::L3T3Key,
            "L3T3_KEY_SHIFT" => Self::L3T3KeyShift,
            "S2T1" => Self::S2T1,
            "S2T1h" => Self::S2T1h,
            "S2T2" => Self::S2T2,
            "S2T2h" => Self::S2T2h,
            "S2T3" => Self::S2T3,
            "S2T3h" => Self::S2T3h,
            "S3T1" => Self::S3T1,
            "S3T1h" => Self::S3T1h,
            "S3T2" => Self::S3T2,
            "S3T2h" => Self::S3T2h,
            "S3T3" => Self::S3T3,
            "S3T3h" => Self::S3T3h,
            scalability_mode => {
                static SCALABILITY_MODE_REGEX: OnceCell<Regex> = OnceCell::new();

                SCALABILITY_MODE_REGEX
                    .get_or_init(|| Regex::new(r"^[LS]([1-9][0-9]?)T([1-9][0-9]?)(_KEY)?").unwrap())
                    .captures(scalability_mode)
                    .map(|captures| Self::Custom {
                        scalability_mode: scalability_mode.to_string(),
                        spatial_layers: captures.get(1).unwrap().as_str().parse().unwrap(),
                        temporal_layers: captures.get(2).unwrap().as_str().parse().unwrap(),
                        ksvc: captures.get(3).is_some(),
                    })
                    .ok_or(ParseScalabilityModeError::InvalidInput)?
            }
        })
    }
}

impl std::fmt::Display for ScalabilityMode {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.as_str())
    }
}

impl ScalabilityMode {
    /// Returns true if there is no scalability.
    pub fn is_none(&self) -> bool {
        matches!(self, Self::None)
    }

    /// Number of spatial layers.
    pub fn spatial_layers(&self) -> NonZeroU8 {
        match self {
            Self::None | Self::L1T2 | Self::L1T2h | Self::L1T3 | Self::L1T3h => {
                NonZeroU8::new(1).unwrap()
            }
            Self::L2T1
            | Self::L2T1h
            | Self::L2T1Key
            | Self::L2T2
            | Self::L2T2h
            | Self::L2T2Key
            | Self::L2T2KeyShift
            | Self::L2T3
            | Self::L2T3h
            | Self::L2T3Key
            | Self::L2T3KeyShift
            | Self::S2T1
            | Self::S2T1h
            | Self::S2T2
            | Self::S2T2h
            | Self::S2T3
            | Self::S2T3h => NonZeroU8::new(2).unwrap(),
            Self::L3T1
            | Self::L3T1h
            | Self::L3T1Key
            | Self::L3T2
            | Self::L3T2h
            | Self::L3T2Key
            | Self::L3T2KeyShift
            | Self::L3T3
            | Self::L3T3h
            | Self::L3T3Key
            | Self::L3T3KeyShift
            | Self::S3T1
            | Self::S3T1h
            | Self::S3T2
            | Self::S3T2h
            | Self::S3T3
            | Self::S3T3h => NonZeroU8::new(3).unwrap(),
            Self::Custom { spatial_layers, .. } => *spatial_layers,
        }
    }

    /// Number of temporal layers.
    pub fn temporal_layers(&self) -> NonZeroU8 {
        match self {
            Self::None
            | Self::L2T1
            | Self::L2T1h
            | Self::L2T1Key
            | Self::L3T1
            | Self::L3T1h
            | Self::L3T1Key
            | Self::S2T1
            | Self::S2T1h
            | Self::S3T1
            | Self::S3T1h => NonZeroU8::new(1).unwrap(),
            Self::L1T2
            | Self::L1T2h
            | Self::L2T2
            | Self::L2T2h
            | Self::L2T2Key
            | Self::L2T2KeyShift
            | Self::L3T2
            | Self::L3T2h
            | Self::L3T2Key
            | Self::L3T2KeyShift
            | Self::S2T2
            | Self::S2T2h
            | Self::S3T2
            | Self::S3T2h => NonZeroU8::new(2).unwrap(),
            Self::L1T3
            | Self::L1T3h
            | Self::L2T3
            | Self::L2T3h
            | Self::L2T3Key
            | Self::L2T3KeyShift
            | Self::L3T3
            | Self::L3T3h
            | Self::L3T3Key
            | Self::L3T3KeyShift
            | Self::S2T3
            | Self::S2T3h
            | Self::S3T3
            | Self::S3T3h => NonZeroU8::new(3).unwrap(),
            Self::Custom {
                temporal_layers, ..
            } => *temporal_layers,
        }
    }

    /// K-SVC mode.
    pub fn ksvc(&self) -> bool {
        match self {
            Self::L2T2Key
            | Self::L2T2KeyShift
            | Self::L2T3Key
            | Self::L2T3KeyShift
            | Self::L3T1Key
            | Self::L3T2Key
            | Self::L3T2KeyShift
            | Self::L3T3Key
            | Self::L3T3KeyShift => true,
            Self::Custom { ksvc, .. } => *ksvc,
            _ => false,
        }
    }

    /// String representation of scalability mode.
    pub fn as_str(&self) -> &str {
        match self {
            Self::None => "S1T1",
            Self::L1T2 => "L1T2",
            Self::L1T2h => "L1T2h",
            Self::L1T3 => "L1T3",
            Self::L1T3h => "L1T3h",
            Self::L2T1 => "L2T1",
            Self::L2T1h => "L2T1h",
            Self::L2T1Key => "L2T1_KEY",
            Self::L2T2 => "L2T2",
            Self::L2T2h => "L2T2h",
            Self::L2T2Key => "L2T2_KEY",
            Self::L2T2KeyShift => "L2T2_KEY_SHIFT",
            Self::L2T3 => "L2T3",
            Self::L2T3h => "L2T3h",
            Self::L2T3Key => "L2T3_KEY",
            Self::L2T3KeyShift => "L2T3_KEY_SHIFT",
            Self::L3T1 => "L3T1",
            Self::L3T1h => "L3T1h",
            Self::L3T1Key => "L3T1_KEY",
            Self::L3T2 => "L3T2",
            Self::L3T2h => "L3T2h",
            Self::L3T2Key => "L3T2_KEY",
            Self::L3T2KeyShift => "L3T2_KEY_SHIFT",
            Self::L3T3 => "L3T3",
            Self::L3T3h => "L3T3h",
            Self::L3T3Key => "L3T3_KEY",
            Self::L3T3KeyShift => "L3T3_KEY_SHIFT",
            Self::S2T1 => "S2T1",
            Self::S2T1h => "S2T1h",
            Self::S2T2 => "S2T2",
            Self::S2T2h => "S2T2h",
            Self::S2T3 => "S2T3",
            Self::S2T3h => "S2T3h",
            Self::S3T1 => "S3T1",
            Self::S3T1h => "S3T1h",
            Self::S3T2 => "S3T2",
            Self::S3T2h => "S3T2h",
            Self::S3T3 => "S3T3",
            Self::S3T3h => "S3T3h",
            Self::Custom {
                scalability_mode, ..
            } => scalability_mode,
        }
    }
}

impl<'de> Deserialize<'de> for ScalabilityMode {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct ScalabilityModeVisitor;

        impl<'de> de::Visitor<'de> for ScalabilityModeVisitor {
            type Value = ScalabilityMode;

            fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
                formatter.write_str(r#"Scalability mode string like "S1T3""#)
            }

            #[inline]
            fn visit_none<E>(self) -> Result<Self::Value, E>
            where
                E: de::Error,
            {
                Ok(ScalabilityMode::None)
            }

            fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
            where
                E: de::Error,
            {
                v.parse().map_err(de::Error::custom)
            }
        }

        deserializer.deserialize_str(ScalabilityModeVisitor)
    }
}

impl Serialize for ScalabilityMode {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        serializer.serialize_str(self.as_str())
    }
}
