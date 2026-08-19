#include "RenderObject/Grass/Grass.h"
#include "RenderObject/Terrain/ProgressiveTerrain.h"
#include "BufferHelpers.h"
#include "Scene/RenderWorld.h"
#include <format>

Grass::Grass()
{
}

void Grass::initialize(RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch)
{
	auto csShader = resources.shaders.getShader("grassFindCS", ShaderType::Compute, ShaderRef{ "terrain/grass/grassFindCS.hlsl", "main", "cs_6_6" });
	grassFindCS.init(*renderSystem.core.device, *csShader);

	csShader = resources.shaders.getShader("grassCompactCS", ShaderType::Compute, ShaderRef{ "terrain/grass/grassCompactCS.hlsl", "main", "cs_6_6" });
	grassCompactCS.init(*renderSystem.core.device, *csShader);

	csShader = resources.shaders.getShader("grassUpdateCS", ShaderType::Compute, ShaderRef{ "terrain/grass/grassUpdateCS.hlsl", "main", "cs_6_6", { {"FRUSTUM_CULLING", "1"}, {"DISTANCE_CULLING", "0"} } });
	grassUpdateCS.init(*renderSystem.core.device, *csShader);

	csShader = resources.shaders.getShader("indirectDrawIndexedClearCS", ShaderType::Compute);
	indirectDrawClearCS.init(*renderSystem.core.device, *csShader);

	grassModel = resources.models.getModel("GrassBlade.mesh", batch, { "meshes/grass", true });
	grassMaterial = resources.materials.getMaterial("grassBladeInstanced", batch);

	// Command signature for indirect indexed draw
	D3D12_INDIRECT_ARGUMENT_DESC argumentDescs[1] = {};
	argumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

	D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
	commandSignatureDesc.pArgumentDescs = argumentDescs;
	commandSignatureDesc.NumArgumentDescs = _countof(argumentDescs);
	commandSignatureDesc.ByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);

	renderSystem.core.device->CreateCommandSignature(&commandSignatureDesc, nullptr, IID_PPV_ARGS(&commandSignature));

	UINT zero = 0;
	CreateStaticBuffer(renderSystem.core.device, batch, &zero, sizeof(UINT), D3D12_RESOURCE_STATE_COPY_SOURCE, &zeroCounterBuffer);

	createSharedBuffers(renderSystem, resources, batch);
	createSharedCommandBuffer(renderSystem, batch);
}

