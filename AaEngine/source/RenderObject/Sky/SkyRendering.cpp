#include "RenderObject/Sky/SkyRendering.h"
#include "Scene/SceneManager.h"
#include "Resources/Material/MaterialResources.h"

void SkyRendering::createClouds(SceneManager& sceneMgr, MaterialResources& materials, ID3D12Device* device, DirectX::ResourceUploadBatch& batch)
{
	auto e = sceneMgr.createEntity(EntityCreateProperties{ .order = Order::Post });
	e->material = materials.getMaterial("BasicClouds", batch);

	cloudsModel.CreateIndexBufferGrid(device, &batch, 256);
	cloudsModel.bbox.Extents = { 50000, 10000, 50000 };

	e->geometry.fromModel(cloudsModel);
	e->setBoundingBox(cloudsModel.bbox);
	e->setFlag(RenderObjectFlag::NoShadow);
}

void SkyRendering::createMoon(SceneManager& sceneMgr, MaterialResources& materials, DirectX::ResourceUploadBatch& batch)
{
	auto moon = sceneMgr.createEntity(EntityCreateProperties{ .order = Order::Post, .suborder = -1 });
	moon->material = materials.getMaterial("Moon", batch);
	moon->geometry.type = EntityGeometry::Type::Manual;
	moon->geometry.vertexCount = 6;
	moon->geometry.instanceCount = 1;
	moon->setBoundingBox({ {}, { 10000, 10000, 10000 } });
}
