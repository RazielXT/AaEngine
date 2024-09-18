#include "AaBinaryModelLoader.h"
#include "AaModelSerialization.h"
#include "AaLogger.h"
#include "Math.h"

AaModel* AaBinaryModelLoader::load(std::string filename, AaRenderSystem* rs)
{
	ModelInfo m;

	if (!AaModelSerialization::ReadModel(filename, m))
	{
		AaLogger::logError("Failed to load model " + filename);
		return nullptr;
	}

	return AaBinaryModelLoader::load(m, rs);
}

AaModel* AaBinaryModelLoader::load(ModelInfo& info, AaRenderSystem* rs)
{
	auto model = new AaModel(rs);

	model->calculateBounds(info.positions);

	if (info.indexCount)
	{
		model->indexCount = info.indexCount;
		model->buildIndexBuffer(info.indices.data());
	}

	if (!info.positions.empty())
		model->addLayoutElement(0, 0, DXGI_FORMAT_R32G32B32_FLOAT, VertexElementSemantic::POSITION, 0);
	if (!info.normals.empty())
		model->addLayoutElement(0, model->getLayoutVertexSize(0), DXGI_FORMAT_R32G32B32_FLOAT, VertexElementSemantic::NORMAL, 0);
	if (!info.tangents.empty())
		model->addLayoutElement(0, model->getLayoutVertexSize(0), DXGI_FORMAT_R32G32B32_FLOAT, VertexElementSemantic::TANGENT, 0);
	if (!info.texCoords.empty())
		model->addLayoutElement(0, model->getLayoutVertexSize(0), DXGI_FORMAT_R32G32_FLOAT, VertexElementSemantic::TEXCOORD, 0);

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
	model->buildVertexBuffer(data.data(), data.size());

	return model;
}
