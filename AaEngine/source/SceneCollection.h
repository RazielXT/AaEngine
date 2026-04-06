#pragma once

#include <string>
#include <vector>
#include "RenderObject.h"
#include "VertexBufferModel.h"

class SceneManager;
class RenderSystem;
struct GraphicsResources;

namespace SceneCollection
{
	struct Mesh
	{
		VertexBufferModel* model;
	};

	struct Entity
	{
		std::string name;
		std::string materialName;
		Mesh mesh;
		ObjectTransformation tr;
	};

	struct Data
	{
		std::string name;
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
	void loadScene(const Data& data, LoadCtx);
};