#include "GltfLoader.h"
#include "FileLogger.h"
#include "Math.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#include "tinygltf/tiny_gltf.h"
#include "RenderSystem.h"
#include "GraphicsResources.h"
#include "ObjectId.h"
#include "SceneManager.h"

static std::pair<const char*, DXGI_FORMAT> ParseGltfElement(const std::string& name, int type)
{
	auto format = DXGI_FORMAT_R32G32B32_FLOAT;
	if (type == TINYGLTF_TYPE_VEC2)
		format = DXGI_FORMAT_R32G32_FLOAT;
	if (type == TINYGLTF_TYPE_VEC4)
		format = DXGI_FORMAT_R32G32B32A32_FLOAT;

	const char* semanticName = nullptr;

	if (name == VertexElementSemantic::POSITION)
		semanticName = VertexElementSemantic::POSITION;
	else if (name == VertexElementSemantic::TANGENT)
		semanticName = VertexElementSemantic::TANGENT;
	else if (name == VertexElementSemantic::NORMAL)
		semanticName = VertexElementSemantic::NORMAL;
	else if (name.starts_with(VertexElementSemantic::TEXCOORD))
		semanticName = VertexElementSemantic::TEXCOORD;

	return { semanticName, format };
}

static SceneCollection::Data ProcessGLTF(const tinygltf::Model& data, SceneCollection::LoadCtx& ctx)
{
	SceneCollection::Data loadInfo;

	for (const auto& mesh : data.meshes)
	{
		uint32_t idx = 0;

		for (const auto& prim : mesh.primitives)
		{
			SceneCollection::Entity info;
			info.name = mesh.name;

			if (idx > 0)
				info.name += "_" + std::to_string(idx);
			idx++;

			auto model = info.mesh.model = new VertexBufferModel();
			ctx.resources.models.addLoadedModel(info.name, model);

			// Vertex buffer
			{
				size_t vertexCount{};
				std::vector<const uint8_t*> dataBuffers;

				for (const auto& attr : prim.attributes)
				{
					const tinygltf::Accessor& acc = data.accessors[attr.second];
					const tinygltf::BufferView& view = data.bufferViews[acc.bufferView];
					const tinygltf::Buffer& buf = data.buffers[view.buffer];

					auto e = ParseGltfElement(attr.first, acc.type);

					if (!e.first)
						continue;

					model->addLayoutElement(e.second, e.first);
					dataBuffers.push_back(buf.data.data() + view.byteOffset + acc.byteOffset);

					vertexCount = acc.count;
				}

				const auto vertexSize = model->getLayoutVertexSize(0);

				std::vector<char> vbData;
				vbData.resize(vertexSize * vertexCount);

				for (size_t i = 0; i < model->vertexLayout.size(); i++)
				{
					const auto& element = model->vertexLayout[i];
					auto elementSize = model->getFormatSize(element.Format);
					auto elementData = dataBuffers[i];

					for (size_t i = 0; i < vertexCount; i++)
					{
						memcpy(&vbData[i * vertexSize + element.AlignedByteOffset], &elementData[i * elementSize], elementSize);
					}
				}

				model->vertexCount = vertexCount;
				model->CreateVertexBuffer(ctx.renderSystem.core.device, &ctx.batch, vbData.data(), vertexCount, vertexSize);
			}		

			// Index buffer
			{
				const tinygltf::Accessor& idxAcc = data.accessors[prim.indices];
				const tinygltf::BufferView& idxView = data.bufferViews[idxAcc.bufferView];
				const tinygltf::Buffer& idxBuf = data.buffers[idxView.buffer];

				model->indexCount = idxAcc.count;

				auto indices = idxBuf.data.data() + idxView.byteOffset + idxAcc.byteOffset;

				if (idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
					model->CreateIndexBuffer(ctx.renderSystem.core.device, &ctx.batch, (const uint16_t*)indices, idxAcc.count);
				else
					model->CreateIndexBuffer(ctx.renderSystem.core.device, &ctx.batch, (const uint32_t*)indices, idxAcc.count);
			}

			if (prim.material >= 0)
				info.materialName = data.materials[prim.material].name;

			loadInfo.entities.push_back(info);
		}
	}

	for (const auto& node : data.nodes)
	{
		for (auto& e : loadInfo.entities)
		{
			if (e.name != node.name)
				continue;

			ObjectTransformation& tr = e.tr;
			if (node.translation.size() == 3)
				tr.position = { (float)node.translation[0], (float)node.translation[1], (float)node.translation[2] };
			if (node.scale.size() == 3)
				tr.scale = { (float)node.scale[0], (float)node.scale[1], (float)node.scale[2] };
			if (node.rotation.size() == 4)
				tr.orientation = { (float)node.rotation[0], (float)node.rotation[1], (float)node.rotation[2], (float)node.rotation[3] };

			tr.position *= 40;
			tr.scale *= 40;

			break;
		}
	}

	return loadInfo;
}

SceneCollection::Data GltfLoader::load(const std::string& path, SceneCollection::LoadCtx ctx)
{
	tinygltf::TinyGLTF loader;
	tinygltf::Model gltfModel;
	std::string error;
	std::string warning;

	if (path.ends_with(".gltf") && !loader.LoadASCIIFromFile(&gltfModel, &error, &warning, path))
	{
		FileLogger::logError("Failed to load " + path + "\n" + error);
		return {};
	}
	if (path.ends_with(".glb") && !loader.LoadBinaryFromFile(&gltfModel, &error, &warning, path))
	{
		FileLogger::logError("Failed to load " + path + "\n" + error);
		return {};
	}

	return ProcessGLTF(gltfModel, ctx);
}
