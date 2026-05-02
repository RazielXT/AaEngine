#include "Resources/Model/BinaryModelSerialization.h"

bool BinaryModelSerialization::SaveModel(const std::string& filename, const ModelInfo& model)
{
	std::ofstream file(filename, std::ios::binary);
	{
		Header h{ .dataType = HeaderType::Metadata, .dataSize = sizeof(MetadataObject) };
		file.write((const char*)&h, h.headerSize);

		MetadataObject data{ model.indexCount, model.vertexCount };
		file.write((const char*)&data, h.dataSize);
	}
	if (!model.layout.empty())
	{
		Header h{ .dataType = HeaderType::Layout, .dataSize = (uint32_t)(model.layout.size() * sizeof(VertexLayoutElement)) };
		file.write((const char*)&h, h.headerSize);
		file.write((const char*)model.layout.data(), h.dataSize);
	}
	if (!model.vertexData.empty())
	{
		Header h{ .dataType = HeaderType::Vertices, .dataSize = (uint32_t)model.vertexData.size() };
		file.write((const char*)&h, h.headerSize);
		file.write(model.vertexData.data(), h.dataSize);
	}
	if (!model.indices.empty())
	{
		Header h{ .dataType = HeaderType::Indices, .dataSize = (uint32_t)model.indices.size() * sizeof(uint16_t) };
		file.write((const char*)&h, h.headerSize);
		file.write((const char*)model.indices.data(), h.dataSize);
	}

	return !file.fail();
}

bool BinaryModelSerialization::ReadModel(const std::string& filename, ModelInfo& model)
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
		else if (h.dataType == HeaderType::Layout)
		{
			model.layout.resize(h.dataSize / sizeof(VertexLayoutElement));
			file.read((char*)model.layout.data(), h.dataSize);
		}
		else if (h.dataType == HeaderType::Vertices)
		{
			model.vertexData.resize(h.dataSize);
			file.read(model.vertexData.data(), h.dataSize);
		}
		else if (h.dataType == HeaderType::Indices)
		{
			model.indices.resize(h.dataSize / sizeof(uint16_t));
			file.read((char*)model.indices.data(), h.dataSize);
		}
	}

	return file.eof();
}
