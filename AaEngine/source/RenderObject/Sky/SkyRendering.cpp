#include "RenderObject/Sky/SkyRendering.h"
#include "Scene/RenderWorld.h"
#include "Resources/Material/MaterialResources.h"

void SkyRendering::createSky(RenderWorld& renderWorld, MaterialResources& materials, DirectX::ResourceUploadBatch& batch)
{
	auto e = renderWorld.createEntity(EntityCreateProperties{ .order = Order::Post, .suborder = -2 });
	e->material = materials.getMaterial("Sky", batch);

	e->geometry.type = EntityGeometry::Type::Manual;
	e->geometry.vertexCount = 3;
	e->geometry.instanceCount = 1;

	e->setBoundingBox({ {}, { 10000, 10000, 10000 } });
	e->setFlag(RenderObjectFlag::NoShadow);
}

void SkyRendering::createClouds(RenderWorld& renderWorld, MaterialResources& materials, ID3D12Device* device, DirectX::ResourceUploadBatch& batch)
{
	auto e = renderWorld.createEntity(EntityCreateProperties{ .order = Order::Post });
	e->material = materials.getMaterial("BasicClouds", batch);

	cloudsModel.CreateIndexBufferGrid(device, &batch, 256);
	cloudsModel.bbox.Extents = { 50000, 10000, 50000 };

	e->geometry.fromModel(cloudsModel);
	e->setBoundingBox(cloudsModel.bbox);
	e->setFlag(RenderObjectFlag::NoShadow);
}

void SkyRendering::createMoon(RenderWorld& renderWorld, MaterialResources& materials, DirectX::ResourceUploadBatch& batch)
{
	auto moon = renderWorld.createEntity(EntityCreateProperties{ .order = Order::Post, .suborder = -1 });
	moon->material = materials.getMaterial("Moon", batch);
	moon->geometry.type = EntityGeometry::Type::Manual;
	moon->geometry.vertexCount = 6;
	moon->geometry.instanceCount = 1;
	moon->setBoundingBox({ {}, { 10000, 10000, 10000 } });
}
