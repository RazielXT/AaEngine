#include "Resources/Model/BinaryModelLoader.h"
#include "Resources/Model/BinaryModelSerialization.h"
#include "Utils/Logger.h"
#include "Math.h"

VertexBufferModel* BinaryModelLoader::load(std::string filename, ModelParseOptions options)
{
	ModelInfo info;

	if (!BinaryModelSerialization::ReadModel(filename, info))
	{
		Logger::logError("Failed to load model " + filename);
		return nullptr;
	}

	auto model = new VertexBufferModel();

	for (auto& element : info.layout)
	{
		model->addLayoutElement(element.format, VertexElementSemantic::GetConstName(element.semantic));
	}

	auto vertexSize = model->getLayoutVertexSize(0);

	model->calculateBounds(info.vertexData.data(), info.vertexCount, vertexSize, model->vertexLayout);

	if (info.indexCount)
	{
		model->indexCount = info.indexCount;
		model->CreateIndexBuffer(options.device, options.batch, info.indices.data(), info.indices.size());
	}

	model->vertexCount = info.vertexCount;
	model->CreateVertexBuffer(options.device, options.batch, info.vertexData.data(), info.vertexCount, vertexSize);

	return model;
}
