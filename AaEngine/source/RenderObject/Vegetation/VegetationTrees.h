#pragma once

#include "RenderCore/RenderSystem.h"
#include "Resources/GraphicsResources.h"
#include "Scene/RenderObject.h"

class SceneManager;

class VegetationTrees
{
public:

	VegetationTrees(SceneManager&);

	void initialize(GraphicsResources&, ResourceUploadBatch& batch);

	void clear();

	void addTree(const ObjectTransformation&);

	void swapLods();

	void update(const Vector3& position);

private:

	struct TreeEntity
	{
		SceneEntity* branch;
		SceneEntity* trunk;
	};
	std::vector<TreeEntity> trees;

	struct Tree
	{
		MaterialInstance* branchMaterial{};
		MaterialInstance* trunkMaterial{};

		struct Lod
		{
			VertexBufferModel* branchModel;
			VertexBufferModel* trunkModel;
		};

		Lod lod[3];
	};

	Tree treeData;

	SceneManager& sceneMgr;
};
