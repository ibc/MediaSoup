#define MS_CLASS "RTC::Parameters"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/Parameters.hpp"
#include "Logger.hpp"

namespace RTC
{
	/* Instance methods. */

	std::vector<flatbuffers::Offset<FBS::RtpParameters::Parameter>> Parameters::FillBuffer(
	  flatbuffers::FlatBufferBuilder& builder) const
	{
		MS_TRACE();

		std::vector<flatbuffers::Offset<FBS::RtpParameters::Parameter>> parameters;

		for (const auto& kv : this->mapKeyValues)
		{
			const auto& key   = kv.first;
			const auto& value = kv.second;

			flatbuffers::Offset<FBS::RtpParameters::Parameter> parameter;

			switch (value.type)
			{
				case Value::Type::BOOLEAN:
				{
					auto valueOffset = FBS::RtpParameters::CreateBoolean(builder, value.booleanValue);

					parameter = FBS::RtpParameters::CreateParameterDirect(
					  builder, key.c_str(), FBS::RtpParameters::Value::Boolean, valueOffset.Union());

					break;
				}

				case Value::Type::INTEGER:
				{
					auto valueOffset = FBS::RtpParameters::CreateInteger32(builder, value.integerValue);

					parameters.emplace_back(FBS::RtpParameters::CreateParameterDirect(
					  builder, key.c_str(), FBS::RtpParameters::Value::Integer32, valueOffset.Union()));

					break;
				}

				case Value::Type::DOUBLE:
				{
					auto valueOffset = FBS::RtpParameters::CreateDouble(builder, value.doubleValue);

					parameters.emplace_back(FBS::RtpParameters::CreateParameterDirect(
					  builder, key.c_str(), FBS::RtpParameters::Value::Double, valueOffset.Union()));

					break;
				}

				case Value::Type::STRING:
				{
					auto valueOffset =
					  FBS::RtpParameters::CreateStringDirect(builder, value.stringValue.c_str());

					parameters.emplace_back(FBS::RtpParameters::CreateParameterDirect(
					  builder, key.c_str(), FBS::RtpParameters::Value::String, valueOffset.Union()));

					break;
				}

				case Value::Type::ARRAY_OF_INTEGERS:
				{
					auto valueOffset =
					  FBS::RtpParameters::CreateInteger32ArrayDirect(builder, &value.arrayOfIntegers);

					parameters.emplace_back(FBS::RtpParameters::CreateParameterDirect(
					  builder, key.c_str(), FBS::RtpParameters::Value::Integer32Array, valueOffset.Union()));

					break;
				}
			}
		}

		return parameters;
	}

	void Parameters::Set(const flatbuffers::Vector<flatbuffers::Offset<FBS::RtpParameters::Parameter>>* data)
	{
		MS_TRACE();

		for (const auto* parameter : *data)
		{
			const auto key = parameter->name()->str();

			switch (parameter->value_type())
			{
				case FBS::RtpParameters::Value::Boolean:
				{
					this->mapKeyValues.emplace(
					  key, Value((parameter->value_as_Boolean()->value() == 0) ? false : true));

					break;
				}

				case FBS::RtpParameters::Value::Integer32:
				{
					this->mapKeyValues.emplace(key, Value(parameter->value_as_Integer32()->value()));

					break;
				}

				case FBS::RtpParameters::Value::Double:
				{
					this->mapKeyValues.emplace(key, Value(parameter->value_as_Double()->value()));

					break;
				}

				case FBS::RtpParameters::Value::String:
				{
					this->mapKeyValues.emplace(key, Value(parameter->value_as_String()->value()->str()));

					break;
				}

				case FBS::RtpParameters::Value::Integer32Array:
				{
					this->mapKeyValues.emplace(key, Value(parameter->value_as_Integer32Array()->value()));

					break;
				}

				default:; // Just ignore other value types.
			}
		}
	}

