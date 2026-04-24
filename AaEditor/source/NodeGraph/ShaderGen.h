#pragma once

#include <unordered_map>
#include <string>

enum class ValueType
{
	Float,
	Float2,
	Float3,
	Float4,
	UV,
};

struct Pin;

struct ShaderGenContext
{
	int tempIndex = 0;
	std::unordered_map<const Pin*, std::string> pinVars;
	std::string code;

	std::string NewTemp(ValueType)
	{
		return "v" + std::to_string(tempIndex++);
	}
	std::string TypeName(ValueType type)
	{
		switch (type)
		{
		case ValueType::UV:
		case ValueType::Float2:
			return "float2";

		case ValueType::Float3:
			return "float3";

		case ValueType::Float4:
			return "float4";

		case ValueType::Float:
		default:
			return "float";
		}
	}
};
