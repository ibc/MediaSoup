#[doc(hidden)]
#[macro_export]
macro_rules! uuid_based_wrapper_type {
    (
        $(#[$outer:meta])*
        $struct_name: ident
    ) => {
        $(#[$outer])*
        #[derive(
            Debug,
            Copy,
            Clone,
            serde::Deserialize,
            serde::Serialize,
            Hash,
            Ord,
            PartialOrd,
            Eq,
            PartialEq,
        )]
        pub struct $struct_name(::uuid::Uuid);

        impl std::fmt::Display for $struct_name {
            fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
                std::fmt::Display::fmt(&self.0, f)
            }
        }

        impl ::std::str::FromStr for $struct_name {
            type Err = ::uuid::Error;

            fn from_str(s: &str) -> Result<Self, Self::Err> {
                ::uuid::Uuid::from_str(s).map(Self)
            }
        }

        impl From<$struct_name> for ::uuid::Uuid {
            fn from(id: $struct_name) -> Self {
                id.0
            }
        }

        impl $struct_name {
            pub(super) fn new() -> Self {
                $struct_name(::uuid::Uuid::new_v4())
            }
        }

        impl From<$struct_name> for $crate::worker::SubscriptionTarget {
            fn from(id: $struct_name) -> Self {
                Self::Uuid(id.0)
            }
        }
    };
}
