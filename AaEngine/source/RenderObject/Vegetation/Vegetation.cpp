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
			chunks[x][y] = {};
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
};

void Vegetation::initChunk(VegetationChunk& chunk, RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch, MaterialInstance* material)
{
	constexpr UINT maxCount = SubgroupCount * MaxItemsPerSubgroup;

	constexpr UINT infoSize = sizeof(VegetationInfo) * maxCount;
	chunk.infoBuffer = resources.shaderBuffers.CreateStructuredBuffer(infoSize);
	chunk.infoBuffer->SetName(L"VegetationInfo");

	chunk.subgroupMetaBuffer = resources.shaderBuffers.CreateStructuredBuffer(sizeof(SubgroupMeta) * SubgroupCount);
	chunk.subgroupMetaBuffer->SetName(L"VegetationSubgroupMeta");

	chunk.redirectBuffer = resources.shaderBuffers.CreateStructuredBuffer(sizeof(UINT) * maxCount);
	chunk.redirectBuffer->SetName(L"VegetationRedirect");

	chunk.impostors.commandSignature = commandSignature;
	chunk.impostors.maxCommands = 1;
}

void Vegetation::createChunks(RenderWorld& renderWorld, RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch)
{
	vegMaterial = resources.materials.getMaterial("VegetationBillboard", batch);
	auto& terrainParams = renderWorld.terrain.params;
	float chunkSize = terrainParams.tileSize / ChunksPerTerrainTile;
	int half = (int)VegGridSize / 2;

	gridCenterChunk = terrainParams.worldCoordAt(Vector3::Zero, chunkSize);

	for (int x = 0; x < (int)VegGridSize; x++)
		for (int y = 0; y < (int)VegGridSize; y++)
		{
			XMINT2 desired = { gridCenterChunk.x + x - half, gridCenterChunk.y + y - half };
			int ax = TerrainGridParams::wrapIndex(desired.x, VegGridSize);
			int ay = TerrainGridParams::wrapIndex(desired.y, VegGridSize);

			auto& chunk = chunks[ax][ay];
			initChunk(chunk, renderSystem, resources, batch, vegMaterial);

			UINT chunkIndex = ax * VegGridSize + ay;
			chunk.impostors.commandBuffer = sharedCommandBuffer;
			chunk.impostors.commandBufferOffset = chunkIndex * sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);

			auto e = renderWorld.createEntity();
			chunk.entity = e;
			e->material = vegMaterial;
			e->Material().setParam("ChunkId", x + y * VegGridSize);
			e->geometry.type = EntityGeometry::Type::Indirect;
			e->geometry.source = &chunk.impostors;
			e->geometry.geometryCustomBuffer = chunk.infoBuffer->GetGPUVirtualAddress();
			e->geometry.geometryRedirectBuffer = chunk.redirectBuffer->GetGPUVirtualAddress();

			e->geometry.indexCount = 6;
			e->geometry.indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
			e->geometry.indexBufferView.SizeInBytes = e->geometry.indexCount * sizeof(uint16_t);
			e->geometry.indexBufferView.Format = DXGI_FORMAT_R16_UINT;

			e->setBoundingBox({ { chunkSize * 0.5f, terrainParams.tileHeight * 0.5f, chunkSize * 0.5f },
							   { chunkSize * 0.5f, terrainParams.tileHeight * 0.5f, chunkSize * 0.5f } });

			chunk.worldCoord = desired;
			chunk.dirty = true;

			e->setPosition(terrainParams.chunkWorldMin(desired, chunkSize));
		}
}

