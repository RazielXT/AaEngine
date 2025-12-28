#include "WaterSim.h"
#include "TextureUtils.h"
#include "SceneManager.h"
#include "GridMesh.h"

WaterSim::WaterSim()
{
}

WaterSim::~WaterSim()
{
}

const UINT TextureSize = 1024;
const UINT TerrainModelSize = 33;// TextureSize - WaterModelChucks + 1;

GridInstanceMesh terrainGridMesh;
GridInstanceMesh waterGridMesh;
GridLODSystem terrainGridTiles;

void WaterSim::initializeGpuResources(RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch, SceneManager& sceneMgr)
{
	terrainTexture = resources.textures.loadFile(*renderSystem.core.device, batch, "Mountain Range Height Map PNG.png");
	resources.descriptors.createTextureView(*terrainTexture);
	terrainTexture2 = resources.textures.loadFile(*renderSystem.core.device, batch, "Caustics_tex.png");
	resources.descriptors.createTextureView(*terrainTexture2);

	// Generate gradient data
	std::vector<float> gradient(TextureSize* TextureSize);
	for (UINT y = 0; y < TextureSize; ++y)
	{
		for (UINT x = 0; x < TextureSize; ++x)
		{
			float t = static_cast<float>(TextureSize - x) / (TextureSize - 1);
			//t += static_cast<float>(TextureSize - y) / (TextureSize - 1);
			gradient[y * TextureSize + x] = t * 20;
		}
	}
	srcWater = TextureUtils::CreateUploadTexture(renderSystem.core.device, batch, gradient.data(), TextureSize, TextureSize, DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	
	std::vector<XMFLOAT2> velocityData(TextureSize * TextureSize);
	for (UINT y = 0; y < TextureSize; ++y)
	{
		for (UINT x = 0; x < TextureSize; ++x)
		{
			velocityData[y * TextureSize + x] = { 1, 0 };
		}
	}
	srcVelocity = TextureUtils::CreateUploadTexture(renderSystem.core.device, batch, velocityData.data(), TextureSize, TextureSize, DXGI_FORMAT_R32G32_FLOAT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	terrainModel.CreateIndexBufferGrid(renderSystem.core.device, &batch, TerrainModelSize);
	terrainModel.bbox.Extents = { 50, 50, 50 };

	waterModel.CreateIndexBufferGrid(renderSystem.core.device, &batch, TerrainModelSize);
	waterModel.bbox = terrainModel.bbox;

	terrainHeight.InitUAV(renderSystem.core.device, TextureSize, TextureSize, DXGI_FORMAT_R16_UNORM, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	terrainHeight.SetName("WaterSimTerrainHeight");
	resources.descriptors.createUAVView(terrainHeight);
	resources.descriptors.createTextureView(terrainHeight);

	terrainNormal.InitUAV(renderSystem.core.device, TextureSize, TextureSize, DXGI_FORMAT_R8G8_SNORM, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, { .mipLevels = 5 });
	terrainNormal.SetName("WaterSimTerrainNormal");
	resources.descriptors.createUAVView(terrainNormal);
	resources.descriptors.createTextureView(terrainNormal, -1);
	terrainNormalMips = resources.descriptors.createUAVMips(terrainNormal);

	for (size_t i = 0; i < std::size(waterHeight); i++)
	{
		auto& w = waterHeight[i];
		w.InitUAV(renderSystem.core.device, TextureSize, TextureSize, DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		w.SetName("WaterSimWaterHeight" + std::to_string(i));
		resources.descriptors.createUAVView(w);
		resources.descriptors.createTextureView(w);
	}
	for (size_t i = 0; i < std::size(waterVelocity); i++)
	{
		auto& w = waterVelocity[i];
		w.InitUAV(renderSystem.core.device, TextureSize, TextureSize, DXGI_FORMAT_R32G32_FLOAT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		w.SetName("WaterSimWaterVelocity" + std::to_string(i));
		resources.descriptors.createUAVView(w);
		resources.descriptors.createTextureView(w);
	}

	waterNormalTexture.InitUAV(renderSystem.core.device, TextureSize, TextureSize, DXGI_FORMAT_R8G8_SNORM, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, { .mipLevels = 5 });
	waterNormalTexture.SetName("WaterNormalTexture");
	resources.descriptors.createUAVView(waterNormalTexture);
	resources.descriptors.createTextureView(waterNormalTexture);
	waterNormalTextureMips = resources.descriptors.createUAVMips(waterNormalTexture);

	auto e = sceneMgr.createEntity("WaterSimTerrain", Order::Normal);
	terrainGridMesh.create(16 * 16 * 4 * 4);
	terrainGridMesh.entity = e;
	e->geometry.fromInstancedModel(terrainModel, 0, terrainGridMesh.gpuBuffer.data[0].GpuAddress());
	e->setBoundingBox(terrainModel.bbox);
	e->material = resources.materials.getMaterial("DarkGreenGrid", batch);
	e->Material().setParam("TexIdHeightmap", terrainHeight.view.srvHeapIndex);
	e->Material().setParam("TexIdNormalmap", terrainNormal.view.srvHeapIndex);
	e->setPosition({ -20, -20, 30 });

	terrainGridTiles.Initialize({ 102.4f, 102.4f }, e->getPosition());

	auto e2 = sceneMgr.createEntity("WaterSim", Order::Transparent);
	waterGridMesh.create(16 * 16 * 4 * 4);
	waterGridMesh.entity = e2;
	e2->geometry.fromInstancedModel(waterModel, 0, waterGridMesh.gpuBuffer.data[0].GpuAddress());
	e2->setBoundingBox(waterModel.bbox);
	e2->material = resources.materials.getMaterial("WaterLake", batch);
	e2->Material().setParam("TexIdHeightmap", waterHeight[0].view.srvHeapIndex);
	e2->Material().setParam("TexIdNormalmap", waterNormalTexture.view.srvHeapIndex);
	e2->setPosition({ -20, -20, 30 });

	auto csShader = resources.shaders.getShader("momentumComputeShader", ShaderType::Compute, ShaderRef{ "waterSim/frenzySimMomentumCS.hlsl", "CS_Momentum", "cs_6_6" }); //frenzySimMomentumCS
	momentumComputeShader.init(*renderSystem.core.device, *csShader);

	csShader = resources.shaders.getShader("continuityComputeShader", ShaderType::Compute, ShaderRef{ "waterSim/frenzySimContinuityCS.hlsl", "CS_Continuity", "cs_6_6" }); //frenzySimContinuityCS
	continuityComputeShader.init(*renderSystem.core.device, *csShader);

	csShader = resources.shaders.getShader("generateHeightmapNormalsCS", ShaderType::Compute, ShaderRef{ "grid/generateHeightmapNormalsCS.hlsl", "CSMain", "cs_6_6" });
	heightmapToNormalCS.init(*renderSystem.core.device, *csShader);

	csShader = resources.shaders.getShader("watermapToTextureCS", ShaderType::Compute, ShaderRef{ "utils/watermapToTextureCS.hlsl", "CSMain", "cs_6_6" });
	waterToTextureCS.init(*renderSystem.core.device, *csShader);

	csShader = resources.shaders.getShader("copyTexturesCS", ShaderType::Compute, ShaderRef{ "utils/copyTexturesCS.hlsl", "CSMain", "cs_6_6" });
	copyTexturesCS.init(*renderSystem.core.device, *csShader);

	csShader = resources.shaders.getShader("meshTextureCS", ShaderType::Compute, ShaderRef{ "waterSim/colorWaterSimTexture.hlsl", "CSMain", "cs_6_6" });
	meshTextureCS.init(*renderSystem.core.device, *csShader);

	generateNormalMipsCS.init(*renderSystem.core.device, "generateNormalMipmaps4x", resources.shaders);
}

static bool first = true;

void WaterSim::update(RenderSystem& renderSystem, ID3D12GraphicsCommandList* commandList, float dt, UINT frameIdx, Vector3 cameraPos)
{
	const UINT prevFrameIdx = (frameIdx == 0) ? (FrameCount - 1) : (frameIdx - 1);

	if (first)
	{
		TextureUtils::CopyTextureBuffer(commandList, srcWater.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, waterHeight[prevFrameIdx].texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		TextureUtils::CopyTextureBuffer(commandList, srcWater.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, waterHeight[frameIdx].texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		TextureTransitions<2> tr;
		tr.add(terrainTexture->texture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		tr.add(&terrainNormal, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		tr.push(commandList);

		copyTexturesCS.dispatch(commandList, terrainTexture->srvHandles, TextureSize, TextureSize, terrainHeight.view.uavHandles);
		heightmapToNormalCS.dispatch(commandList, terrainHeight.view.srvHeapIndex, terrainNormal.view.uavHeapIndex, TextureSize, TextureSize, 50, 102.4f);

		tr.add(terrainTexture->texture.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		tr.add(&terrainNormal, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		tr.add(&terrainHeight, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		tr.push(commandList);
	}
	else
	{
		TextureTransitions<1> tr;
		tr.add(&terrainNormal, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		tr.push(commandList);

		heightmapToNormalCS.dispatch(commandList, terrainHeight.view.srvHeapIndex, terrainNormal.view.uavHeapIndex, TextureSize, TextureSize, 50, 102.4f);

		auto uavb = CD3DX12_RESOURCE_BARRIER::UAV(terrainNormal.texture.Get());
		commandList->ResourceBarrier(1, &uavb);

		generateNormalMipsCS.dispatch(commandList, TextureSize, terrainNormalMips);

		tr.add(&terrainNormal, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		tr.push(commandList);
	}

	terrainGridTiles.BuildLOD(cameraPos);
	terrainGridMesh.update((UINT)terrainGridTiles.m_renderList.size(), terrainGridTiles.m_renderList.data(), (UINT)terrainGridTiles.m_renderList.size() * sizeof(TileData), frameIdx);

	waterGridMesh.update((UINT)terrainGridTiles.m_renderList.size(), terrainGridTiles.m_renderList.data(), (UINT)terrainGridTiles.m_renderList.size() * sizeof(TileData), frameIdx);
}

void WaterSim::updateCompute(RenderSystem& renderSystem, ID3D12GraphicsCommandList* computeList, float dt, UINT)
{
	CommandsMarker marker(computeList, "WaterSim", PixColor::LightBlue);

	if (first)
	{
		first = false;
		return;
	}

	static UINT frameIdx = 0;
	const float DtPerUpdate = 1 / 60.f;
	static float updateDt = DtPerUpdate;

	updateDt += dt;
	if (updateDt < DtPerUpdate)
		return;

	while (updateDt >= DtPerUpdate)
		updateDt -= DtPerUpdate;

	const UINT prevFrameIdx = (frameIdx == 0) ? (FrameCount - 1) : (frameIdx - 1);

	WaterSimTextures t =
	{
		terrainHeight.view.srvHeapIndex,
		waterHeight[prevFrameIdx].view.uavHeapIndex,
		waterVelocity[prevFrameIdx].view.uavHeapIndex,
		waterHeight[frameIdx].view.uavHeapIndex,
		waterVelocity[frameIdx].view.uavHeapIndex
	};
	WaterSimTextures t2 =
	{
		terrainHeight.view.srvHeapIndex,
		waterVelocity[prevFrameIdx].view.uavHeapIndex,
		waterHeight[prevFrameIdx].view.uavHeapIndex,
		waterVelocity[frameIdx].view.uavHeapIndex,
		waterHeight[frameIdx].view.uavHeapIndex
	};

	auto safeTime = DtPerUpdate * 0.5f;// SmoothDtTau(dt);
	const float gridSpacing = 0.1f;

	{
		momentumComputeShader.dispatch(computeList, TextureSize, safeTime, gridSpacing, t);

		auto uavb = CD3DX12_RESOURCE_BARRIER::UAV(waterVelocity[frameIdx].texture.Get());
		computeList->ResourceBarrier(1, &uavb);

		continuityComputeShader.dispatch(computeList, TextureSize, safeTime, gridSpacing, t);

		uavb = CD3DX12_RESOURCE_BARRIER::UAV(waterHeight[frameIdx].texture.Get());
		computeList->ResourceBarrier(1, &uavb);
	}
	{
		momentumComputeShader.dispatch(computeList, TextureSize, safeTime, gridSpacing, t2);

		auto uavb = CD3DX12_RESOURCE_BARRIER::UAV(waterVelocity[prevFrameIdx].texture.Get());
		computeList->ResourceBarrier(1, &uavb);

		continuityComputeShader.dispatch(computeList, TextureSize, safeTime, gridSpacing, t2);

		uavb = CD3DX12_RESOURCE_BARRIER::UAV(waterHeight[prevFrameIdx].texture.Get());
		computeList->ResourceBarrier(1, &uavb);
	}
	{
		momentumComputeShader.dispatch(computeList, TextureSize, safeTime, gridSpacing, t);

		auto uavb = CD3DX12_RESOURCE_BARRIER::UAV(waterVelocity[frameIdx].texture.Get());
		computeList->ResourceBarrier(1, &uavb);

		continuityComputeShader.dispatch(computeList, TextureSize, safeTime, gridSpacing, t);

		uavb = CD3DX12_RESOURCE_BARRIER::UAV(waterHeight[frameIdx].texture.Get());
		computeList->ResourceBarrier(1, &uavb);
	}

	waterHeight[frameIdx].Transition(computeList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	waterToTextureCS.dispatch(computeList, waterHeight[frameIdx].view.srvHeapIndex, terrainHeight.view.srvHeapIndex, TextureSize, TextureSize, waterNormalTexture.view.uavHandles);
	generateNormalMipsCS.dispatch(computeList, TextureSize, waterNormalTextureMips);
	waterHeight[frameIdx].Transition(computeList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	frameIdx = (frameIdx + 1) % FrameCount;
}

void WaterSim::clear()
{
}

void WaterSim::prepareForRendering(ID3D12GraphicsCommandList* commandList)
{
	waterNormalTexture.Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void WaterSim::prepareAfterRendering(ID3D12GraphicsCommandList* commandList)
{
	waterNormalTexture.Transition(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}
