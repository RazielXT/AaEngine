#include "VegetationTrees.h"
#include "SceneManager.h"

VegetationTrees::VegetationTrees(SceneManager& s) : sceneMgr(s)
{
}

VertexBufferModel* sphereModel;
MaterialInstance* sphereMaterial;

void VegetationTrees::initialize(GraphicsResources& resources, ResourceUploadBatch& batch)
{
	treeData.trunkMaterial = resources.materials.getMaterial("TreeTrunk", batch);
	treeData.branchMaterial = resources.materials.getMaterial("TreeBranch", batch);

	treeData.lod[0].trunkModel = resources.models.getModel("treeTrunk.mesh", batch, {ResourceGroup::Core});
	treeData.lod[0].branchModel = resources.models.getModel("treeBranch.mesh", batch, { ResourceGroup::Core });

	treeData.lod[1].trunkModel = resources.models.getModel("treeTrunkLod1.mesh", batch, { ResourceGroup::Core });
	treeData.lod[1].branchModel = resources.models.getModel("treeBranchLod1.mesh", batch, { ResourceGroup::Core });

	treeData.lod[2].trunkModel = resources.models.getModel("treeTrunkLod2.mesh", batch, { ResourceGroup::Core });
	treeData.lod[2].branchModel = resources.models.getModel("treeBranchLod2.mesh", batch, { ResourceGroup::Core });

	sphereMaterial = resources.materials.getMaterial("TriplanarTest", batch);
	sphereModel = resources.models.getModel("sphere.mesh", batch, { ResourceGroup::Core });
}

void VegetationTrees::clear()
{
	trees.clear();
}

void VegetationTrees::addTree(const ObjectTransformation& tr)
{
	static int t = 0;

// 	auto tree = sceneMgr.createEntity("Tree" + std::to_string(t), tr, *treeData.lod[0].branchModel);
// 	tree->material = treeData.branchMaterial;
// 
// 	auto treeTrunk = sceneMgr.createEntity("TreeTrunk" + std::to_string(t), tr, *treeData.lod[0].trunkModel);
// 	treeTrunk->material = treeData.trunkMaterial;

	auto tr2 = tr;
	tr2.position.y += 20;
	tr2.scale *= 10;

	auto tree = sceneMgr.createEntity("SphereTest" + std::to_string(t), tr2, *sphereModel);
	tree->material = sphereMaterial;
 	t++;
// 
// 	trees.emplace_back(tree, treeTrunk);
}

void VegetationTrees::swapLods()
{
	static int LodIdx = 0;
	LodIdx = (LodIdx + 1) % 2;

	for (auto& t : trees)
	{
		t.branch->geometry.fromModel(*treeData.lod[LodIdx + 1].branchModel);
		t.trunk->geometry.fromModel(*treeData.lod[LodIdx + 1].trunkModel);
	}
}

void VegetationTrees::update(const Vector3& position)
{
	for (auto& t : trees)
	{
		float sqDistance = Vector3::DistanceSquared(t.branch->getPosition(), position);
		UINT idx = 0;
		if (sqDistance > 100 * 100)
			idx = 1;
		if (sqDistance > 200 * 200)
			idx = 2;

		t.branch->geometry.fromModel(*treeData.lod[idx].branchModel);
		t.trunk->geometry.fromModel(*treeData.lod[idx].trunkModel);
	}
}
