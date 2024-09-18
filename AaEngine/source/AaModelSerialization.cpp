#include "AaModelSerialization.h"

bool AaModelSerialization::SaveModel(const std::string& filename, const ModelInfo& model)
{
	std::ofstream file(filename, std::ios::binary);
	{
		Header h{ .dataType = HeaderType::Metadata, .dataSize = sizeof(MetadataObject) };
		file.write((const char*)&h, h.headerSize);

		MetadataObject data{ model.indexCount, model.vertexCount };
		file.write((const char*)&data, h.dataSize);
	}
	if (!model.positions.empty())
	{
		Header h{ .dataType = HeaderType::Positions, .dataSize = (uint32_t)model.positions.size() * sizeof(float) };
		file.write((const char*)&h, h.headerSize);
		file.write((const char*)model.positions.data(), h.dataSize);
	}
	if (!model.normals.empty())
	{
		Header h{ .dataType = HeaderType::Normals, .dataSize = (uint32_t)model.normals.size() * sizeof(float) };
		file.write((const char*)&h, h.headerSize);
		file.write((const char*)model.normals.data(), h.dataSize);
	}
	if (!model.tangents.empty())
	{
		Header h{ .dataType = HeaderType::Tangents, .dataSize = (uint32_t)model.tangents.size() * sizeof(float) };
		file.write((const char*)&h, h.headerSize);
		file.write((const char*)model.tangents.data(), h.dataSize);
	}
	if (!model.texCoords.empty())
	{
		Header h{ .dataType = HeaderType::TexCoords, .dataSize = (uint32_t)model.texCoords.size() * sizeof(float) };
		file.write((const char*)&h, h.headerSize);
		file.write((const char*)model.texCoords.data(), h.dataSize);
	}
	if (!model.indices.empty())
	{
		Header h{ .dataType = HeaderType::Indices, .dataSize = (uint32_t)model.indices.size() * sizeof(uint16_t) };
		file.write((const char*)&h, h.headerSize);
		file.write((const char*)model.indices.data(), h.dataSize);
	}

	return !file.fail();
}

bool AaModelSerialization::ReadModel(const std::string& filename, ModelInfo& model)
{
	std::ifstream file(filename, std::ios::binary);

	while (file)
	{
		Header h;
		file.read((char*)&h.headerSize, sizeof(h.headerSize));
		file.read((char*)&h.dataType, h.headerSize - sizeof(h.headerSize));

		if (!h.dataSize)
			continue;

		if (h.dataType == HeaderType::Metadata)
		{
			MetadataObject obj;
			file.read((char*)&obj, h.dataSize);

			model.indexCount = obj.indexCount;
			model.vertexCount = obj.vertexCount;
		}
		else if (h.dataType == HeaderType::Positions)
		{
			model.positions.resize(h.dataSize / sizeof(float));
			file.read((char*)model.positions.data(), h.dataSize);
		}
		else if (h.dataType == HeaderType::Normals)
		{
			model.normals.resize(h.dataSize / sizeof(float));
			file.read((char*)model.normals.data(), h.dataSize);
		}
		else if (h.dataType == HeaderType::Tangents)
		{
			model.tangents.resize(h.dataSize / sizeof(float));
			file.read((char*)model.tangents.data(), h.dataSize);
		}
		else if (h.dataType == HeaderType::TexCoords)
		{
			model.texCoords.resize(h.dataSize / sizeof(float));
			file.read((char*)model.texCoords.data(), h.dataSize);
		}
		else if (h.dataType == HeaderType::Indices)
		{
			model.indices.resize(h.dataSize / sizeof(uint16_t));
			file.read((char*)model.indices.data(), h.dataSize);
		}
	}

	return file.eof();
}
