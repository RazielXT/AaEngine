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

	createSharedCommandBuffer(renderSystem, batch);
}

void Grass::createSharedCommandBuffer(RenderSystem& renderSystem, ResourceUploadBatch& batch)
{
	// One command per (view, chunk). Views own contiguous TotalChunks-sized blocks; within a block,
	// chunks are ordered by update-division group so each frame's cleared commands are contiguous.
	constexpr UINT totalCommands = RenderViewId_Count * TotalChunks;

	std::vector<D3D12_DRAW_INDEXED_ARGUMENTS> defaultCommands(totalCommands);
	for (auto& cmd : defaultCommands)
	{
		cmd.IndexCountPerInstance = grassModel->indexCount;
		cmd.InstanceCount = 0;
		cmd.StartIndexLocation = 0;
		cmd.BaseVertexLocation = 0;
		cmd.StartInstanceLocation = 0;
	}

	CreateStaticBuffer(renderSystem.core.device, batch, defaultCommands.data(), totalCommands, sizeof(D3D12_DRAW_INDEXED_ARGUMENTS),
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, &sharedCommandBuffer, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	sharedCommandBuffer->SetName(L"GrassSharedCommands");
}

void Grass::createChunks(RenderWorld& renderWorld, RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch)
{
	auto& terrainParams = renderWorld.terrain.params;
	float chunkSize = terrainParams.tileSize / ChunksPerTerrainTile;
	int half = (int)GrassGridSize / 2;

	gridCenterChunk = terrainParams.gridCenterAt(Vector3::Zero, chunkSize, GrassGridSize);

	for (int x = 0; x < (int)GrassGridSize; x++)
		for (int y = 0; y < (int)GrassGridSize; y++)
		{
			XMINT2 desired = { gridCenterChunk.x + x - half, gridCenterChunk.y + y - half };
			int ax = TerrainGridParams::wrapIndex(desired.x, GrassGridSize);
			int ay = TerrainGridParams::wrapIndex(desired.y, GrassGridSize);

			auto& chunk = chunks[ax][ay];
			initChunk(chunk, renderSystem, resources, batch);

			auto e = renderWorld.createEntity();
			chunk.entity = e;
			e->material = grassMaterial;
			e->Material().setParam("ChunkId", x * GrassGridSize + y);
			e->geometry.type = EntityGeometry::Type::Indirect;
			e->geometry.vertexBufferView = grassModel->vertexBufferView;
			e->geometry.indexBufferView = grassModel->indexBufferView;
			e->geometry.indexCount = grassModel->indexCount;
			e->geometry.layout = &grassModel->vertexLayout;

			for (size_t i = 0; i < std::size(chunk.viewData); i++)
			{
				auto& viewData = chunk.viewData[i];

				viewData.indirect.commandSignature = commandSignature;
				viewData.indirect.maxCommands = 1;
				viewData.indirect.commandBuffer = sharedCommandBuffer;
				viewData.indirect.commandBufferOffset = commandSlot(i, ax * GrassGridSize + ay) * sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);

				if (i == 0)
				{
					e->geometry.geometryCustomBuffer = viewData.transformationBuffer->GetGPUVirtualAddress();
					e->geometry.source = &viewData.indirect;
				}
				else
				{
					auto& g = chunk.views.viewGeometries[i - 1];
					g = e->geometry;
					g.geometryCustomBuffer = viewData.transformationBuffer->GetGPUVirtualAddress();
					g.source = &viewData.indirect;
				}
			}

			e->geometry.viewVariants = &chunk.views.variants;

			e->setBoundingBox({ { chunkSize * 0.5f, terrainParams.tileHeight * 0.5f, chunkSize * 0.5f },
							   { chunkSize * 0.5f, terrainParams.tileHeight * 0.5f, chunkSize * 0.5f } });

			e->setFlag(RenderObjectFlag::OnlyFirstCascade);

			chunk.worldCoord = desired;
			chunk.dirty = true;

			e->setPosition(terrainParams.chunkWorldMin(desired, chunkSize));
		}
}

void Grass::clear()
{
	for (int x = 0; x < GrassGridSize; x++)
		for (int y = 0; y < GrassGridSize; y++)
		{
			chunks[x][y] = {};
		}
}

void Grass::enableUpdating(bool enabled)
{
	updatingEnabled = enabled;
}

