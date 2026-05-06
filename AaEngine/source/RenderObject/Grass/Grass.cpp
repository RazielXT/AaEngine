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

	D3D12_DRAW_INDEXED_ARGUMENTS defaultData{};
	defaultData.IndexCountPerInstance = grassModel->indexCount;
	defaultData.InstanceCount = 0;

	CreateStaticBuffer(renderSystem.core.device, batch, &defaultData, sizeof(defaultData), D3D12_RESOURCE_STATE_COPY_SOURCE, &defaultCommandBuffer);

	UINT zero = 0;
	CreateStaticBuffer(renderSystem.core.device, batch, &zero, sizeof(UINT), D3D12_RESOURCE_STATE_COPY_SOURCE, &zeroCounterBuffer);
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
			e->geometry.source = &chunk.indirect;
			e->geometry.geometryCustomBuffer = chunk.transformationBuffer->GetGPUVirtualAddress();
			e->geometry.vertexBufferView = grassModel->vertexBufferView;
			e->geometry.indexBufferView = grassModel->indexBufferView;
			e->geometry.indexCount = grassModel->indexCount;
			e->geometry.layout = &grassModel->vertexLayout;

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

	struct RenderGrassInfo
	{
		Vector3 position;
		float scale;
		float cosYaw;
		float sinYaw;
	};
	constexpr UINT transformSize = sizeof(RenderGrassInfo) * maxPerChunk;
	chunk.transformationBuffer = resources.shaderBuffers.CreateStructuredBuffer(transformSize);
	chunk.transformationBuffer->SetName(L"GrassTransformation");

	chunk.infoCounter = resources.shaderBuffers.CreateStructuredBuffer(sizeof(UINT));
	chunk.infoCounter->SetName(L"GrassInfoCounter");

	chunk.indirect.commandSignature = commandSignature;
	chunk.indirect.commandBuffer = resources.shaderBuffers.CreateStructuredBuffer(sizeof(D3D12_DRAW_INDEXED_ARGUMENTS));
	chunk.indirect.commandBuffer->SetName(L"GrassCommandBuffer");
	chunk.indirect.maxCommands = 1;
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

void Grass::updateChunk(ID3D12GraphicsCommandList* commandList, GrassChunk& chunk, const GrassUpdateComputeShader::Input& cullingInput)
{
	CD3DX12_RESOURCE_BARRIER barrier[2]{};

	barrier[0] = CD3DX12_RESOURCE_BARRIER::Transition(chunk.indirect.commandBuffer.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
	commandList->ResourceBarrier(1, barrier);

	commandList->CopyBufferRegion(chunk.indirect.commandBuffer.Get(), 0, defaultCommandBuffer.Get(), 0, sizeof(D3D12_DRAW_INDEXED_ARGUMENTS));

	barrier[0] = CD3DX12_RESOURCE_BARRIER::Transition(chunk.indirect.commandBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	barrier[1] = CD3DX12_RESOURCE_BARRIER::Transition(chunk.transformationBuffer.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->ResourceBarrier(2, barrier);

	grassUpdateCS.dispatch(commandList, cullingInput, chunk.transformationBuffer.Get(), chunk.indirect.commandBuffer.Get(), chunk.infoBuffer.Get(), chunk.infoCounter.Get());

	barrier[0] = CD3DX12_RESOURCE_BARRIER::Transition(chunk.indirect.commandBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	barrier[1] = CD3DX12_RESOURCE_BARRIER::Transition(chunk.transformationBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(2, barrier);
}

void Grass::update(ID3D12GraphicsCommandList* commandList, const Camera& camera, const ProgressiveTerrain& terrain)
{
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

	GrassUpdateComputeShader::Input cullingInput{};
	camera.extractFrustumPlanes(cullingInput.frustumPlanes);
	auto camPos = camera.getPosition();
	cullingInput.cameraPosition = { camPos.x, camPos.y, camPos.z };
	cullingInput.maxDistance = chunkSize * GrassGridSize * 0.5f;

	auto frustum = camera.prepareFrustum();

	static UINT counter = 0;
	const UINT UpdateDivision = 4;

	for (UINT x = 0; x < GrassGridSize; x++)
		for (UINT y = 0; y < GrassGridSize; y++)
		{
			auto& chunk = chunks[x][y];
			if (chunk.entity->isVisible(frustum))
			{
				if (!counter || (x * GrassGridSize + y + counter) % UpdateDivision == 0)
					updateChunk(commandList, chunk, cullingInput);
			}
		}

	counter++;
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

void GrassUpdateComputeShader::dispatch(ID3D12GraphicsCommandList* commandList, const Input& input, ID3D12Resource* transformBuffer, ID3D12Resource* commands, ID3D12Resource* infoBuffer, ID3D12Resource* infoCounter)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	commandList->SetComputeRoot32BitConstants(0, sizeof(input) / sizeof(float), &input, 0);
	commandList->SetComputeRootShaderResourceView(1, infoBuffer->GetGPUVirtualAddress());
	commandList->SetComputeRootShaderResourceView(2, infoCounter->GetGPUVirtualAddress());
	commandList->SetComputeRootUnorderedAccessView(3, transformBuffer->GetGPUVirtualAddress());
	commandList->SetComputeRootUnorderedAccessView(4, commands->GetGPUVirtualAddress());

	commandList->Dispatch(groups * groups, 1, 1);
}