void Vegetation::regenerateChunk(ID3D12GraphicsCommandList* commandList, VegetationChunk& chunk, const ProgressiveTerrain& terrain)
{
	auto& terrainParams = terrain.params;
	float chunkSize = terrainParams.tileSize / ChunksPerTerrainTile;

	// Map vegetation chunk world coord to terrain chunk world coord
	XMINT2 terrainChunk = {
		(int)std::floor((float)chunk.worldCoord.x / ChunksPerTerrainTile),
		(int)std::floor((float)chunk.worldCoord.y / ChunksPerTerrainTile)
	};

	UINT heightmapSrv = terrain.getHeightmapSrvIndex(terrainChunk);

	// Compute UV sub-region within the terrain tile for this vegetation chunk
	int subX = ((chunk.worldCoord.x % (int)ChunksPerTerrainTile) + (int)ChunksPerTerrainTile) % (int)ChunksPerTerrainTile;
	int subY = ((chunk.worldCoord.y % (int)ChunksPerTerrainTile) + (int)ChunksPerTerrainTile) % (int)ChunksPerTerrainTile;

	float uvScale = 1.0f / ChunksPerTerrainTile;

	VegetationFindComputeShader::Input input;
	input.terrainTexture = heightmapSrv;
	input.terrainHeight = terrainParams.tileHeight;

	auto worldMin = terrainParams.chunkWorldMin(chunk.worldCoord, chunkSize);
	input.terrainOffset = { worldMin.x + chunkSize * 0.5f, worldMin.z + chunkSize * 0.5f };
	input.chunkWorldSize = { chunkSize, chunkSize };
	input.subUvOffset = { subX * uvScale, subY * uvScale };
	input.subUvScale = { uvScale, uvScale };

	input.noiseTexture = noiseTexture->srvHeapIndex;

	// Clear subgroup metadata (counters=0, minY=INT_MAX, maxY=INT_MIN) before dispatching find CS
	CD3DX12_RESOURCE_BARRIER toCopyDest = CD3DX12_RESOURCE_BARRIER::Transition(chunk.subgroupMetaBuffer.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
	commandList->ResourceBarrier(1, &toCopyDest);
	commandList->CopyBufferRegion(chunk.subgroupMetaBuffer.Get(), 0, defaultMetaBuffer.Get(), 0, sizeof(SubgroupMeta) * SubgroupCount);

	CD3DX12_RESOURCE_BARRIER toUav[2]{};
	toUav[0] = CD3DX12_RESOURCE_BARRIER::Transition(chunk.infoBuffer.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	toUav[1] = CD3DX12_RESOURCE_BARRIER::Transition(chunk.subgroupMetaBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->ResourceBarrier(2, toUav);

	vegetationFindCS.dispatch(commandList, input, chunk.infoBuffer.Get(), chunk.subgroupMetaBuffer.Get());

	// UAV barrier to ensure find CS writes complete before update CS reads
	CD3DX12_RESOURCE_BARRIER uavBarriers[2]{};
	uavBarriers[0] = CD3DX12_RESOURCE_BARRIER::UAV(chunk.infoBuffer.Get());
	uavBarriers[1] = CD3DX12_RESOURCE_BARRIER::UAV(chunk.subgroupMetaBuffer.Get());
	commandList->ResourceBarrier(2, uavBarriers);

	CD3DX12_RESOURCE_BARRIER toSrv[2]{};
	toSrv[0] = CD3DX12_RESOURCE_BARRIER::Transition(chunk.infoBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	toSrv[1] = CD3DX12_RESOURCE_BARRIER::Transition(chunk.subgroupMetaBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(2, toSrv);
}

void Vegetation::updateChunk(ID3D12GraphicsCommandList* commandList, VegetationChunk& chunk, const VegetationUpdateComputeShader::Input& cullingInput)
{
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(chunk.redirectBuffer.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->ResourceBarrier(1, &barrier);

	D3D12_GPU_VIRTUAL_ADDRESS commandsAddr = sharedCommandBuffer->GetGPUVirtualAddress() + chunk.impostors.commandBufferOffset;
	vegetationUpdateCS.dispatch(commandList, cullingInput, chunk.subgroupMetaBuffer.Get(), commandsAddr, chunk.redirectBuffer.Get());

	barrier = CD3DX12_RESOURCE_BARRIER::Transition(chunk.redirectBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &barrier);
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

				auto& chunk = chunks[ax][ay];
				if (chunk.worldCoord.x != desired.x || chunk.worldCoord.y != desired.y)
				{
					chunk.worldCoord = desired;
					chunk.dirty = true;

					chunk.entity->setPosition(terrainParams.chunkWorldMin(desired, chunkSize));
				}
			}
	}

	for (int x = 0; x < (int)VegGridSize; x++)
		for (int y = 0; y < (int)VegGridSize; y++)
		{
			auto& chunk = chunks[x][y];
			if (chunk.dirty)
			{
				regenerateChunk(commandList, chunk, terrain);
				chunk.dirty = false;
			}
		}
}

void Vegetation::updateCulling(ID3D12GraphicsCommandList* commandList, const Camera& camera, const ProgressiveTerrain& terrain)
{
	if (!updatingEnabled)
		return;

	CommandsMarker marker(commandList, "Trees", PixColor::ForestGreen);

	auto& terrainParams = terrain.params;
	float chunkSize = terrainParams.tileSize / ChunksPerTerrainTile;

	VegetationUpdateComputeShader::Input cullingInput{};
	camera.extractFrustumPlanes(cullingInput.frustumPlanes);
	cullingInput.chunkSize = chunkSize;

	auto frustum = camera.prepareFrustum();

	// Transition shared command buffer to UAV and clear all InstanceCounts
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(sharedCommandBuffer.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->ResourceBarrier(1, &barrier);

	indirectDrawClearCS.dispatch(commandList, TotalChunks, sharedCommandBuffer.Get());

	CD3DX12_RESOURCE_BARRIER uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(sharedCommandBuffer.Get());
	commandList->ResourceBarrier(1, &uavBarrier);

	for (int x = 0; x < (int)VegGridSize; x++)
		for (int y = 0; y < (int)VegGridSize; y++)
		{
			auto& chunk = chunks[x][y];

			if (chunk.entity->isVisible(frustum))
			{
				auto worldMin = terrainParams.chunkWorldMin(chunk.worldCoord, chunkSize);
				cullingInput.chunkWorldMin = { worldMin.x, worldMin.y, worldMin.z };
				updateChunk(commandList, chunk, cullingInput);
			}
		}

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
	}
	CreateStaticBuffer(renderSystem.core.device, batch, defaultMeta.data(), sizeof(SubgroupMeta) * SubgroupCount, D3D12_RESOURCE_STATE_COPY_SOURCE, &defaultMetaBuffer);

	// Shared command buffer for all chunks — pre-fill IndexCountPerInstance=6, rest=0
	std::vector<D3D12_DRAW_INDEXED_ARGUMENTS> defaultCommands(TotalChunks);
	for (auto& cmd : defaultCommands)
	{
		cmd.IndexCountPerInstance = 6;
		cmd.InstanceCount = 0;
		cmd.StartIndexLocation = 0;
		cmd.BaseVertexLocation = 0;
		cmd.StartInstanceLocation = 0;
	}
	CreateStaticBuffer(renderSystem.core.device, batch, defaultCommands.data(), TotalChunks, sizeof(D3D12_DRAW_INDEXED_ARGUMENTS),
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

void VegetationUpdateComputeShader::dispatch(ID3D12GraphicsCommandList* commandList, const Input& input, ID3D12Resource* subgroupMeta, D3D12_GPU_VIRTUAL_ADDRESS commandsAddr, ID3D12Resource* redirectBuffer)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	commandList->SetComputeRoot32BitConstants(0, sizeof(input) / sizeof(float), &input, 0);
	commandList->SetComputeRootShaderResourceView(1, subgroupMeta->GetGPUVirtualAddress());
	commandList->SetComputeRootUnorderedAccessView(2, commandsAddr);
	commandList->SetComputeRootUnorderedAccessView(3, redirectBuffer->GetGPUVirtualAddress());

	commandList->Dispatch(Vegetation::SubgroupsPerDim, Vegetation::SubgroupsPerDim, 1);
}

void Vegetation::createBillboardIndexBuffer(RenderSystem& renderSystem, ResourceUploadBatch& batch)
{
	std::vector<uint16_t> data = { 0, 1, 2, 2, 1, 3 };

	CreateStaticBuffer(renderSystem.core.device, batch, data, D3D12_RESOURCE_STATE_INDEX_BUFFER, &indexBuffer);
	indexBuffer->SetName(L"VegetationBillboardIB");
}