void Grass::initChunk(GrassChunk& chunk, RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch)
{
	struct GrassInfo
	{
		Vector3 position;
		float rotation;
		float scale;
	};
	constexpr UINT infoSize = sizeof(GrassInfo) * maxPerChunk;
	chunk.infoBuffer = resources.shaderBuffers.CreateStructuredBuffer(infoSize);
	chunk.infoBuffer->SetName(L"GrassInfo");

	chunk.infoCounter = resources.shaderBuffers.CreateStructuredBuffer(sizeof(UINT));
	chunk.infoCounter->SetName(L"GrassInfoCounter");

	struct RenderGrassInfo
	{
		Vector3 position;
		float scale;
		float cosYaw;
		float sinYaw;
	};
	constexpr UINT transformSize = sizeof(RenderGrassInfo) * maxPerChunk;

	for (auto& d : chunk.viewData)
	{
		d.transformationBuffer = resources.shaderBuffers.CreateStructuredBuffer(transformSize);
		d.transformationBuffer->SetName(L"GrassTransformation");
	}
}

void Grass::regenerateChunk(ID3D12GraphicsCommandList* commandList, GrassChunk& chunk, const ProgressiveTerrain& terrain)
{
	auto& terrainParams = terrain.params;
	float chunkSize = terrainParams.tileSize / ChunksPerTerrainTile;

	XMINT2 terrainChunk = {
		(int)std::floor((float)chunk.worldCoord.x / ChunksPerTerrainTile),
		(int)std::floor((float)chunk.worldCoord.y / ChunksPerTerrainTile)
	};

	UINT heightmapSrv = terrain.getHeightmapSrvIndex(terrainChunk);

	int subX = ((chunk.worldCoord.x % (int)ChunksPerTerrainTile) + (int)ChunksPerTerrainTile) % (int)ChunksPerTerrainTile;
	int subY = ((chunk.worldCoord.y % (int)ChunksPerTerrainTile) + (int)ChunksPerTerrainTile) % (int)ChunksPerTerrainTile;

	float uvScale = 1.0f / ChunksPerTerrainTile;

	GrassFindComputeShader::Input input;
	input.terrainTexture = heightmapSrv;
	input.terrainHeight = terrainParams.tileHeight;

	auto worldMin = terrainParams.chunkWorldMin(chunk.worldCoord, chunkSize);
	input.terrainOffset = { worldMin.x + chunkSize * 0.5f, worldMin.z + chunkSize * 0.5f };
	input.chunkWorldSize = { chunkSize, chunkSize };
	input.subUvOffset = { subX * uvScale, subY * uvScale };
	input.subUvScale = { uvScale, uvScale };

	if (!chunk.firstRun)
	{
		CD3DX12_RESOURCE_BARRIER toUav[2]{};
		toUav[0] = CD3DX12_RESOURCE_BARRIER::Transition(chunk.infoBuffer.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		toUav[1] = CD3DX12_RESOURCE_BARRIER::Transition(chunk.infoCounter.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		commandList->ResourceBarrier(2, toUav);
	}
	chunk.firstRun = false;

	CD3DX12_RESOURCE_BARRIER toCopyDest = CD3DX12_RESOURCE_BARRIER::Transition(chunk.infoCounter.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);
	commandList->ResourceBarrier(1, &toCopyDest);
	commandList->CopyBufferRegion(chunk.infoCounter.Get(), 0, zeroCounterBuffer.Get(), 0, sizeof(UINT));
	CD3DX12_RESOURCE_BARRIER toUavAgain = CD3DX12_RESOURCE_BARRIER::Transition(chunk.infoCounter.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->ResourceBarrier(1, &toUavAgain);

	grassFindCS.dispatch(commandList, input, chunk.infoBuffer.Get(), chunk.infoCounter.Get());

	CD3DX12_RESOURCE_BARRIER uavBarriers[2]{};
	uavBarriers[0] = CD3DX12_RESOURCE_BARRIER::UAV(chunk.infoBuffer.Get());
	uavBarriers[1] = CD3DX12_RESOURCE_BARRIER::UAV(chunk.infoCounter.Get());
	commandList->ResourceBarrier(2, uavBarriers);

	CD3DX12_RESOURCE_BARRIER toSrv[2]{};
	toSrv[0] = CD3DX12_RESOURCE_BARRIER::Transition(chunk.infoBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	toSrv[1] = CD3DX12_RESOURCE_BARRIER::Transition(chunk.infoCounter.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(2, toSrv);
}

void Grass::updateChunk(ID3D12GraphicsCommandList* commandList, GrassChunk& chunk, UINT viewIndex, const GrassUpdateComputeShader::Input& cullingInput)
{
	auto& view = chunk.viewData[viewIndex];

	// The shared command buffer is already in UAV state and was cleared in updateCulling; only the
	// per-view transformation buffer needs transitioning here.
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(view.transformationBuffer.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->ResourceBarrier(1, &barrier);

	D3D12_GPU_VIRTUAL_ADDRESS commandsAddr = sharedCommandBuffer->GetGPUVirtualAddress() + view.indirect.commandBufferOffset;
	grassUpdateCS.dispatch(commandList, cullingInput, view.transformationBuffer.Get(), commandsAddr, chunk.infoBuffer.Get(), chunk.infoCounter.Get());

	barrier = CD3DX12_RESOURCE_BARRIER::Transition(view.transformationBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &barrier);
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

				auto& chunk = chunks[ax][ay];
				if (chunk.worldCoord.x != desired.x || chunk.worldCoord.y != desired.y)
				{
					chunk.worldCoord = desired;
					chunk.dirty = true;

					chunk.entity->setPosition(terrainParams.chunkWorldMin(desired, chunkSize));
				}
			}
	}

	for (UINT x = 0; x < GrassGridSize; x++)
		for (UINT y = 0; y < GrassGridSize; y++)
		{
			auto& chunk = chunks[x][y];
			if (chunk.dirty)
			{
				regenerateChunk(commandList, chunk, terrain);
				chunk.dirty = false;
			}
		}
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

	// Shadow cascade cameras are orthographic; BoundingFrustum is only valid for perspective,
	// so use the oriented box for the CPU coarse visibility test (matching the scene visibility path).
	const bool ortho = cullCamera.isOrthographic();
	auto frustum = cullCamera.prepareFrustum();
	auto orientedBox = cullCamera.prepareOrientedBox();

	// Stagger main-view chunk updates across frames; shadow views refresh fully when their cascade updates.
	static UINT counter = 0;
	if (viewIndex == 0)
		counter++;

	const UINT activeGroup = (UpdateDivision - (counter % UpdateDivision)) % UpdateDivision;

	// Clear this view's slice of the shared command buffer in one dispatch. Because chunks are laid out
	// grouped by division, the commands touched this frame form a single contiguous range.
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(sharedCommandBuffer.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->ResourceBarrier(1, &barrier);

	D3D12_GPU_VIRTUAL_ADDRESS bufferBase = sharedCommandBuffer->GetGPUVirtualAddress();
	{
		UINT clearSlot = viewIndex * TotalChunks + divisionGroupBase(activeGroup);
		D3D12_GPU_VIRTUAL_ADDRESS clearAddr = bufferBase + (UINT64)clearSlot * sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
		indirectDrawClearCS.dispatch(commandList, divisionGroupCount(activeGroup), clearAddr);
	}

	CD3DX12_RESOURCE_BARRIER uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(sharedCommandBuffer.Get());
	commandList->ResourceBarrier(1, &uavBarrier);

	for (UINT x = 0; x < GrassGridSize; x++)
		for (UINT y = 0; y < GrassGridSize; y++)
		{
			if ((x * GrassGridSize + y) % UpdateDivision != activeGroup)
				continue;

			auto& chunk = chunks[x][y];
			bool visible = ortho ? chunk.entity->isVisible(orientedBox) : chunk.entity->isVisible(frustum);
			if (visible)
				updateChunk(commandList, chunk, viewIndex, cullingInput);
		}

	barrier = CD3DX12_RESOURCE_BARRIER::Transition(sharedCommandBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &barrier);
}

const UINT groups = 4;

void GrassFindComputeShader::dispatch(ID3D12GraphicsCommandList* commandList, const Input& input, ID3D12Resource* infoBuffer, ID3D12Resource* counter)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	commandList->SetComputeRoot32BitConstants(0, sizeof(input) / sizeof(float), &input, 0);
	commandList->SetComputeRootUnorderedAccessView(1, infoBuffer->GetGPUVirtualAddress());
	commandList->SetComputeRootUnorderedAccessView(2, counter->GetGPUVirtualAddress());

	commandList->Dispatch(groups, groups, 1);
}

void GrassUpdateComputeShader::dispatch(ID3D12GraphicsCommandList* commandList, const Input& input, ID3D12Resource* transformBuffer, D3D12_GPU_VIRTUAL_ADDRESS commandsAddr, ID3D12Resource* infoBuffer, ID3D12Resource* infoCounter)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	commandList->SetComputeRoot32BitConstants(0, sizeof(input) / sizeof(float), &input, 0);
	commandList->SetComputeRootShaderResourceView(1, infoBuffer->GetGPUVirtualAddress());
	commandList->SetComputeRootShaderResourceView(2, infoCounter->GetGPUVirtualAddress());
	commandList->SetComputeRootUnorderedAccessView(3, transformBuffer->GetGPUVirtualAddress());
	commandList->SetComputeRootUnorderedAccessView(4, commandsAddr);

	commandList->Dispatch(groups * groups, 64, 1);
}
