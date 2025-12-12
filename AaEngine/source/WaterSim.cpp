#include "WaterSim.h"
#include "TextureUtils.h"
#include "SceneManager.h"

WaterSim::WaterSim()
{
}

WaterSim::~WaterSim()
{
}

const UINT TextureSize = 1024;
const UINT TerrainSize = TextureSize / 4;

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

	waterModel.addLayoutElement(DXGI_FORMAT_R32_FLOAT, VertexElementSemantic::TEXCOORD);
	waterModel.addLayoutElement(DXGI_FORMAT_R32G32_FLOAT, VertexElementSemantic::NORMAL);
	waterModel.CreateIndexBuffer(renderSystem.core.device, &batch, TextureSize, TextureSize);
	struct WaterVertex
	{
		float height; // x,y,z
		XMFLOAT2 normal; // x,y,z
	};
	{
		std::vector<WaterVertex> vertices;
		vertices.reserve(TextureSize * TextureSize);

		for (UINT y = 0; y < TextureSize; ++y)
		{
			for (UINT x = 0; x < TextureSize; ++x)
			{
				WaterVertex v{};

				// Flat grid position (z = 0 for now)
				v.height = 0;
// 				DirectX::XMFLOAT3(
// 					static_cast<float>(x) * 0.1f,
// 					0.0f,
// 					static_cast<float>(y) * 0.1f);

//				v.normal = { 0, 1, 0 };

				// Normalized UVs
// 				v.uv = DirectX::XMFLOAT2(
// 					static_cast<float>(x) * 40 / (TextureSize - 1),
// 					static_cast<float>(y) * 40 / (TextureSize - 1));

				vertices.push_back(v);
			}
		}
		waterModel.CreateVertexBuffer(renderSystem.core.device, &batch, vertices.data(), vertices.size(), sizeof(WaterVertex));
		waterModel.calculateBounds();
	}

	struct Vertex
	{
		DirectX::XMFLOAT3 position; // x,y,z
		DirectX::XMFLOAT3 normal; // x,y,z
		DirectX::XMFLOAT2 uv;       // u,v
	};
	terrainModel.addLayoutElement(DXGI_FORMAT_R32G32B32_FLOAT, VertexElementSemantic::POSITION);
	terrainModel.addLayoutElement(DXGI_FORMAT_R32G32B32_FLOAT, VertexElementSemantic::NORMAL);
	terrainModel.addLayoutElement(DXGI_FORMAT_R32G32_FLOAT, VertexElementSemantic::TEXCOORD);
	terrainModel.CreateIndexBuffer(renderSystem.core.device, &batch, TerrainSize, TerrainSize);

	{
		std::vector<Vertex> vertices;
		vertices.reserve(TerrainSize* TerrainSize);

		for (UINT y = 0; y < TerrainSize; ++y)
		{
			for (UINT x = 0; x < TerrainSize; ++x)
			{
				Vertex v;

				// Flat grid position (z = 0 for now)
				v.position = DirectX::XMFLOAT3(
					static_cast<float>(x) * 0.1f * TextureSize / TerrainSize,
					0.0f,
					static_cast<float>(y) * 0.1f * TextureSize / TerrainSize);

				v.normal = { 0, 1, 0 };

				// Normalized UVs
				v.uv = DirectX::XMFLOAT2(
					static_cast<float>(x) / (TerrainSize - 1),
					static_cast<float>(y) / (TerrainSize - 1));

				vertices.push_back(v);
			}
		}
		terrainModel.CreateVertexBuffer(renderSystem.core.device, &batch, vertices.data(), vertices.size(), sizeof(Vertex));
		terrainModel.calculateBounds();
		terrainModel.bbox.Extents = { 20, 20, 20 };
	}

	terrainHeight.InitUAV(renderSystem.core.device, TextureSize, TextureSize, DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	terrainHeight.SetName("WaterSimTerrainHeight");
	resources.descriptors.createUAVView(terrainHeight);
	resources.descriptors.createTextureView(terrainHeight);

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

	waterInfoTexture.InitUAV(renderSystem.core.device, TextureSize, TextureSize, DXGI_FORMAT_R32G32B32A32_FLOAT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	waterInfoTexture.SetName("WaterInfoTexture");

	resources.descriptors.createUAVView(waterInfoTexture);
	resources.descriptors.createTextureView(waterInfoTexture);
	resources.textures.setNamedTexture("WaterInfoTexture", waterInfoTexture.view);

	auto e = sceneMgr.createEntity("WaterSimTerrain", Order::Normal);
	e->geometry.fromModel(terrainModel);
	e->setBoundingBox(terrainModel.bbox);
	e->material = resources.materials.getMaterial("DarkGreen", batch);
	e->setPosition({ -20, -20, 30 });

	auto e2 = sceneMgr.createEntity("WaterSim", Order::Transparent);
	e2->geometry.fromModel(waterModel);
	e2->setBoundingBox(waterModel.bbox);
	e2->material = resources.materials.getMaterial("WaterLake", batch);
	e2->material->SetTexture(waterInfoTexture.view, "WaterInfoTexture");
	e2->setPosition({ -20, -20, 30 });

	auto csShader = resources.shaders.getShader("momentumComputeShader", ShaderTypeCompute, ShaderRef{ "waterSim/frenzySimMomentumCS.hlsl", "CS_Momentum", "cs_6_6" }); //frenzySimMomentumCS
	momentumComputeShader.init(*renderSystem.core.device, *csShader);

	csShader = resources.shaders.getShader("continuityComputeShader", ShaderTypeCompute, ShaderRef{ "waterSim/frenzySimContinuityCS.hlsl", "CS_Continuity", "cs_6_6" }); //frenzySimContinuityCS
	continuityComputeShader.init(*renderSystem.core.device, *csShader);

	csShader = resources.shaders.getShader("textureToMeshCS", ShaderTypeCompute, ShaderRef{ "utils/heightmapToMeshCS.hlsl", "CSMain", "cs_6_6" });
	textureToMeshCS.init(*renderSystem.core.device, *csShader);

	csShader = resources.shaders.getShader("watermapToTextureCS", ShaderTypeCompute, ShaderRef{ "utils/watermapToTextureCS.hlsl", "CSMain", "cs_6_6" });
	waterToTextureCS.init(*renderSystem.core.device, *csShader);

	csShader = resources.shaders.getShader("copyTexturesCS", ShaderTypeCompute, ShaderRef{ "utils/copyTexturesCS.hlsl", "CSMain", "cs_6_6" });
	copyTexturesCS.init(*renderSystem.core.device, *csShader);

	csShader = resources.shaders.getShader("meshTextureCS", ShaderTypeCompute, ShaderRef{ "waterSim/colorWaterSimTexture.hlsl", "CSMain", "cs_6_6" });
	meshTextureCS.init(*renderSystem.core.device, *csShader);
}

bool first = true;
float SafeDt(float dtRaw)
{
	static float dt = 1.0f / 60.0f;

	// max relative change per frame
	const float maxRate = 0.10f; // 10%

	float diff = dtRaw - dt;
	float maxDiff = maxRate * dt;

	diff = std::clamp(diff, -maxDiff, maxDiff);

	dt += diff;

	return dt;
}

float SmoothDtTau(float dtRaw)
{
	static float dt = 1.0f / 60.0f;

	const float tau = 0.1f;  // seconds
	const float alpha = dtRaw / (dtRaw + tau);

	dt = (1 - alpha) * dt + alpha * dtRaw;
	return dt;
}

void WaterSim::update(RenderSystem& renderSystem, ID3D12GraphicsCommandList* commandList, float dt, UINT)
{
	static UINT frameIdx = 0;
	const UINT prevFrameIdx = (frameIdx == 0) ? (FrameCount - 1) : (frameIdx - 1);

	if (first)
	{
		TextureUtils::CopyTextureBuffer(commandList, srcWater.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, waterHeight[prevFrameIdx].texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		TextureUtils::CopyTextureBuffer(commandList, srcWater.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, waterHeight[frameIdx].texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		//TextureUtils::CopyTextureBuffer(commandList, srcVelocity.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, waterVelocity[prevFrameIdx].texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		//TextureUtils::CopyTextureBuffer(commandList, srcVelocity.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, waterVelocity[frameIdx].texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

// 		auto b = CD3DX12_RESOURCE_BARRIER::Transition(terrainTexture2->texture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
// 		commandList->ResourceBarrier(1, &b);
// 
// 		copyTexturesCS.dispatch(commandList, terrainTexture2->srvHandles, TextureSize, TextureSize, waterHeight[prevFrameIdx].view.uavHandles);
// 		copyTexturesCS.dispatch(commandList, terrainTexture2->srvHandles, TextureSize, TextureSize, waterHeight[frameIdx].view.uavHandles);
// 
// 		b = CD3DX12_RESOURCE_BARRIER::Transition(terrainTexture2->texture.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
// 		commandList->ResourceBarrier(1, &b);



		auto b2 = CD3DX12_RESOURCE_BARRIER::Transition(terrainTexture->texture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		commandList->ResourceBarrier(1, &b2);

		copyTexturesCS.dispatch(commandList, terrainTexture->srvHandles, TextureSize, TextureSize, terrainHeight.view.uavHandles);
		copyTexturesCS.dispatch(commandList, terrainTexture->srvHandles, TextureSize, TextureSize, terrainHeight.view.uavHandles);

		b2 = CD3DX12_RESOURCE_BARRIER::Transition(terrainTexture->texture.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		commandList->ResourceBarrier(1, &b2);

		textureToMeshCS.dispatch(commandList, terrainHeight.view.srvHeapIndex, TerrainSize, TerrainSize, 50, terrainModel.vertexBuffer);
	}
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
	waterToTextureCS.dispatch(computeList, waterHeight[frameIdx].view.srvHeapIndex, TextureSize, TextureSize, waterInfoTexture.view.uavHandles);
	waterHeight[frameIdx].Transition(computeList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	frameIdx = (frameIdx + 1) % FrameCount;
}

void WaterSim::clear()
{
}

void WaterSim::prepareForRendering(ID3D12GraphicsCommandList* commandList)
{
	waterInfoTexture.Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void WaterSim::prepareAfterRendering(ID3D12GraphicsCommandList* commandList)
{
	waterInfoTexture.Transition(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}