void Grass::createSharedBuffers(RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch)
{
	struct GrassInfo
	{
		Vector3 position;
		float rotation;
		float scale;
		UINT tileId;
	};
	constexpr UINT infoSize = sizeof(GrassInfo) * MaxTotalInstances;

	// Ping-pong candidate buffers rest as SRV; the counters start zeroed so the first compaction sees
	// an empty source before every tile appends its freshly generated blades.
	UINT zero = 0;
	for (int i = 0; i < 2; i++)
	{
		infoBuffer[i] = resources.shaderBuffers.CreateStructuredBuffer(infoSize);
		infoBuffer[i]->SetName(L"GrassInfo");

		CreateStaticBuffer(renderSystem.core.device, batch, &zero, sizeof(UINT), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			&infoCounter[i], D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		infoCounter[i]->SetName(L"GrassInfoCounter");
	}

	struct RenderGrassInfo
	{
		Vector3 position;
		float scale;
		float cosYaw;
		float sinYaw;
	};
	constexpr UINT transformSize = sizeof(RenderGrassInfo) * MaxTotalInstances;

	for (auto& t : transformationBuffer)
	{
		t = resources.shaderBuffers.CreateStructuredBuffer(transformSize);
		t->SetName(L"GrassTransformation");
	}
}

void Grass::createSharedCommandBuffer(RenderSystem& renderSystem, ResourceUploadBatch& batch)
{
	// One command per view; all tiles are batched into a single indirect draw per view. The clear
	// pass only rewrites InstanceCount each frame, so IndexCountPerInstance is seeded here.
	std::vector<D3D12_DRAW_INDEXED_ARGUMENTS> defaultCommands(ViewCount);
	for (auto& cmd : defaultCommands)
	{
		cmd.IndexCountPerInstance = grassModel->indexCount;
		cmd.InstanceCount = 0;
		cmd.StartIndexLocation = 0;
		cmd.BaseVertexLocation = 0;
		cmd.StartInstanceLocation = 0;
	}

	CreateStaticBuffer(renderSystem.core.device, batch, defaultCommands.data(), ViewCount, sizeof(D3D12_DRAW_INDEXED_ARGUMENTS),
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, &sharedCommandBuffer, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	sharedCommandBuffer->SetName(L"GrassSharedCommands");
}

void Grass::createChunks(RenderWorld& renderWorld, RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch)
{
	auto& terrainParams = renderWorld.terrain.params;
	float chunkSize = terrainParams.tileSize / ChunksPerTerrainTile;
	int half = (int)GrassGridSize / 2;

	gridCenterChunk = terrainParams.gridCenterAt(Vector3::Zero, chunkSize, GrassGridSize);

	// Assign each tile its current world position and mark it for generation. Tiles keep a fixed id
	// (their grid slot) that tags their blades in the shared buffer so compaction can drop them.
	for (int x = 0; x < (int)GrassGridSize; x++)
		for (int y = 0; y < (int)GrassGridSize; y++)
		{
			XMINT2 desired = { gridCenterChunk.x + x - half, gridCenterChunk.y + y - half };
			int ax = TerrainGridParams::wrapIndex(desired.x, GrassGridSize);
			int ay = TerrainGridParams::wrapIndex(desired.y, GrassGridSize);

			auto& tile = tiles[ax][ay];
			tile.worldCoord = desired;
			tile.dirty = true;
		}

	// Single entity batches every tile into one indirect draw per view.
	auto e = renderWorld.createEntity();
	entity = e;
	e->material = grassMaterial;
	e->geometry.type = EntityGeometry::Type::Indirect;
	e->geometry.vertexBufferView = grassModel->vertexBufferView;
	e->geometry.indexBufferView = grassModel->indexBufferView;
	e->geometry.indexCount = grassModel->indexCount;
	e->geometry.layout = &grassModel->vertexLayout;

	e->Material().setParam("TileSize", chunkSize);

	for (UINT i = 0; i < ViewCount; i++)
	{
		indirect[i].commandSignature = commandSignature;
		indirect[i].maxCommands = 1;
		indirect[i].commandBuffer = sharedCommandBuffer;
		indirect[i].commandBufferOffset = i * sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);

		if (i == 0)
		{
			e->geometry.geometryCustomBuffer = transformationBuffer[i]->GetGPUVirtualAddress();
			e->geometry.source = &indirect[i];
		}
		else
		{
			auto& g = views.viewGeometries[i - 1];
			g = e->geometry;
			g.geometryCustomBuffer = transformationBuffer[i]->GetGPUVirtualAddress();
			g.source = &indirect[i];
		}
	}

	e->geometry.viewVariants = &views.variants;

	// Bounding box spans the whole grass grid so CPU frustum culling keeps the entity drawing for the
	// main view and shadow cascades; per-instance visibility is handled on the GPU.
	float gridHalf = chunkSize * GrassGridSize * 0.5f;
	e->setBoundingBox({ { gridHalf, terrainParams.tileHeight * 0.5f, gridHalf },
					   { gridHalf, terrainParams.tileHeight * 0.5f, gridHalf } });

	e->setFlag(RenderObjectFlag::Value(RenderObjectFlag::OnlyFirstCascade | RenderObjectFlag::NoVoxelization));

	XMINT2 minCoord = { gridCenterChunk.x - half, gridCenterChunk.y - half };
	e->setPosition(terrainParams.chunkWorldMin(minCoord, chunkSize));
}

void Grass::clear()
{
	for (int x = 0; x < GrassGridSize; x++)
		for (int y = 0; y < GrassGridSize; y++)
		{
			tiles[x][y] = {};
		}

	currentInfo = 0;
	entity = nullptr;
}

void Grass::enableUpdating(bool enabled)
{
	updatingEnabled = enabled;
}

GrassFindComputeShader::Input Grass::buildFindInput(const GrassTile& tile, UINT tileId, const ProgressiveTerrain& terrain) const
{
	auto& terrainParams = terrain.params;
	float chunkSize = terrainParams.tileSize / ChunksPerTerrainTile;

	XMINT2 terrainChunk = {
		(int)std::floor((float)tile.worldCoord.x / ChunksPerTerrainTile),
		(int)std::floor((float)tile.worldCoord.y / ChunksPerTerrainTile)
	};

	UINT heightmapSrv = terrain.getHeightmapSrvIndex(terrainChunk);

	int subX = ((tile.worldCoord.x % (int)ChunksPerTerrainTile) + (int)ChunksPerTerrainTile) % (int)ChunksPerTerrainTile;
	int subY = ((tile.worldCoord.y % (int)ChunksPerTerrainTile) + (int)ChunksPerTerrainTile) % (int)ChunksPerTerrainTile;

	float uvScale = 1.0f / ChunksPerTerrainTile;

	GrassFindComputeShader::Input input;
	input.terrainTexture = heightmapSrv;
	input.terrainHeight = terrainParams.tileHeight;

	auto worldMin = terrainParams.chunkWorldMin(tile.worldCoord, chunkSize);
	input.terrainOffset = { worldMin.x + chunkSize * 0.5f, worldMin.z + chunkSize * 0.5f };
	input.chunkWorldSize = { chunkSize, chunkSize };
	input.subUvOffset = { subX * uvScale, subY * uvScale };
	input.subUvScale = { uvScale, uvScale };
	input.tileId = tileId;

	return input;
}

void Grass::regenerateTiles(ID3D12GraphicsCommandList* commandList, UINT dirtyMask, const ProgressiveTerrain& terrain)
{
	UINT src = currentInfo;
	UINT dst = currentInfo ^ 1;

	// --- Compaction: stream src -> dst, dropping the dirty tiles that are about to be regenerated. ---
	// dst rests as SRV; move its info buffer to UAV and zero its counter before writing.
	CD3DX12_RESOURCE_BARRIER toWrite[2]{};
	toWrite[0] = CD3DX12_RESOURCE_BARRIER::Transition(infoBuffer[dst].Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	toWrite[1] = CD3DX12_RESOURCE_BARRIER::Transition(infoCounter[dst].Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
	commandList->ResourceBarrier(2, toWrite);

	commandList->CopyBufferRegion(infoCounter[dst].Get(), 0, zeroCounterBuffer.Get(), 0, sizeof(UINT));

	CD3DX12_RESOURCE_BARRIER counterToUav = CD3DX12_RESOURCE_BARRIER::Transition(infoCounter[dst].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->ResourceBarrier(1, &counterToUav);

	grassCompactCS.dispatch(commandList, { dirtyMask }, infoBuffer[src].Get(), infoCounter[src].Get(), infoBuffer[dst].Get(), infoCounter[dst].Get(), DispatchGroupsX);

	CD3DX12_RESOURCE_BARRIER compactUav[2]{};
	compactUav[0] = CD3DX12_RESOURCE_BARRIER::UAV(infoBuffer[dst].Get());
	compactUav[1] = CD3DX12_RESOURCE_BARRIER::UAV(infoCounter[dst].Get());
	commandList->ResourceBarrier(2, compactUav);

	// dst is now the live buffer holding the surviving instances; append the regenerated tiles to it.
	currentInfo = dst;

	for (UINT x = 0; x < GrassGridSize; x++)
		for (UINT y = 0; y < GrassGridSize; y++)
		{
			UINT tileId = x * GrassGridSize + y;
			if (!(dirtyMask & (1u << tileId)))
				continue;

			auto input = buildFindInput(tiles[x][y], tileId, terrain);
			grassFindCS.dispatch(commandList, input, infoBuffer[dst].Get(), infoCounter[dst].Get());
		}

	CD3DX12_RESOURCE_BARRIER appendUav[2]{};
	appendUav[0] = CD3DX12_RESOURCE_BARRIER::UAV(infoBuffer[dst].Get());
	appendUav[1] = CD3DX12_RESOURCE_BARRIER::UAV(infoCounter[dst].Get());
	commandList->ResourceBarrier(2, appendUav);

	CD3DX12_RESOURCE_BARRIER toSrv[2]{};
	toSrv[0] = CD3DX12_RESOURCE_BARRIER::Transition(infoBuffer[dst].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	toSrv[1] = CD3DX12_RESOURCE_BARRIER::Transition(infoCounter[dst].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(2, toSrv);
}

void Grass::update(ID3D12GraphicsCommandList* commandList, const Camera& camera, const ProgressiveTerrain& terrain)
{
	if (!updatingEnabled)
		return;

	CommandsMarker marker(commandList, "Grass", PixColor::ForestGreen);

	auto& terrainParams = terrain.params;
	float chunkSize = terrainParams.tileSize / ChunksPerTerrainTile;
	int half = (int)GrassGridSize / 2;

	XMINT2 cameraChunk = terrainParams.gridCenterAt(camera.getPosition(), chunkSize, GrassGridSize);

	if (cameraChunk.x != gridCenterChunk.x || cameraChunk.y != gridCenterChunk.y)
	{
		gridCenterChunk = cameraChunk;

		for (int x = 0; x < (int)GrassGridSize; x++)
			for (int y = 0; y < (int)GrassGridSize; y++)
			{
				XMINT2 desired = { gridCenterChunk.x + x - half, gridCenterChunk.y + y - half };
				int ax = TerrainGridParams::wrapIndex(desired.x, GrassGridSize);
				int ay = TerrainGridParams::wrapIndex(desired.y, GrassGridSize);

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

	// Collect all tiles that moved this frame into a single dirty mask; one compaction pass drops
	// their stale blades and the find pass re-appends fresh ones, all against the shared buffer.
	UINT dirtyMask = 0;
	for (UINT x = 0; x < GrassGridSize; x++)
		for (UINT y = 0; y < GrassGridSize; y++)
		{
			if (tiles[x][y].dirty)
			{
				dirtyMask |= (1u << (x * GrassGridSize + y));
				tiles[x][y].dirty = false;
			}
		}

	if (dirtyMask)
		regenerateTiles(commandList, dirtyMask, terrain);
}

void Grass::updateCulling(ID3D12GraphicsCommandList* commandList, const Camera& cullCamera, const Camera& distanceCamera, const ProgressiveTerrain& terrain, UINT viewIndex)
{
	if (!updatingEnabled)
		return;

	CommandsMarker marker(commandList, "Grass", PixColor::ForestGreen);

	auto& terrainParams = terrain.params;
	float chunkSize = terrainParams.tileSize / ChunksPerTerrainTile;

	GrassUpdateComputeShader::Input cullingInput{};
	cullCamera.extractFrustumPlanes(cullingInput.frustumPlanes);
	auto camPos = distanceCamera.getPosition();
	cullingInput.cameraPosition = { camPos.x, camPos.y, camPos.z };
	cullingInput.maxDistance = chunkSize * GrassGridSize * 0.5f;

	UINT cur = currentInfo;
	D3D12_GPU_VIRTUAL_ADDRESS commandsAddr = sharedCommandBuffer->GetGPUVirtualAddress() + (UINT64)viewIndex * sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);

	// Clear this view's single draw command (only InstanceCount is reset; it doubles as the append
	// counter the culling shader increments).
	CD3DX12_RESOURCE_BARRIER cmdToUav = CD3DX12_RESOURCE_BARRIER::Transition(sharedCommandBuffer.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->ResourceBarrier(1, &cmdToUav);

	indirectDrawClearCS.dispatch(commandList, 1, commandsAddr);

	CD3DX12_RESOURCE_BARRIER cmdUav = CD3DX12_RESOURCE_BARRIER::UAV(sharedCommandBuffer.Get());
	commandList->ResourceBarrier(1, &cmdUav);

	// One dispatch culls the whole candidate buffer into this view's transformation buffer.
	CD3DX12_RESOURCE_BARRIER transformToUav = CD3DX12_RESOURCE_BARRIER::Transition(transformationBuffer[viewIndex].Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->ResourceBarrier(1, &transformToUav);

	grassUpdateCS.dispatch(commandList, cullingInput, transformationBuffer[viewIndex].Get(), commandsAddr, infoBuffer[cur].Get(), infoCounter[cur].Get(), DispatchGroupsX);

	CD3DX12_RESOURCE_BARRIER transformToSrv = CD3DX12_RESOURCE_BARRIER::Transition(transformationBuffer[viewIndex].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &transformToSrv);

	CD3DX12_RESOURCE_BARRIER cmdToSrv = CD3DX12_RESOURCE_BARRIER::Transition(sharedCommandBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &cmdToSrv);
}

const UINT groups = 4;

void GrassFindComputeShader::dispatch(ID3D12GraphicsCommandList* commandList, const Input& input, ID3D12Resource* infoBuffer, ID3D12Resource* counter)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	commandList->SetComputeRoot32BitConstants(0, sizeof(input) / sizeof(UINT), &input, 0);
	commandList->SetComputeRootUnorderedAccessView(1, infoBuffer->GetGPUVirtualAddress());
	commandList->SetComputeRootUnorderedAccessView(2, counter->GetGPUVirtualAddress());

	commandList->Dispatch(groups, groups, 1);
}

void GrassCompactComputeShader::dispatch(ID3D12GraphicsCommandList* commandList, const Input& input, ID3D12Resource* srcInfoBuffer, ID3D12Resource* srcCounter, ID3D12Resource* dstInfoBuffer, ID3D12Resource* dstCounter, UINT dispatchGroupsX)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	commandList->SetComputeRoot32BitConstants(0, sizeof(input) / sizeof(UINT), &input, 0);
	commandList->SetComputeRootShaderResourceView(1, srcInfoBuffer->GetGPUVirtualAddress());
	commandList->SetComputeRootShaderResourceView(2, srcCounter->GetGPUVirtualAddress());
	commandList->SetComputeRootUnorderedAccessView(3, dstInfoBuffer->GetGPUVirtualAddress());
	commandList->SetComputeRootUnorderedAccessView(4, dstCounter->GetGPUVirtualAddress());

	commandList->Dispatch(dispatchGroupsX, 64, 1);
}

void GrassUpdateComputeShader::dispatch(ID3D12GraphicsCommandList* commandList, const Input& input, ID3D12Resource* transformBuffer, D3D12_GPU_VIRTUAL_ADDRESS commandsAddr, ID3D12Resource* infoBuffer, ID3D12Resource* infoCounter, UINT dispatchGroupsX)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	commandList->SetComputeRoot32BitConstants(0, sizeof(input) / sizeof(float), &input, 0);
	commandList->SetComputeRootShaderResourceView(1, infoBuffer->GetGPUVirtualAddress());
	commandList->SetComputeRootShaderResourceView(2, infoCounter->GetGPUVirtualAddress());
	commandList->SetComputeRootUnorderedAccessView(3, transformBuffer->GetGPUVirtualAddress());
	commandList->SetComputeRootUnorderedAccessView(4, commandsAddr);

	commandList->Dispatch(dispatchGroupsX, 64, 1);
}
