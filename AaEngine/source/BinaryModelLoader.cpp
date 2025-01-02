#include "BinaryModelLoader.h"
#include "BinaryModelSerialization.h"
#include "FileLogger.h"
#include "Math.h"

VertexBufferModel* BinaryModelLoader::load(std::string filename, ModelParseOptions options)
{
	ModelInfo info;

	if (!BinaryModelSerialization::ReadModel(filename, info))
	{
		FileLogger::logError("Failed to load model " + filename);
		return nullptr;
	}

	auto model = new VertexBufferModel();

	model->calculateBounds(info.positions);

	if (info.indexCount)
	{
		model->indexCount = info.indexCount;
		model->CreateIndexBuffer(options.device, options.batch, info.indices);
	}

	if (!info.positions.empty())
		model->addLayoutElement(0, 0, DXGI_FORMAT_R32G32B32_FLOAT, VertexElementSemantic::POSITION, 0);
	if (!info.texCoords.empty())
		model->addLayoutElement(0, model->getLayoutVertexSize(0), DXGI_FORMAT_R32G32_FLOAT, VertexElementSemantic::TEXCOORD, 0);
	if (!info.normals.empty())
		model->addLayoutElement(0, model->getLayoutVertexSize(0), DXGI_FORMAT_R32G32B32_FLOAT, VertexElementSemantic::NORMAL, 0);
	if (!info.tangents.empty())
		model->addLayoutElement(0, model->getLayoutVertexSize(0), DXGI_FORMAT_R32G32B32_FLOAT, VertexElementSemantic::TANGENT, 0);

	auto vertexSize = model->getLayoutVertexSize(0);

	std::vector<char> data;
	data.resize(vertexSize * info.positions.size());

	auto elementsCount = info.positions.size() / 3;

	for (size_t i = 0; i < elementsCount; i++)
	{
		for (auto& element : model->vertexLayout)
		{
			void* source = nullptr;
			if (element.SemanticName == VertexElementSemantic::POSITION)
				memcpy(&data[i * vertexSize], &info.positions[i * 3], sizeof(float) * 3);
			else if (element.SemanticName == VertexElementSemantic::NORMAL)
				memcpy(&data[i * vertexSize + element.AlignedByteOffset], &info.normals[i * 3], sizeof(float) * 3);
			else if (element.SemanticName == VertexElementSemantic::TANGENT)
				memcpy(&data[i * vertexSize + element.AlignedByteOffset], &info.tangents[i * 3], sizeof(float) * 3);
			else if (element.SemanticName == VertexElementSemantic::TEXCOORD)
				memcpy(&data[i * vertexSize + element.AlignedByteOffset], &info.texCoords[i * 2], sizeof(float) * 2);
		}
	}

	model->vertexCount = info.positions.size();
	model->CreateVertexBuffer(options.device, options.batch, data.data(), elementsCount, vertexSize);

	return model;
}
