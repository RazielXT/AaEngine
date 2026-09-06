#include "Scene/EntityInstancing.h"
#include "Scene/RenderWorld.h"
#include "Resources/Material/Material.h"
#include "RenderObject/Terrain/ProgressiveTerrain.h"

void InstancingManager::initialize(RenderSystem& rs, GraphicsResources& graphicsResources)
{
	renderSystem = &rs;
	resources = &graphicsResources;

	D3D12_INDIRECT_ARGUMENT_DESC argumentDesc{};
	argumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

	D3D12_COMMAND_SIGNATURE_DESC signatureDesc{};
	signatureDesc.pArgumentDescs = &argumentDesc;
	signatureDesc.NumArgumentDescs = 1;
	signatureDesc.ByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);

	rs.core.device->CreateCommandSignature(&signatureDesc, nullptr, IID_PPV_ARGS(&drawIndexedCommandSignature));
}

void InstancingManager::clear()
{
	groups.clear();
	updates.clear();
}

InstanceGroup* InstancingManager::getGroup(UINT id) const
{
	auto it = groups.find(id);
	return it == groups.end() ? nullptr : it->second.get();
}

void InstancingManager::updateEntity(ObjectId id, const ObjectTransformation& transform)
{
	updates.emplace_back(id, transform);
}

void InstancingManager::update()
{
	for (auto& info : updates)
	{
		auto& group = groups[info.id.getGroupId()];

		group->update(info.id.getLocalIdx(), info.transform);
	}

	updates.clear();
}

InstanceGroup* InstancingManager::build(RenderWorld& renderWorld, const InstanceGroupDescription& description)
{
	if (description.objects.empty())
		return nullptr;

	auto& it = groups[groups.size()];
	it = std::make_unique<InstanceGroup>();
	it->create(renderWorld, description, groups.size() - 1);

	return it.get();
}

InstanceGroup* InstancingManager::buildHeightmap(RenderWorld& renderWorld, const HeightmapInstanceGroupDescription& description)
{
	if (!renderSystem || !resources || !description.material || !description.model ||
		!description.sectorWorldSize || !description.maxInstancesPerSector ||
		!description.sectorCount.x || !description.sectorCount.y || description.placement.constants.empty())
		return nullptr;

	float sectorsPerTile = renderWorld.terrain.params.tileSize / description.sectorWorldSize;
	UINT roundedSectorsPerTile = (UINT)std::round(sectorsPerTile);
	if (std::abs(sectorsPerTile - roundedSectorsPerTile) > 0.001f ||
		!roundedSectorsPerTile || (roundedSectorsPerTile & (roundedSectorsPerTile - 1)))
		return nullptr;

	auto& group = groups[groups.size()];
	group = std::make_unique<InstanceGroup>();
	group->createHeightmap(renderWorld, *renderSystem, *resources, description, (UINT)groups.size() - 1, drawIndexedCommandSignature);
	return group.get();
}

void InstancingManager::updateHeightmap(ID3D12GraphicsCommandList* commandList, const Vector3& cameraPosition, const ProgressiveTerrain& terrain)
{
	for (auto& [id, group] : groups)
	{
		if (group->heightmapPlaced)
			group->updateHeightmap(commandList, cameraPosition, terrain);
	}
}

XM_ALIGNED_STRUCT(16) InstanceData
{
	XMFLOAT4X4 transform;
};

InstanceGroup::~InstanceGroup()
{
	if (gpuIdsBuffer)
		DescriptorManager::get().removeUAV(gpuIdsBufferView);
}

void InstanceGroup::create(RenderWorld& renderWorld, const InstanceGroupDescription& description, UINT groupIdx)
{
	count = description.objects.size();
	sectorWorldSize = description.sectorWorldSize;
	material = description.material;
	model = description.model;
	modelBbox = description.model->bbox;

	std::vector<UINT> ids;
	ids.resize(count);
	entities.resize(count);
	for (size_t i = 0; i < description.objects.size(); i++)
	{
		entities[i].transformation = description.objects[i];
		ids[i] = ObjectId(i, ObjectType::Instanced, groupIdx).value;
	}
	gpuIdsBuffer = ShaderDataBuffers::get().CreateUploadStructuredBuffer(ids.data(), sizeof(UINT) * ids.size());
	gpuIdsBufferView = DescriptorManager::get().createBufferView(gpuIdsBuffer.Get(), sizeof(UINT), count);

	createSectors(renderWorld, groupIdx);
}

void InstanceGroup::update(UINT idx, ObjectTransformation transform)
{
	auto sectorIdx = entities[idx].sectorIndex;
	entities[idx].transformation = transform;
	rebuildSector(sectorIdx);
	updateSectorBbox(sectorIdx);
}

