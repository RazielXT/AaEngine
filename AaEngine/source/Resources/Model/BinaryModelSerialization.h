#pragma once

#include <vector>
#include <fstream>
#include <d3d12.h>
#include <cstdint>

struct VertexLayoutElement
{
	DXGI_FORMAT format;
	char semantic[16];
};

struct ModelInfo
{
	std::vector<VertexLayoutElement> layout;
	std::vector<char> vertexData;

	std::vector<uint16_t> indices;
	uint32_t indexCount{};
	uint32_t vertexCount{};
};

namespace BinaryModelSerialization
{
	enum HeaderType
	{
		Layout,
		Vertices,
		Indices,
		Metadata,
	};
	struct Header
	{
		uint32_t headerSize = sizeof(Header);
		uint32_t dataType{};
		uint32_t dataSize{};
		uint32_t version = 2;
	};
	struct MetadataObject
	{
		uint32_t indexCount{};
		uint32_t vertexCount{};
	};

	bool SaveModel(const std::string& filename, const ModelInfo& model);
	bool ReadModel(const std::string& filename, ModelInfo& model);
}