	bool Parameters::HasBoolean(const std::string& key) const
	{
		MS_TRACE();

		auto it = this->mapKeyValues.find(key);

		if (it == this->mapKeyValues.end())
		{
			return false;
		}

		const auto& value = it->second;

		return value.type == Value::Type::BOOLEAN;
	}

	bool Parameters::HasInteger(const std::string& key) const
	{
		MS_TRACE();

		auto it = this->mapKeyValues.find(key);

		if (it == this->mapKeyValues.end())
		{
			return false;
		}

		const auto& value = it->second;

		return value.type == Value::Type::INTEGER;
	}

	bool Parameters::HasPositiveInteger(const std::string& key) const
	{
		MS_TRACE();

		auto it = this->mapKeyValues.find(key);

		if (it == this->mapKeyValues.end())
		{
			return false;
		}

		const auto& value = it->second;

		return value.type == Value::Type::INTEGER && value.integerValue >= 0;
	}

	bool Parameters::HasDouble(const std::string& key) const
	{
		MS_TRACE();

		auto it = this->mapKeyValues.find(key);

		if (it == this->mapKeyValues.end())
		{
			return false;
		}

		const auto& value = it->second;

		return value.type == Value::Type::DOUBLE;
	}

	bool Parameters::HasString(const std::string& key) const
	{
		MS_TRACE();

		auto it = this->mapKeyValues.find(key);

		if (it == this->mapKeyValues.end())
		{
			return false;
		}

		const auto& value = it->second;

		return value.type == Value::Type::STRING;
	}

	bool Parameters::HasArrayOfIntegers(const std::string& key) const
	{
		MS_TRACE();

		auto it = this->mapKeyValues.find(key);

		if (it == this->mapKeyValues.end())
		{
			return false;
		}

		const auto& value = it->second;

		return value.type == Value::Type::ARRAY_OF_INTEGERS;
	}

	bool Parameters::IncludesInteger(const std::string& key, int32_t integer) const
	{
		MS_TRACE();

		auto it = this->mapKeyValues.find(key);

		if (it == this->mapKeyValues.end())
		{
			return false;
		}

		const auto& value = it->second;
		const auto& array = value.arrayOfIntegers;

		return std::find(array.begin(), array.end(), integer) != array.end();
	}

	bool Parameters::GetBoolean(const std::string& key) const
	{
		MS_TRACE();

		auto it = this->mapKeyValues.find(key);

		MS_ASSERT(it != this->mapKeyValues.end(), "key does not exist [key:%s]", key.c_str());

		const auto& value = it->second;

		return value.booleanValue;
	}

	int32_t Parameters::GetInteger(const std::string& key) const
	{
		MS_TRACE();

		auto it = this->mapKeyValues.find(key);

		MS_ASSERT(it != this->mapKeyValues.end(), "key does not exist [key:%s]", key.c_str());

		const auto& value = it->second;

		return value.integerValue;
	}

	double Parameters::GetDouble(const std::string& key) const
	{
		MS_TRACE();

		auto it = this->mapKeyValues.find(key);

		MS_ASSERT(it != this->mapKeyValues.end(), "key does not exist [key:%s]", key.c_str());

		const auto& value = it->second;

		return value.doubleValue;
	}

	const std::string& Parameters::GetString(const std::string& key) const
	{
		MS_TRACE();

		auto it = this->mapKeyValues.find(key);

		MS_ASSERT(it != this->mapKeyValues.end(), "key does not exist [key:%s]", key.c_str());

		const auto& value = it->second;

		return value.stringValue;
	}

	const std::vector<int32_t>& Parameters::GetArrayOfIntegers(const std::string& key) const
	{
		MS_TRACE();

		auto it = this->mapKeyValues.find(key);

		MS_ASSERT(it != this->mapKeyValues.end(), "key does not exist [key:%s]", key.c_str());

		const auto& value = it->second;

		return value.arrayOfIntegers;
	}
} // namespace RTC
