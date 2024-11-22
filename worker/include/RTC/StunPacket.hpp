#ifndef MS_RTC_STUN_PACKET_HPP
#define MS_RTC_STUN_PACKET_HPP

#include "common.hpp"
#include <string>

namespace RTC
{
	class StunPacket
	{
	public:
		// STUN message class.
		enum class Class : uint16_t
		{
			REQUEST          = 0,
			INDICATION       = 1,
			SUCCESS_RESPONSE = 2,
			ERROR_RESPONSE   = 3
		};

		// STUN message method.
		enum class Method : uint16_t
		{
			BINDING = 1
		};

		// Attribute type.
		enum class Attribute : uint16_t
		{
			MAPPED_ADDRESS     = 0x0001,
			USERNAME           = 0x0006,
			MESSAGE_INTEGRITY  = 0x0008,
			ERROR_CODE         = 0x0009,
			UNKNOWN_ATTRIBUTES = 0x000A,
			REALM              = 0x0014,
			NONCE              = 0x0015,
			XOR_MAPPED_ADDRESS = 0x0020,
			PRIORITY           = 0x0024,
			USE_CANDIDATE      = 0x0025,
			SOFTWARE           = 0x8022,
			ALTERNATE_SERVER   = 0x8023,
			FINGERPRINT        = 0x8028,
			ICE_CONTROLLED     = 0x8029,
			ICE_CONTROLLING    = 0x802A,
			NOMINATION         = 0xC001
		};

		// Authentication result.
		enum class Authentication
		{
			OK           = 0,
			UNAUTHORIZED = 1,
			BAD_MESSAGE  = 2
		};

	public:
		static bool IsStun(const uint8_t* data, size_t len)
		{
			// clang-format off
			return (
				// STUN headers are 20 bytes.
				(len >= 20) &&
				// DOC: https://tools.ietf.org/html/draft-ietf-avtcore-rfc5764-mux-fixes
				(data[0] < 3) &&
				// Magic cookie must match.
				(data[4] == StunPacket::MagicCookie[0]) && (data[5] == StunPacket::MagicCookie[1]) &&
				(data[6] == StunPacket::MagicCookie[2]) && (data[7] == StunPacket::MagicCookie[3])
			);
			// clang-format on
		}
		static StunPacket* Parse(const uint8_t* data, size_t len);

	private:
		static const uint8_t MagicCookie[];

	public:
		StunPacket(
		  Class klass, Method method, const uint8_t* transactionId, const uint8_t* data, size_t size);
		~StunPacket();

		void Dump() const;
		Class GetClass() const
		{
			return this->klass;
		}
		Method GetMethod() const
		{
			return this->method;
		}
		const uint8_t* GetData() const
		{
			return this->data;
		}
		size_t GetSize() const
		{
			return this->size;
		}
		const uint8_t* GetTransactionId() const
		{
			return this->transactionId;
		}
		void SetUsername(const char* username, size_t len)
		{
			this->username.assign(username, len);
		}
		void SetPriority(uint32_t priority)
		{
			this->priority = priority;
		}
		void SetIceControlling(uint64_t iceControlling)
		{
			this->iceControlling = iceControlling;
		}
		void SetIceControlled(uint64_t iceControlled)
		{
			this->iceControlled = iceControlled;
		}
		void SetUseCandidate()
		{
			this->hasUseCandidate = true;
		}
		void SetNomination(uint32_t nomination)
		{
			this->nomination = nomination;
		}
		void SetXorMappedAddress(const struct sockaddr* xorMappedAddress)
		{
			this->xorMappedAddress = xorMappedAddress;
		}
		void SetErrorCode(uint16_t errorCode)
		{
			this->errorCode = errorCode;
		}
		void SetSoftware(const char* software, size_t len)
		{
			this->software.assign(software, len);
		}
		void SetMessageIntegrity(const uint8_t* messageIntegrity)
		{
			this->messageIntegrity = messageIntegrity;
		}
		void SetFingerprint()
		{
			this->hasFingerprint = true;
		}
		const std::string& GetUsername() const
		{
			return this->username;
		}
		void SetPassword(const std::string& password);
		uint32_t GetPriority() const
		{
			return this->priority;
		}
		uint64_t GetIceControlling() const
		{
			return this->iceControlling;
		}
		uint64_t GetIceControlled() const
		{
			return this->iceControlled;
		}
		bool HasUseCandidate() const
		{
			return this->hasUseCandidate;
		}
		void SetHasNomination()
		{
			this->hasNomination = true;
		}
		bool HasNomination() const
		{
			return this->hasNomination;
		}
		uint32_t GetNomination() const
		{
			return this->nomination;
		}
		uint16_t GetErrorCode() const
		{
			return this->errorCode;
		}
		std::string GetSoftware() const
		{
			return this->software;
		}
		bool HasMessageIntegrity() const
		{
			return (this->messageIntegrity != nullptr);
		}
		bool HasFingerprint() const
		{
			return this->hasFingerprint;
		}
		Authentication CheckAuthentication(
		  // The first username fragment in the USERNAME attribute.
		  const std::string& usernameFragment1,
		  const std::string& password);
		StunPacket* CreateSuccessResponse();
		StunPacket* CreateErrorResponse(uint16_t errorCode);
		void Serialize(uint8_t* buffer);

	private:
		// Passed by argument.
		Class klass;                             // 2 bytes.
		Method method;                           // 2 bytes.
		const uint8_t* transactionId{ nullptr }; // 12 bytes.
		uint8_t* data{ nullptr };                // Pointer to binary data.
		size_t size{ 0u };                       // The full message size (including header).
		// STUN attributes.
		std::string username; // Less than 513 bytes.
		std::string password;
		uint32_t priority{ 0u };       // 4 bytes unsigned integer.
		uint64_t iceControlling{ 0u }; // 8 bytes unsigned integer.
		uint64_t iceControlled{ 0u };  // 8 bytes unsigned integer.
		bool hasNomination{ false };
		uint32_t nomination{ 0u };                          // 4 bytes unsigned integer.
		bool hasUseCandidate{ false };                      // 0 bytes.
		const uint8_t* messageIntegrity{ nullptr };         // 20 bytes.
		bool hasFingerprint{ false };                       // 4 bytes.
		const struct sockaddr* xorMappedAddress{ nullptr }; // 8 or 20 bytes.
		uint16_t errorCode{ 0u };                           // 4 bytes (no reason phrase).
		std::string software;                               // Less than 763 bytes.
	};
} // namespace RTC

#endif
