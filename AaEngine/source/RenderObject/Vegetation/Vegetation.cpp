#include "RenderObject/Vegetation/Vegetation.h"
#include "RenderObject/Terrain/ProgressiveTerrain.h"
#include "BufferHelpers.h"
#include "Scene/RenderWorld.h"
#include <format>
#include <climits>

Vegetation::Vegetation()
{
}

void Vegetation::initialize(RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch)
{
	auto csShader = resources.shaders.getShader("vegetationFindCS", ShaderType::Compute, ShaderRef{ "terrain/vegetation/vegetationFindCS.hlsl", "main", "cs_6_6" });
	vegetationFindCS.init(*renderSystem.core.device, *csShader);

	csShader = resources.shaders.getShader("vegetationUpdateCS", ShaderType::Compute, ShaderRef{ "terrain/vegetation/vegetationUpdateCS.hlsl", "main", "cs_6_6" });
	vegetationUpdateCS.init(*renderSystem.core.device, *csShader);

	csShader = resources.shaders.getShader("indirectDrawIndexedClearCS", ShaderType::Compute);
	indirectDrawClearCS.init(*renderSystem.core.device, *csShader);

	noiseTexture = resources.textures.loadFile(*renderSystem.core.device, batch, "perlinNoiseLow.dds");
	resources.descriptors.createTextureView(*noiseTexture);

	createBillboardIndexBuffer(renderSystem, batch);
	initializeImpostors(renderSystem, batch);
}

void Vegetation::enableUpdating(bool enabled)
{
	updatingEnabled = enabled;
}

void Vegetation::clear()
{
	for (int x = 0; x < VegGridSize; x++)
		for (int y = 0; y < VegGridSize; y++)
		{
			tiles[x][y] = {};
		}
}

const UINT groups = 4;
const UINT threads = 8;
const UINT perThread = 8;

struct VegetationInfo
{
	Vector3 position;
	float rotation;
	float scale;
	float random;
};

struct SubgroupMeta
{
	INT counter;
	INT minY;
	INT maxY;
	INT _pad;
	float worldMinX;
	float worldMinZ;
	float _pad1;
	float _pad2;
};

void Vegetation::createSharedBuffers(GraphicsResources& resources)
{
	constexpr UINT infoSize = sizeof(VegetationInfo) * MaxTotalItems;
	infoBuffer = resources.shaderBuffers.CreateStructuredBuffer(infoSize);
	infoBuffer->SetName(L"VegetationInfo");

	subgroupMetaBuffer = resources.shaderBuffers.CreateStructuredBuffer(sizeof(SubgroupMeta) * TotalSubgroups);
	subgroupMetaBuffer->SetName(L"VegetationSubgroupMeta");

	for (UINT v = 0; v < ViewCount; v++)
	{
		redirectBuffer[v] = resources.shaderBuffers.CreateStructuredBuffer(sizeof(UINT) * MaxTotalItems);
		redirectBuffer[v]->SetName(L"VegetationRedirect");
	}
}

