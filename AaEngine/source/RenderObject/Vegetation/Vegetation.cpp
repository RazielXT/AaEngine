#include "RenderObject/Vegetation/Vegetation.h"
#include "RenderObject/Terrain/ProgressiveTerrain.h"
#include "BufferHelpers.h"
#include "Scene/SceneManager.h"
#include <format>

Vegetation::Vegetation()
{
}

void Vegetation::initialize(RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch)
{
	auto csShader = resources.shaders.getShader("vegetationFindCS", ShaderType::Compute, ShaderRef{ "terrain/vegetation/vegetationFindCS.hlsl", "main", "cs_6_6" });
	vegetationFindCS.init(*renderSystem.core.device, *csShader);

	csShader = resources.shaders.getShader("vegetationUpdateCS", ShaderType::Compute, ShaderRef{ "terrain/vegetation/vegetationUpdateCS.hlsl", "main", "cs_6_6" });
	vegetationUpdateCS.init(*renderSystem.core.device, *csShader);

	createBillboardIndexBuffer(renderSystem, batch);
	initializeImpostors(renderSystem, batch);
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

void Vegetation::initChunk(VegetationChunk& chunk, RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch, MaterialInstance* material)
{
	constexpr UINT row = groups * threads * perThread;
	constexpr UINT maxCount = row * row;

	constexpr UINT infoSize = sizeof(VegetationInfo) * maxCount;
	chunk.infoBuffer = resources.shaderBuffers.CreateStructuredBuffer(infoSize);
	chunk.infoBuffer->SetName(L"VegetationInfo");

	constexpr UINT tranformationSize = sizeof(XMFLOAT4X4) * maxCount;
	chunk.transformationBuffer = resources.shaderBuffers.CreateStructuredBuffer(tranformationSize);
	chunk.transformationBuffer->SetName(L"VegetationTransformation");

	chunk.infoCounter = resources.shaderBuffers.CreateStructuredBuffer(sizeof(UINT));
	chunk.infoCounter->SetName(L"VegetationInfoCounter");

	chunk.impostors.commandSignature = commandSignature;
	chunk.impostors.commandBuffer = resources.shaderBuffers.CreateStructuredBuffer(sizeof(D3D12_DRAW_INDEXED_ARGUMENTS));
	chunk.impostors.commandBuffer->SetName(L"VegetationCommandBuffer");
	chunk.impostors.maxCommands = 1;
}

void Vegetation::createChunks(SceneManager& sceneMgr, RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch)
{
	vegMaterial = resources.materials.getMaterial("VegetationBillboard", batch);
	auto& terrainParams = sceneMgr.terrain.params;
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

			auto e = sceneMgr.createEntity("Veg" + std::format("{}{}", ax, ay));
			chunk.entity = e;
			e->material = vegMaterial;
			e->Material().setParam("ChunkId", x + y * VegGridSize);
			e->geometry.type = EntityGeometry::Type::Indirect;
			e->geometry.source = &chunk.impostors;
			e->geometry.geometryCustomBuffer = chunk.infoBuffer->GetGPUVirtualAddress();

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

	// Transition info/counter back to UAV if they were left as SRV from a previous cycle
	if (!chunk.firstRun)
	{
		CD3DX12_RESOURCE_BARRIER toUav[2]{};
		toUav[0] = CD3DX12_RESOURCE_BARRIER::Transition(chunk.infoBuffer.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		toUav[1] = CD3DX12_RESOURCE_BARRIER::Transition(chunk.infoCounter.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		commandList->ResourceBarrier(2, toUav);
	}
	chunk.firstRun = false;

	// Clear the counter to zero before dispatching find CS
	CD3DX12_RESOURCE_BARRIER toCopyDest = CD3DX12_RESOURCE_BARRIER::Transition(chunk.infoCounter.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);
	commandList->ResourceBarrier(1, &toCopyDest);
	commandList->CopyBufferRegion(chunk.infoCounter.Get(), 0, zeroCounterBuffer.Get(), 0, sizeof(UINT));
	CD3DX12_RESOURCE_BARRIER toUavAgain = CD3DX12_RESOURCE_BARRIER::Transition(chunk.infoCounter.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->ResourceBarrier(1, &toUavAgain);

	vegetationFindCS.dispatch(commandList, input, chunk.infoBuffer.Get(), chunk.infoCounter.Get());

	// UAV barrier to ensure find CS writes complete before update CS reads
	CD3DX12_RESOURCE_BARRIER uavBarriers[2]{};
	uavBarriers[0] = CD3DX12_RESOURCE_BARRIER::UAV(chunk.infoBuffer.Get());
	uavBarriers[1] = CD3DX12_RESOURCE_BARRIER::UAV(chunk.infoCounter.Get());
	commandList->ResourceBarrier(2, uavBarriers);

	CD3DX12_RESOURCE_BARRIER toSrv[2]{};
	toSrv[0] = CD3DX12_RESOURCE_BARRIER::Transition(chunk.infoBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	toSrv[1] = CD3DX12_RESOURCE_BARRIER::Transition(chunk.infoCounter.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(2, toSrv);
}

void Vegetation::updateChunk(ID3D12GraphicsCommandList* commandList, VegetationChunk& chunk)
{
	CD3DX12_RESOURCE_BARRIER barrier[2]{};

	barrier[0] = CD3DX12_RESOURCE_BARRIER::Transition(chunk.impostors.commandBuffer.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
	commandList->ResourceBarrier(1, barrier);

	commandList->CopyBufferRegion(chunk.impostors.commandBuffer.Get(), 0, defaultCommandBuffer.Get(), 0, sizeof(D3D12_DRAW_INDEXED_ARGUMENTS));

	barrier[0] = CD3DX12_RESOURCE_BARRIER::Transition(chunk.impostors.commandBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	barrier[1] = CD3DX12_RESOURCE_BARRIER::Transition(chunk.transformationBuffer.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->ResourceBarrier(2, barrier);

	vegetationUpdateCS.dispatch(commandList, chunk.transformationBuffer.Get(), chunk.impostors.commandBuffer.Get(), chunk.infoBuffer.Get(), chunk.infoCounter.Get());

	barrier[0] = CD3DX12_RESOURCE_BARRIER::Transition(chunk.impostors.commandBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	barrier[1] = CD3DX12_RESOURCE_BARRIER::Transition(chunk.transformationBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(2, barrier);
}

void Vegetation::update(ID3D12GraphicsCommandList* commandList, const Vector3& cameraPos, const ProgressiveTerrain& terrain)
{
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

	// Regenerate dirty chunks (find vegetation positions) and update their indirect draw calls
	for (int x = 0; x < (int)VegGridSize; x++)
		for (int y = 0; y < (int)VegGridSize; y++)
		{
			auto& chunk = chunks[x][y];
			if (chunk.dirty && chunk.entity)
			{
				regenerateChunk(commandList, chunk, terrain);
				updateChunk(commandList, chunk);
				chunk.dirty = false;
			}
		}
}

struct IndirectCommand
{
	D3D12_DRAW_INDEXED_ARGUMENTS drawArguments;
};

void Vegetation::initializeImpostors(RenderSystem& renderSystem, ResourceUploadBatch& batch)
{
	D3D12_INDIRECT_ARGUMENT_DESC argumentDescs[1] = {};
	argumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

	D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
	commandSignatureDesc.pArgumentDescs = argumentDescs;
	commandSignatureDesc.NumArgumentDescs = _countof(argumentDescs);
	commandSignatureDesc.ByteStride = sizeof(IndirectCommand);

	renderSystem.core.device->CreateCommandSignature(&commandSignatureDesc, nullptr, IID_PPV_ARGS(&commandSignature));

	IndirectCommand defaultData{};
	defaultData.drawArguments.IndexCountPerInstance = 6;
	defaultData.drawArguments.InstanceCount = 0;

	CreateStaticBuffer(renderSystem.core.device, batch, &defaultData, sizeof(IndirectCommand), D3D12_RESOURCE_STATE_COPY_SOURCE, &defaultCommandBuffer);

	UINT zero = 0;
	CreateStaticBuffer(renderSystem.core.device, batch, &zero, sizeof(UINT), D3D12_RESOURCE_STATE_COPY_SOURCE, &zeroCounterBuffer);
}

void VegetationFindComputeShader::dispatch(ID3D12GraphicsCommandList* commandList, const Input& input, ID3D12Resource* infoBuffer, ID3D12Resource* counter)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	commandList->SetComputeRoot32BitConstants(0, sizeof(input) / sizeof(float), &input, 0);
	commandList->SetComputeRootUnorderedAccessView(1, infoBuffer->GetGPUVirtualAddress());
	commandList->SetComputeRootUnorderedAccessView(2, counter->GetGPUVirtualAddress());

	commandList->Dispatch(groups, groups, 1);
}

void VegetationUpdateComputeShader::dispatch(ID3D12GraphicsCommandList* commandList, ID3D12Resource* tranformBuffer, ID3D12Resource* commands, ID3D12Resource* infoBuffer, ID3D12Resource* infoCounter)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	commandList->SetComputeRootShaderResourceView(0, infoBuffer->GetGPUVirtualAddress());
	commandList->SetComputeRootShaderResourceView(1, infoCounter->GetGPUVirtualAddress());
	commandList->SetComputeRootUnorderedAccessView(2, tranformBuffer->GetGPUVirtualAddress());
	commandList->SetComputeRootUnorderedAccessView(3, commands->GetGPUVirtualAddress());

	commandList->Dispatch(groups * groups, 1, 1);
}

void Vegetation::createBillboardIndexBuffer(RenderSystem& renderSystem, ResourceUploadBatch& batch)
{
	std::vector<uint16_t> data = { 0, 1, 2, 2, 1, 3 };

	CreateStaticBuffer(renderSystem.core.device, batch, data, D3D12_RESOURCE_STATE_INDEX_BUFFER, &indexBuffer);
	indexBuffer->SetName(L"VegetationBillboardIB");
}