void InstanceGroup::updateBbox()
{
	for (UINT i = 0; i < sectors.size(); i++)
		updateSectorBbox(i);
}

RenderEntity* InstanceGroup::getEntity(UINT instanceIdx) const
{
	return sectors[entities[instanceIdx].sectorIndex].entity;
}

void InstanceGroup::createSectors(RenderWorld& renderWorld, UINT groupIdx)
{
	std::map<std::pair<int, int>, UINT> sectorByCoord;

	for (UINT i = 0; i < entities.size(); i++)
	{
		auto& position = entities[i].transformation.position;
		XMINT2 coord{};
		if (sectorWorldSize > 0)
		{
			coord.x = (int)std::floor(position.x / sectorWorldSize);
			coord.y = (int)std::floor(position.z / sectorWorldSize);
		}

		auto key = std::make_pair(coord.x, coord.y);
		auto found = sectorByCoord.find(key);
		if (found == sectorByCoord.end())
		{
			UINT sectorIdx = (UINT)sectors.size();
			sectorByCoord.emplace(key, sectorIdx);
			sectors.push_back({});
			sectors.back().worldCoord = coord;
			found = sectorByCoord.find(key);
		}

		entities[i].sectorIndex = found->second;
		sectors[found->second].instanceIndices.push_back(i);
	}

	for (UINT i = 0; i < sectors.size(); i++)
	{
		auto& sector = sectors[i];
		rebuildSector(i);

		sector.entity = renderWorld.createEntity({ Order::Normal, (uint16_t)groupIdx });
		sector.entity->geometry.fromInstancedModel(*model, (UINT)sector.instanceIndices.size(), sector.gpuBuffer->GetGPUVirtualAddress());
		sector.entity->material = material;
		sector.entity->setScale(Vector3::One);
		sector.entity->setOrientation(Quaternion::Identity);
		updateSectorBbox(i);
	}

	entity = sectors.front().entity;
	gpuBuffer = sectors.front().gpuBuffer;
}

void InstanceGroup::rebuildSector(UINT sectorIdx)
{
	auto& sector = sectors[sectorIdx];
	std::vector<InstanceData> data(sector.instanceIndices.size());
	for (UINT i = 0; i < sector.instanceIndices.size(); i++)
	{
		auto instanceIdx = sector.instanceIndices[i];
		XMStoreFloat4x4(&data[i].transform, XMMatrixTranspose(entities[instanceIdx].transformation.createWorldMatrix()));
	}

	sector.gpuBuffer = ShaderDataBuffers::get().CreateUploadStructuredBuffer(data.data(), sizeof(InstanceData) * data.size());
	if (sector.entity)
	{
		sector.entity->geometry.geometryCustomBuffer = sector.gpuBuffer->GetGPUVirtualAddress();
		sector.entity->geometry.instanceCount = (UINT)data.size();
	}
}

void InstanceGroup::updateSectorBbox(UINT sectorIdx)
{
	auto& sector = sectors[sectorIdx];
	const auto& first = entities[sector.instanceIndices.front()].transformation;
	BoundingBoxVolume volume{ first.position, first.position };
	for (auto instanceIdx : sector.instanceIndices)
		volume.add(entities[instanceIdx].transformation.position);

	auto bbox = volume.createBbox();
	bbox.Center += first.orientation * modelBbox.Center;
	bbox.Extents += modelBbox.Extents;
	sector.entity->setPosition(bbox.Center);
	bbox.Center = {};
	sector.entity->setBoundingBox(bbox);
}