void Vegetation::createChunks(RenderWorld& renderWorld, RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch)
{
	vegMaterial = resources.materials.getMaterial("VegetationBillboard", batch);
	auto& terrainParams = renderWorld.terrain.params;
	float chunkSize = terrainParams.tileSize / ChunksPerTerrainTile;
	int half = (int)VegGridSize / 2;

	createSharedBuffers(resources);

	gridCenterChunk = terrainParams.worldCoordAt(Vector3::Zero, chunkSize);

	for (int x = 0; x < (int)VegGridSize; x++)
		for (int y = 0; y < (int)VegGridSize; y++)
		{
			XMINT2 desired = { gridCenterChunk.x + x - half, gridCenterChunk.y + y - half };
			int ax = TerrainGridParams::wrapIndex(desired.x, VegGridSize);
			int ay = TerrainGridParams::wrapIndex(desired.y, VegGridSize);

			auto& tile = tiles[ax][ay];
			tile.worldCoord = desired;
			tile.dirty = true;
		}

	// Single indirect draw per view over the whole shared buffer.
	for (UINT v = 0; v < ViewCount; v++)
	{
		indirect[v].commandSignature = commandSignature;
		indirect[v].maxCommands = 1;
		indirect[v].commandBuffer = sharedCommandBuffer;
		indirect[v].commandBufferOffset = v * sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
	}

	// One main entity drives the whole vegetation grid (view 0 == default).
	auto e = renderWorld.createEntity();
	entity = e;
	e->material = vegMaterial;
	e->geometry.type = EntityGeometry::Type::Indirect;
	e->geometry.source = &indirect[0];
	e->geometry.geometryCustomBuffer = infoBuffer->GetGPUVirtualAddress();
	e->geometry.geometryRedirectBuffer = redirectBuffer[0]->GetGPUVirtualAddress();

	e->geometry.indexCount = 6;
	e->geometry.indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
	e->geometry.indexBufferView.SizeInBytes = e->geometry.indexCount * sizeof(uint16_t);
	e->geometry.indexBufferView.Format = DXGI_FORMAT_R16_UINT;

	float gridHalf = chunkSize * VegGridSize * 0.5f;
	e->setBoundingBox({ { gridHalf, terrainParams.tileHeight * 0.5f, gridHalf },
					   { gridHalf, terrainParams.tileHeight * 0.5f, gridHalf } });

	// Keep the single entity centered on the grid so its bounding box tracks the camera.
	XMINT2 minCoord = { gridCenterChunk.x - half, gridCenterChunk.y - half };
	e->setPosition(terrainParams.chunkWorldMin(minCoord, chunkSize));
}

