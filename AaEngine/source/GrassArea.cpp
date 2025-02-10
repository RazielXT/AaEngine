#include "GrassArea.h"
#include "MathUtils.h"
#include "SceneEntity.h"
#include "SceneManager.h"
#include "MaterialResources.h"
#include "GraphicsResources.h"
#include "BufferHelpers.h"

void GrassAreaDescription::initialize(BoundingBoxVolume extends)
{
	const Vector2 areaSize = Vector2(extends.max.x, extends.max.z) - Vector2(extends.min.x, extends.min.z);
	areaCount = { UINT(areaSize.x / spacing), UINT(areaSize.y / spacing) };
	count = areaCount.x * areaCount.y;
	bbox = extends.createBbox();
}

GrassAreaGenerator::GrassAreaGenerator()
{
}

GrassAreaGenerator::~GrassAreaGenerator()
{
	DescriptorManager::get().removeTextureView(rtt);

	clear();
}

void GrassAreaGenerator::initializeGpuResources(RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch)
{
	defaultMaterial = resources.materials.getMaterial("GrassLeaves");

	grassCS.init(*renderSystem.core.device, "grassInit", resources.shaders);

	heap.InitRtv(renderSystem.core.device, formats.size(), L"GrassGeneratorRtv");
	heap.InitDsv(renderSystem.core.device, 1, L"GrassGeneratorDsv");
	rtt.Init(renderSystem.core.device, 512, 512, heap, formats, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, true, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	rtt.SetName("GrassGeneratorRttTex");

	DescriptorManager::get().createTextureView(rtt);

	createIndexBuffer(renderSystem, batch);
}

void GrassAreaGenerator::clear()
{
	for (auto g : grasses)
	{
		delete g;
	}
	grasses.clear();
}

void GrassAreaGenerator::scheduleGrassCreation(GrassAreaPlacementTask grassTask, ID3D12GraphicsCommandList* commandList, const FrameParameters& frame, GraphicsResources& resources, SceneManager& sceneMgr)
{
	Camera tmpCamera;
	const UINT NearClipDistance = 1;
	auto bbox = grassTask.bbox;
	tmpCamera.setOrthographicCamera(bbox.Extents.x * 2, bbox.Extents.z * 2, 1, bbox.Extents.y * 2 + NearClipDistance);
	tmpCamera.setPosition(bbox.Center + Vector3(0, bbox.Extents.y + NearClipDistance, 0));
	tmpCamera.setDirection({ 0, -1, 0 });
	tmpCamera.updateMatrix();

	RenderObjectsStorage tmpStorage;
	SceneEntity entity(tmpStorage, *grassTask.terrain);
	RenderObjectsVisibilityData visibility;
	tmpStorage.updateVisibility(tmpCamera, visibility);

	rtt.PrepareAsTarget(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	ShaderConstantsProvider constants(frame, visibility, tmpCamera, rtt);

	RenderQueue queue{ formats, MaterialTechnique::TerrainScan };
	queue.update({ EntityChange::Add, Order::Normal, &entity }, resources);
	queue.renderObjects(constants, commandList);
	//queue.update({ { EntityChange::DeleteAll } });

	rtt.TransitionFromTarget(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	GrassAreaDescription desc;
	desc.initialize(grassTask.bbox);

	auto grassEntity = createGrassEntity("grass_" + std::string(entity.name), desc, sceneMgr);
	auto grassArea = createGrassArea(desc, resources);

	{
		auto colorTex = rtt.textures[0].view.srvHeapIndex;
		auto normalTex = rtt.textures[1].view.srvHeapIndex;
		auto typeTex = rtt.textures[2].view.srvHeapIndex;
		auto depthTex = rtt.textures[3].view.srvHeapIndex; //rtt.depth.view.srvHeapIndex;

		grassCS.dispatch(commandList, desc, XMMatrixInverse(nullptr, tmpCamera.getProjectionMatrix()), { colorTex, normalTex, typeTex, depthTex }  , grassArea->gpuBuffer.Get(), grassArea->vertexCounter.Get());

		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(grassArea->vertexCounter.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
		commandList->ResourceBarrier(1, &barrier);
		commandList->CopyResource(grassArea->vertexCounterRead.Get(), grassArea->vertexCounter.Get());
	}

	scheduled.emplace_back(grassEntity, grassArea);
}

std::vector<std::pair<SceneEntity*, GrassArea*>> GrassAreaGenerator::finishGrassCreation()
{
	for (auto [entity, grass] : scheduled)
	{
		uint32_t* pCounterValue = nullptr;
		CD3DX12_RANGE readbackRange(0, sizeof(uint32_t));
		grass->vertexCounterRead->Map(0, &readbackRange, reinterpret_cast<void**>(&pCounterValue));
		grass->vertexCount = 4;
		grass->instanceCount = *pCounterValue;
		grass->indexCount = 6;
		grass->vertexCounterRead->Unmap(0, nullptr);

		entity->geometry.fromGrass(*grass);
	}

	return std::move(scheduled);
}

SceneEntity* GrassAreaGenerator::createGrassEntity(const std::string& name, const GrassAreaDescription& desc, SceneManager& sceneMgr)
{
	Order renderQueue = Order::Normal;
	if (defaultMaterial->IsTransparent())
		renderQueue = Order::Transparent;

	auto grassEntity = sceneMgr.createEntity(name, renderQueue);
	grassEntity->setBoundingBox(desc.bbox);
	grassEntity->material = defaultMaterial;
	grassEntity->setFlag(RenderObjectFlag::NoShadow);

	return grassEntity;
}

GrassArea* GrassAreaGenerator::createGrassArea(const GrassAreaDescription& desc, GraphicsResources& resources)
{
	auto grass = new GrassArea();

	struct Vertex
	{
		Vector3 pos[2];
		Vector3 color;
		Vector3 normal;
		float scale;
	};

	grass->gpuBuffer = resources.shaderBuffers.CreateStructuredBuffer(sizeof(Vertex) * desc.count);
	grass->gpuBuffer->SetName(L"GrassArea");

	grass->vertexCounter = resources.shaderBuffers.CreateStructuredBuffer(sizeof(UINT));
	grass->vertexCounter->SetName(L"GrassAreaVertexCounter");
	grass->vertexCounterRead = resources.shaderBuffers.CreateStructuredBuffer(sizeof(UINT), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_HEAP_TYPE_READBACK);
	grass->vertexCounterRead->SetName(L"GrassAreaVertexCounterRead");
	grass->indexBuffer = indexBuffer.Get();

	return grasses.emplace_back(grass);
}

void GrassAreaGenerator::createIndexBuffer(RenderSystem& renderSystem, ResourceUploadBatch& batch)
{
	UINT maxWidth = 1;

	std::vector<uint32_t> data;
	data.resize(maxWidth * maxWidth * 6);

	for (int x = 0; x < maxWidth; x++)
		for (int y = 0; y < maxWidth; y++)
		{
			auto idx = x * maxWidth + y;
			auto bufferIdx = idx * 6;
			auto quadIdx = idx * 4;

			data[bufferIdx++] = quadIdx;
			data[bufferIdx++] = quadIdx + 1;
			data[bufferIdx++] = quadIdx + 2;

			data[bufferIdx++] = quadIdx + 2;
			data[bufferIdx++] = quadIdx + 1;
			data[bufferIdx] = quadIdx + 3;
		}

	CreateStaticBuffer(renderSystem.core.device, batch, data, D3D12_RESOURCE_STATE_INDEX_BUFFER, &indexBuffer);
	indexBuffer->SetName(L"TerrainChunkIB");
}
