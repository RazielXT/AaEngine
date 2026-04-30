#pragma once

#include <string>
#include <vector>
#include "Scene/RenderObject.h"
#include "Resources/Model/VertexBufferModel.h"

class SceneManager;
class RenderSystem;
struct GraphicsResources;

namespace SceneCollection
{
	struct Mesh
	{
		std::string name;
	};

	struct Entity
	{
		std::string name;
		std::string materialName;
		Mesh mesh;
		ObjectTransformation tr;
		uint32_t primitiveIdx{};
	};

	struct ResourceData
	{
		std::string name;
		std::string path;
		std::vector<Entity> entities;
	};

	struct LoadCtx
	{
		ResourceUploadBatch& batch;
		SceneManager& sceneMgr;
		RenderSystem& renderSystem;
		GraphicsResources& resources;
		ObjectTransformation placement = {};
	};
	void loadResource(const ResourceData& data, LoadCtx);
	void loadResources(const std::vector<ResourceData>& data, LoadCtx);
};