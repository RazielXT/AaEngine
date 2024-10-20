#pragma once

#include <vector>
#include <fstream>

struct ModelInfo
{
	std::vector<float> positions;
	std::vector<float> normals;
	std::vector<float> tangents;
	std::vector<float> texCoords;

	std::vector<uint16_t> indices;
	uint32_t indexCount{};
	uint32_t vertexCount{};
};

namespace BinaryModelSerialization
{
	enum HeaderType
	{
		Positions,
		Normals,
		Tangents,
		TexCoords,
		Indices,
		Metadata,
	};
	struct Header
	{
		uint32_t headerSize = sizeof(Header);
		uint32_t dataType{};
		uint32_t dataSize{};
		uint32_t version = 1;
	};
	struct MetadataObject
	{
		uint32_t indexCount{};
		uint32_t vertexCount{};
	};

	bool SaveModel(const std::string& filename, const ModelInfo& model);
	bool ReadModel(const std::string& filename, ModelInfo& model);
}