void InstanceGroup::createHeightmap(
	RenderWorld& renderWorld,
	RenderSystem& renderSystem,
	GraphicsResources& resources,
	const HeightmapInstanceGroupDescription& description,
	UINT groupIdx,
	ComPtr<ID3D12CommandSignature> commandSignature)
{
	heightmapPlaced = true;
	material = description.material;
	model = description.model;
	modelBbox = description.model->bbox;
	sectorGridSize = description.sectorCount;
	sectorWorldSize = description.sectorWorldSize;
	maxInstancesPerSector = description.maxInstancesPerSector;
	generatedInstanceBounds = description.instanceBounds;
	placementConstants = description.placement.constants;

	auto shaderName = description.placement.name.empty() ? "slopePlacementCS" : description.placement.name;
	auto shaderRef = description.placement.shader;
	if (shaderRef.file.empty())
		shaderRef = { "instancing/slopePlacementCS.hlsl", "main", "cs_6_6" };

	placementShader = std::make_unique<HeightmapPlacementComputeShader>();
	auto shader = resources.shaders.getShader(shaderName, ShaderType::Compute, shaderRef);
	placementShader->init(*renderSystem.core.device, *shader);

	UINT sectorCount = sectorGridSize.x * sectorGridSize.y;
	generatedInstanceBuffer = resources.shaderBuffers.CreateStructuredBuffer(
		sectorCount * maxInstancesPerSector * sizeof(XMFLOAT4X4));
	generatedInstanceBuffer->SetName(L"HeightmapPlacedInstances");
	generatedCommandBuffer = resources.shaderBuffers.CreateStructuredBuffer(
		sectorCount * sizeof(D3D12_DRAW_INDEXED_ARGUMENTS));
	generatedCommandBuffer->SetName(L"HeightmapPlacedCommands");

	sectors.resize(sectorCount);
	int halfX = (int)sectorGridSize.x / 2;
	int halfY = (int)sectorGridSize.y / 2;
	for (int x = 0; x < (int)sectorGridSize.x; x++)
	{
		for (int y = 0; y < (int)sectorGridSize.y; y++)
		{
			XMINT2 desired{ x - halfX, y - halfY };
			int ax = TerrainGridParams::wrapIndex(desired.x, sectorGridSize.x);
			int ay = TerrainGridParams::wrapIndex(desired.y, sectorGridSize.y);
			UINT sectorIdx = ax * sectorGridSize.y + ay;
			auto& sector = sectors[sectorIdx];
			sector.worldCoord = desired;
			sector.dirty = true;

			sector.indirect.commandSignature = commandSignature;
			sector.indirect.commandBuffer = generatedCommandBuffer;
			sector.indirect.maxCommands = 1;
			sector.indirect.commandBufferOffset = sectorIdx * sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);

			sector.entity = renderWorld.createEntity({ Order::Normal, (uint16_t)groupIdx });
			sector.entity->material = material;
			sector.entity->geometry.type = EntityGeometry::Type::Indirect;
			sector.entity->geometry.vertexBufferView = model->vertexBufferView;
			sector.entity->geometry.indexBufferView = model->indexBufferView;
			sector.entity->geometry.indexCount = model->indexCount;
			sector.entity->geometry.layout = &model->vertexLayout;
			sector.entity->geometry.source = &sector.indirect;
			sector.entity->geometry.geometryCustomBuffer = generatedInstanceBuffer->GetGPUVirtualAddress() +
				(UINT64)sectorIdx * maxInstancesPerSector * sizeof(XMFLOAT4X4);
			sector.entity->setScale(Vector3::One);
			sector.entity->setOrientation(Quaternion::Identity);
		}
	}

	entity = sectors.front().entity;
	updateHeightmapSectorPositions(Vector3::Zero, renderWorld.terrain);
}

void InstanceGroup::updateHeightmap(ID3D12GraphicsCommandList* commandList, const Vector3& cameraPosition, const ProgressiveTerrain& terrain)
{
	updateHeightmapSectorPositions(cameraPosition, terrain);

	UINT sectorsPerTerrainTile = (UINT)std::round(terrain.params.tileSize / sectorWorldSize);
	for (auto regenerated : terrain.regeneratedChunks)
	{
		for (auto& sector : sectors)
		{
			XMINT2 terrainChunk{
				(int)std::floor((float)sector.worldCoord.x / sectorsPerTerrainTile),
				(int)std::floor((float)sector.worldCoord.y / sectorsPerTerrainTile)
			};
			if (terrainChunk.x == regenerated.x && terrainChunk.y == regenerated.y)
				sector.dirty = true;
		}
	}

	regenerateHeightmapSectors(commandList, terrain);
}

void InstanceGroup::updateHeightmapSectorPositions(const Vector3& cameraPosition, const ProgressiveTerrain& terrain)
{
	XMINT2 centerX = terrain.params.gridCenterAt(cameraPosition, sectorWorldSize, sectorGridSize.x);
	XMINT2 centerY = terrain.params.gridCenterAt(cameraPosition, sectorWorldSize, sectorGridSize.y);
	XMINT2 newCenter{ centerX.x, centerY.y };
	if (newCenter.x == sectorGridCenter.x && newCenter.y == sectorGridCenter.y && generatedBuffersInitialized)
		return;

	sectorGridCenter = newCenter;
	int halfX = (int)sectorGridSize.x / 2;
	int halfY = (int)sectorGridSize.y / 2;
	for (int x = 0; x < (int)sectorGridSize.x; x++)
	{
		for (int y = 0; y < (int)sectorGridSize.y; y++)
		{
			XMINT2 desired{ newCenter.x + x - halfX, newCenter.y + y - halfY };
			int ax = TerrainGridParams::wrapIndex(desired.x, sectorGridSize.x);
			int ay = TerrainGridParams::wrapIndex(desired.y, sectorGridSize.y);
			auto& sector = sectors[ax * sectorGridSize.y + ay];
			if (sector.worldCoord.x == desired.x && sector.worldCoord.y == desired.y && generatedBuffersInitialized)
				continue;

			sector.worldCoord = desired;
			sector.dirty = true;
			auto worldMin = terrain.params.chunkWorldMin(desired, sectorWorldSize);
			sector.entity->setPosition(worldMin);

			float halfSize = sectorWorldSize * 0.5f;
			BoundingBox bbox;
			bbox.Center = { halfSize, terrain.params.tileHeight * 0.5f, halfSize };
			bbox.Extents = {
				halfSize + generatedInstanceBounds.Extents.x,
				terrain.params.tileHeight * 0.5f + generatedInstanceBounds.Extents.y,
				halfSize + generatedInstanceBounds.Extents.z
			};
			sector.entity->setBoundingBox(bbox);
		}
	}
}