void Vegetation::regenerateDirtyTiles(ID3D12GraphicsCommandList* commandList, const ProgressiveTerrain& terrain)
{
	auto& terrainParams = terrain.params;
	float chunkSize = terrainParams.tileSize / ChunksPerTerrainTile;
	float uvScale = 1.0f / ChunksPerTerrainTile;

	// Collect dirty tiles so buffer transitions can be batched into a single find pass.
	struct DirtyTile { VegetationTile* tile; UINT chunkIndex; };
	std::vector<DirtyTile> dirty;
	for (int x = 0; x < (int)VegGridSize; x++)
		for (int y = 0; y < (int)VegGridSize; y++)
		{
			auto& tile = tiles[x][y];
			if (tile.dirty)
				dirty.push_back({ &tile, (UINT)(x * VegGridSize + y) });
		}

	if (dirty.empty())
		return;

	// Reset only the metadata of the regenerating chunks (counters=0, minY=INT_MAX, maxY=INT_MIN).
	CD3DX12_RESOURCE_BARRIER toCopyDest = CD3DX12_RESOURCE_BARRIER::Transition(subgroupMetaBuffer.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
	commandList->ResourceBarrier(1, &toCopyDest);

	for (auto& d : dirty)
	{
		UINT64 dstOffset = (UINT64)d.chunkIndex * SubgroupCount * sizeof(SubgroupMeta);
		commandList->CopyBufferRegion(subgroupMetaBuffer.Get(), dstOffset, defaultMetaBuffer.Get(), 0, sizeof(SubgroupMeta) * SubgroupCount);
	}

	CD3DX12_RESOURCE_BARRIER toUav[2]{};
	toUav[0] = CD3DX12_RESOURCE_BARRIER::Transition(infoBuffer.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	toUav[1] = CD3DX12_RESOURCE_BARRIER::Transition(subgroupMetaBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->ResourceBarrier(2, toUav);

	for (auto& d : dirty)
	{
		auto& tile = *d.tile;

		XMINT2 terrainChunk = {
			(int)std::floor((float)tile.worldCoord.x / ChunksPerTerrainTile),
			(int)std::floor((float)tile.worldCoord.y / ChunksPerTerrainTile)
		};

		UINT heightmapSrv = terrain.getHeightmapSrvIndex(terrainChunk);

		int subX = ((tile.worldCoord.x % (int)ChunksPerTerrainTile) + (int)ChunksPerTerrainTile) % (int)ChunksPerTerrainTile;
		int subY = ((tile.worldCoord.y % (int)ChunksPerTerrainTile) + (int)ChunksPerTerrainTile) % (int)ChunksPerTerrainTile;

		VegetationFindComputeShader::Input input;
		input.terrainTexture = heightmapSrv;
		input.terrainHeight = terrainParams.tileHeight;

		auto worldMin = terrainParams.chunkWorldMin(tile.worldCoord, chunkSize);
		input.terrainOffset = { worldMin.x + chunkSize * 0.5f, worldMin.z + chunkSize * 0.5f };
		input.chunkWorldSize = { chunkSize, chunkSize };
		input.subUvOffset = { subX * uvScale, subY * uvScale };
		input.subUvScale = { uvScale, uvScale };
		input.noiseTexture = noiseTexture->srvHeapIndex;
		input.subgroupBase = d.chunkIndex * SubgroupCount;

		vegetationFindCS.dispatch(commandList, input, infoBuffer.Get(), subgroupMetaBuffer.Get());

		d.tile->dirty = false;
	}

	CD3DX12_RESOURCE_BARRIER uavBarriers[2]{};
	uavBarriers[0] = CD3DX12_RESOURCE_BARRIER::UAV(infoBuffer.Get());
	uavBarriers[1] = CD3DX12_RESOURCE_BARRIER::UAV(subgroupMetaBuffer.Get());
	commandList->ResourceBarrier(2, uavBarriers);

	CD3DX12_RESOURCE_BARRIER toSrv[2]{};
	toSrv[0] = CD3DX12_RESOURCE_BARRIER::Transition(infoBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	toSrv[1] = CD3DX12_RESOURCE_BARRIER::Transition(subgroupMetaBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(2, toSrv);
}

void Vegetation::update(ID3D12GraphicsCommandList* commandList, const Vector3& cameraPos, const ProgressiveTerrain& terrain)
{
	if (!updatingEnabled)
		return;

	auto& terrainParams = terrain.params;
	float chunkSize = terrainParams.tileSize / ChunksPerTerrainTile;
	int half = (int)VegGridSize / 2;

	XMINT2 cameraChunk = terrainParams.gridCenterAt(cameraPos, chunkSize, VegGridSize);

	if (cameraChunk.x != gridCenterChunk.x || cameraChunk.y != gridCenterChunk.y)
	{
		gridCenterChunk = cameraChunk;

		for (int x = 0; x < (int)VegGridSize; x++)
			for (int y = 0; y < (int)VegGridSize; y++)
			{
				XMINT2 desired = { gridCenterChunk.x + x - half, gridCenterChunk.y + y - half };
				int ax = TerrainGridParams::wrapIndex(desired.x, VegGridSize);
				int ay = TerrainGridParams::wrapIndex(desired.y, VegGridSize);

				auto& tile = tiles[ax][ay];
				if (tile.worldCoord.x != desired.x || tile.worldCoord.y != desired.y)
				{
					tile.worldCoord = desired;
					tile.dirty = true;
				}
			}

		// Keep the single entity centered on the grid so its bounding box tracks the camera.
		XMINT2 minCoord = { gridCenterChunk.x - half, gridCenterChunk.y - half };
		entity->setPosition(terrainParams.chunkWorldMin(minCoord, chunkSize));
	}

	regenerateDirtyTiles(commandList, terrain);
}

void Vegetation::updateCulling(ID3D12GraphicsCommandList* commandList, const Camera& camera, const ProgressiveTerrain& terrain, UINT viewIndex)
{
	if (!updatingEnabled)
		return;

	CommandsMarker marker(commandList, "Trees", PixColor::ForestGreen);

	auto& terrainParams = terrain.params;
	float chunkSize = terrainParams.tileSize / ChunksPerTerrainTile;

	VegetationUpdateComputeShader::Input cullingInput{};
	camera.extractFrustumPlanes(cullingInput.frustumPlanes);
	cullingInput.chunkSize = chunkSize;

	D3D12_GPU_VIRTUAL_ADDRESS commandsAddr = sharedCommandBuffer->GetGPUVirtualAddress() + viewIndex * sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);

	// Clear this view's InstanceCount before appending.
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(sharedCommandBuffer.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->ResourceBarrier(1, &barrier);

	indirectDrawClearCS.dispatch(commandList, ViewCount, sharedCommandBuffer.Get());

	CD3DX12_RESOURCE_BARRIER uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(sharedCommandBuffer.Get());
	commandList->ResourceBarrier(1, &uavBarrier);

	// One dispatch culls every subgroup of every chunk into the shared redirect buffer.
	barrier = CD3DX12_RESOURCE_BARRIER::Transition(redirectBuffer[viewIndex].Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->ResourceBarrier(1, &barrier);

	vegetationUpdateCS.dispatch(commandList, cullingInput, subgroupMetaBuffer.Get(), commandsAddr, redirectBuffer[viewIndex].Get(), TotalSubgroups);

	barrier = CD3DX12_RESOURCE_BARRIER::Transition(redirectBuffer[viewIndex].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &barrier);

	barrier = CD3DX12_RESOURCE_BARRIER::Transition(sharedCommandBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &barrier);
}

void Vegetation::initializeImpostors(RenderSystem& renderSystem, ResourceUploadBatch& batch)
{
	D3D12_INDIRECT_ARGUMENT_DESC argumentDescs[1] = {};
	argumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

	D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
	commandSignatureDesc.pArgumentDescs = argumentDescs;
	commandSignatureDesc.NumArgumentDescs = _countof(argumentDescs);
	commandSignatureDesc.ByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);

	renderSystem.core.device->CreateCommandSignature(&commandSignatureDesc, nullptr, IID_PPV_ARGS(&commandSignature));

	// Default subgroup metadata: counter=0, minY=INT_MAX, maxY=INT_MIN
	std::vector<SubgroupMeta> defaultMeta(SubgroupCount);
	for (auto& meta : defaultMeta)
	{
		meta.counter = 0;
		meta.minY = INT_MAX;
		meta.maxY = INT_MIN;
		meta._pad = 0;
		meta.worldMinX = 0;
		meta.worldMinZ = 0;
		meta._pad1 = 0;
		meta._pad2 = 0;
	}
	CreateStaticBuffer(renderSystem.core.device, batch, defaultMeta.data(), SubgroupCount, D3D12_RESOURCE_STATE_COPY_SOURCE, &defaultMetaBuffer);

	// Shared command buffer, one draw command per view — pre-fill IndexCountPerInstance=6, rest=0
	std::vector<D3D12_DRAW_INDEXED_ARGUMENTS> defaultCommands(ViewCount);
	for (auto& cmd : defaultCommands)
	{
		cmd.IndexCountPerInstance = 6;
		cmd.InstanceCount = 0;
		cmd.StartIndexLocation = 0;
		cmd.BaseVertexLocation = 0;
		cmd.StartInstanceLocation = 0;
	}
	CreateStaticBuffer(renderSystem.core.device, batch, defaultCommands.data(), ViewCount, sizeof(D3D12_DRAW_INDEXED_ARGUMENTS),
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, &sharedCommandBuffer, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	sharedCommandBuffer->SetName(L"VegetationSharedCommands");
}

void VegetationFindComputeShader::dispatch(ID3D12GraphicsCommandList* commandList, const Input& input, ID3D12Resource* infoBuffer, ID3D12Resource* subgroupMeta)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	commandList->SetComputeRoot32BitConstants(0, sizeof(input) / sizeof(float), &input, 0);
	commandList->SetComputeRootUnorderedAccessView(1, infoBuffer->GetGPUVirtualAddress());
	commandList->SetComputeRootUnorderedAccessView(2, subgroupMeta->GetGPUVirtualAddress());

	commandList->Dispatch(groups, groups, 1);
}

void VegetationUpdateComputeShader::dispatch(ID3D12GraphicsCommandList* commandList, const Input& input, ID3D12Resource* subgroupMeta, D3D12_GPU_VIRTUAL_ADDRESS commandsAddr, ID3D12Resource* redirectBuffer, UINT subgroupCount)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	commandList->SetComputeRoot32BitConstants(0, sizeof(input) / sizeof(float), &input, 0);
	commandList->SetComputeRootShaderResourceView(1, subgroupMeta->GetGPUVirtualAddress());
	commandList->SetComputeRootUnorderedAccessView(2, commandsAddr);
	commandList->SetComputeRootUnorderedAccessView(3, redirectBuffer->GetGPUVirtualAddress());

	commandList->Dispatch(subgroupCount, 1, 1);
}

void Vegetation::createBillboardIndexBuffer(RenderSystem& renderSystem, ResourceUploadBatch& batch)
{
	std::vector<uint16_t> data = { 0, 1, 2, 2, 1, 3 };

	CreateStaticBuffer(renderSystem.core.device, batch, data, D3D12_RESOURCE_STATE_INDEX_BUFFER, &indexBuffer);
	indexBuffer->SetName(L"VegetationBillboardIB");
}
