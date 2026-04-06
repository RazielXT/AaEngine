#pragma once

#include <array>
#include <cstdint>
#include <string>

enum class ShaderType
{
	Vertex,
	Pixel,
	Geometry,
	Amplification,
	Mesh,
	Compute,
	COUNT,
	None
};

// ------------------------------------------------------------
//  ENUM RANGE ITERATION
// ------------------------------------------------------------
class ShaderTypes {
public:
	struct Iterator {
		uint32_t value;

		constexpr ShaderType operator*() const {
			return static_cast<ShaderType>(value);
		}
		constexpr void operator++() {
			++value;
		}
		constexpr bool operator!=(const Iterator& other) const {
			return value != other.value;
		}
	};

	constexpr Iterator begin() const { return { 0 }; }
	constexpr Iterator end()   const { return { static_cast<uint32_t>(ShaderType::COUNT) }; }
};

// ------------------------------------------------------------
//  ARRAY MAPPED TO ShaderType
// ------------------------------------------------------------
template<typename T>
struct ShaderTypesArray {
	std::array<T, static_cast<size_t>(ShaderType::COUNT)> data{};

	constexpr T& operator[](ShaderType t) {
		return data[static_cast<size_t>(t)];
	}
	constexpr const T& operator[](ShaderType t) const {
		return data[static_cast<size_t>(t)];
	}
};

namespace ShaderTypeString
{
	ShaderType Parse(const std::string& t);

	std::string ShortName(ShaderType type);
};