void InstanceGroup::regenerateHeightmapSectors(ID3D12GraphicsCommandList* commandList, const ProgressiveTerrain& terrain)
{
	bool hasDirty = std::any_of(sectors.begin(), sectors.end(), [](const InstanceSector& sector) { return sector.dirty; });
	if (!hasDirty)
		return;

	D3D12_RESOURCE_STATES previousInstanceState = generatedBuffersInitialized
		? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
		: D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_STATES previousCommandState = generatedBuffersInitialized
		? D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT
		: D3D12_RESOURCE_STATE_COMMON;
	CD3DX12_RESOURCE_BARRIER toUav[] = {
		CD3DX12_RESOURCE_BARRIER::Transition(generatedInstanceBuffer.Get(), previousInstanceState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
		CD3DX12_RESOURCE_BARRIER::Transition(generatedCommandBuffer.Get(), previousCommandState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
	};
	commandList->ResourceBarrier(_countof(toUav), toUav);

	UINT sectorsPerTerrainTile = (UINT)std::round(terrain.params.tileSize / sectorWorldSize);
	float uvScale = 1.0f / sectorsPerTerrainTile;
	for (UINT sectorIdx = 0; sectorIdx < sectors.size(); sectorIdx++)
	{
		auto& sector = sectors[sectorIdx];
		if (!sector.dirty)
			continue;

		XMINT2 terrainChunk{
			(int)std::floor((float)sector.worldCoord.x / sectorsPerTerrainTile),
			(int)std::floor((float)sector.worldCoord.y / sectorsPerTerrainTile)
		};
		int subX = TerrainGridParams::wrapIndex(sector.worldCoord.x, sectorsPerTerrainTile);
		int subY = TerrainGridParams::wrapIndex(sector.worldCoord.y, sectorsPerTerrainTile);
		auto sectorMin = terrain.params.chunkWorldMin(sector.worldCoord, sectorWorldSize);

		HeightmapPlacementComputeShader::Input input{};
		input.terrainTexture = terrain.getHeightmapSrvIndex(terrainChunk);
		input.terrainHeight = terrain.params.tileHeight;
		input.terrainWorldMin = { terrain.params.tileCenterOffset.x, terrain.params.tileCenterOffset.y };
		input.sectorWorldMin = { sectorMin.x, sectorMin.z };
		input.sectorWorldSize = sectorWorldSize;
		input.maxInstances = maxInstancesPerSector;
		input.heightmapUvMin = { subX * uvScale, subY * uvScale };
		input.heightmapUvSize = { uvScale, uvScale };
		input.seed = (UINT)(sector.worldCoord.x * 73856093 ^ sector.worldCoord.y * 19349663);
		input.indexCount = model->indexCount;

		placementShader->dispatch(
			commandList,
			input,
			placementConstants,
			generatedInstanceBuffer->GetGPUVirtualAddress() + (UINT64)sectorIdx * maxInstancesPerSector * sizeof(XMFLOAT4X4),
			generatedCommandBuffer->GetGPUVirtualAddress() + (UINT64)sectorIdx * sizeof(D3D12_DRAW_INDEXED_ARGUMENTS));
		sector.dirty = false;
	}

	CD3DX12_RESOURCE_BARRIER uavBarriers[] = {
		CD3DX12_RESOURCE_BARRIER::UAV(generatedInstanceBuffer.Get()),
		CD3DX12_RESOURCE_BARRIER::UAV(generatedCommandBuffer.Get())
	};
	commandList->ResourceBarrier(_countof(uavBarriers), uavBarriers);

	CD3DX12_RESOURCE_BARRIER toSrv[] = {
		CD3DX12_RESOURCE_BARRIER::Transition(generatedInstanceBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
		CD3DX12_RESOURCE_BARRIER::Transition(generatedCommandBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT)
	};
	commandList->ResourceBarrier(_countof(toSrv), toSrv);
	generatedBuffersInitialized = true;
}
