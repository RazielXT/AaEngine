#include "WaterSim.h"
#include "TextureUtils.h"
#include "SceneManager.h"

WaterSim::WaterSim()
{
}

WaterSim::~WaterSim()
{
}

const UINT TextureSize = 512;

void WaterSim::initializeGpuResources(RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch, SceneManager& sceneMgr)
{
	terrainTexture = resources.textures.loadFile(*renderSystem.core.device, batch, "simpleHeightmap.png");
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

	waterModel.addLayoutElement(DXGI_FORMAT_R32G32B32_FLOAT, VertexElementSemantic::POSITION);
	waterModel.addLayoutElement(DXGI_FORMAT_R32G32B32_FLOAT, VertexElementSemantic::NORMAL);
	waterModel.addLayoutElement(DXGI_FORMAT_R32G32_FLOAT, VertexElementSemantic::TEXCOORD);
	waterModel.CreateIndexBuffer(renderSystem.core.device, &batch, TextureSize, TextureSize);
	{
		struct Vertex
		{
			DirectX::XMFLOAT3 position; // x,y,z
			DirectX::XMFLOAT3 normal; // x,y,z
			DirectX::XMFLOAT2 uv;       // u,v
		};
		std::vector<Vertex> vertices;
		vertices.reserve(TextureSize* TextureSize);

		for (UINT y = 0; y < TextureSize; ++y)
		{
			for (UINT x = 0; x < TextureSize; ++x)
			{
				Vertex v;

				// Flat grid position (z = 0 for now)
				v.position = DirectX::XMFLOAT3(
					static_cast<float>(x) * 0.1f,
					0.0f,
					static_cast<float>(y) * 0.1f);

				v.normal = { 0, 1, 0 };

				// Normalized UVs
				v.uv = DirectX::XMFLOAT2(
					static_cast<float>(x) / (TextureSize - 1),
					static_cast<float>(y) / (TextureSize - 1));

				vertices.push_back(v);
			}
		}
		waterModel.CreateVertexBuffer(renderSystem.core.device, &batch, vertices.data(), vertices.size(), sizeof(Vertex));
		waterModel.calculateBounds();
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

	meshColorTexture.InitUAV(renderSystem.core.device, TextureSize, TextureSize, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	meshColorTexture.SetName("WaterColorTexture");
	resources.descriptors.createUAVView(meshColorTexture);
	resources.descriptors.createTextureView(meshColorTexture);
	resources.textures.setNamedTexture("WaterColorTexture", meshColorTexture.view);

	auto e = sceneMgr.createEntity("WaterSim", Order::Normal);
	e->geometry.fromModel(waterModel);
	e->setBoundingBox(waterModel.bbox);
	e->material = resources.materials.getMaterial("WaterSim", batch);
	e->setPosition({ -20, -20, 0 });

	auto csShader = resources.shaders.getShader("continuityComputeShader", ShaderTypeCompute, ShaderRef{ "waterSim/frenzySimContinuityCS.hlsl", "CS_Continuity", "cs_6_6" }); //frenzySimContinuityCS
	continuityComputeShader.init(*renderSystem.core.device, *csShader);

	csShader = resources.shaders.getShader("momentumComputeShader", ShaderTypeCompute, ShaderRef{ "waterSim/frenzySimMomentumCS.hlsl", "CS_Momentum", "cs_6_6" }); //frenzySimMomentumCS
	momentumComputeShader.init(*renderSystem.core.device, *csShader);

	csShader = resources.shaders.getShader("textureToMeshCS", ShaderTypeCompute, ShaderRef{ "utils/heightmapToMeshCS.hlsl", "CSMain", "cs_6_6" });
	textureToMeshCS.init(*renderSystem.core.device, *csShader);

	csShader = resources.shaders.getShader("copyTexturesCS", ShaderTypeCompute, ShaderRef{ "utils/copyTexturesCS.hlsl", "CSMain", "cs_6_6" });
	copyTexturesCS.init(*renderSystem.core.device, *csShader);

	csShader = resources.shaders.getShader("meshTextureCS", ShaderTypeCompute, ShaderRef{ "waterSim/colorWaterSimTexture.hlsl", "CSMain", "cs_6_6" });
	meshTextureCS.init(*renderSystem.core.device, *csShader);
}

bool first = true;

void WaterSim::update(ID3D12GraphicsCommandList* commandList, float dt, UINT frameIdx)
{
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


		first = false;
	}

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

	const float gridSpacing = 0.1f;
	{
		momentumComputeShader.dispatch(commandList, TextureSize, dt, gridSpacing, t);

		auto uavb = CD3DX12_RESOURCE_BARRIER::UAV(waterVelocity[frameIdx].texture.Get());
		commandList->ResourceBarrier(1, &uavb);

		continuityComputeShader.dispatch(commandList, TextureSize, dt, gridSpacing, t);

		uavb = CD3DX12_RESOURCE_BARRIER::UAV(waterHeight[frameIdx].texture.Get());
		commandList->ResourceBarrier(1, &uavb);
	}
// 	{
// 		momentumComputeShader.dispatch(commandList, TextureSize, dt, gridSpacing, t2);
// 
// 		auto uavb = CD3DX12_RESOURCE_BARRIER::UAV(waterVelocity[prevFrameIdx].texture.Get());
// 		commandList->ResourceBarrier(1, &uavb);
// 
// 		continuityComputeShader.dispatch(commandList, TextureSize, dt, gridSpacing, t2);
// 
// 		uavb = CD3DX12_RESOURCE_BARRIER::UAV(waterHeight[prevFrameIdx].texture.Get());
// 		commandList->ResourceBarrier(1, &uavb);
// 	}
// 	{
// 		momentumComputeShader.dispatch(commandList, TextureSize, dt, gridSpacing, t);
// 
// 		auto uavb = CD3DX12_RESOURCE_BARRIER::UAV(waterVelocity[frameIdx].texture.Get());
// 		commandList->ResourceBarrier(1, &uavb);
// 
// 		continuityComputeShader.dispatch(commandList, TextureSize, dt, gridSpacing, t);
// 
// 		uavb = CD3DX12_RESOURCE_BARRIER::UAV(waterHeight[frameIdx].texture.Get());
// 		commandList->ResourceBarrier(1, &uavb);
// 	}

	waterHeight[frameIdx].TransitionTarget(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	textureToMeshCS.dispatch(commandList, waterHeight[frameIdx].view.srvHandles, TextureSize, TextureSize, waterModel.vertexBuffer);

	{
		meshColorTexture.TransitionTarget(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		meshTextureCS.dispatch(commandList, { {TextureSize, TextureSize}, terrainHeight.view.srvHeapIndex, waterHeight[frameIdx].view.srvHeapIndex, waterVelocity[frameIdx].view.srvHeapIndex, meshColorTexture.view.uavHeapIndex });

		meshColorTexture.TransitionTarget(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	waterHeight[frameIdx].TransitionTarget(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

void WaterSim::clear()
{
}
