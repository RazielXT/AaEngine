#pragma once

#include "RenderCore/VCT/isoSeparate/IsoSeparateVoxelCascade.h"
#include "RenderCore/ShadowMaps.h"
#include "Resources/Compute/GenerateMipsComputeShader.h"
#include "Resources/Compute/ClearBufferCS.h"
#include "Resources/Compute/ComputeShader.h"
#include "Scene/RenderObject.h"
#include "Resources/Shader/ShaderDataBuffers.h"
#include <span>

struct RenderQueue;
class RenderWorld;
struct RenderContext;
struct CommandsData;
struct FrameParameters;
struct GraphicsResources;
class RenderSystem;

struct IsoSeparateVoxelTracingParams
{
	float voxelSteppingBounces = 0.075f;
	float voxelSteppingDiffuse = 0.0f;
	Vector2 middleConeRatioDistance = { 1.05f, 1.5f };
	Vector2 sideConeRatioDistance = { 2.2f, 5.f };
};

class IsoSeparateBounceVoxelsCS : public ComputeShader
{
public:

	void dispatch(ID3D12GraphicsCommandList* commandList, const std::span<float>& data, const ShaderTextureView& targetColor, const ShaderTextureView& targetOccupancy, const ShaderTextureView& sourceColor, const ShaderTextureView& sourceOccupancy, D3D12_GPU_VIRTUAL_ADDRESS);
};

class IsoSeparateOpacityGridCS : public ComputeShader
{
public:
	IsoSeparateOpacityGridCS()
	{
		volatileTextures = true;
	}

	void dispatch(ID3D12GraphicsCommandList* commandList, const ShaderTextureView& sourceOccupancy, const ShaderTextureView& targetOpacityGrid);
};

class IsoSeparateVoxelization
{
public:

	static constexpr UINT CascadesCount = 4;

	void initialize(RenderSystem& renderSystem, const FrameParameters& params, GraphicsResources& resources, ShadowMaps& shadowMaps, RenderWorld& renderWorld, DXGI_FORMAT outputFormat);
	void shutdown();

	void clear(ID3D12GraphicsCommandList* c);
	void revoxelize();

	void voxelizeCascades(CommandsData& commands, GpuTextureStates& viewportOutput, const RenderContext& ctx);
	void runBounces(CommandsData& computeCommands, const RenderContext& ctx, const IsoSeparateVoxelTracingParams& params);
	void transitionForCompute(ID3D12GraphicsCommandList* commandList);

	IsoSeparateVoxelCascade voxelCascades[CascadesCount];

private:

	XM_ALIGNED_STRUCT(16) SceneVoxelChunkInfo
	{
		Vector3 voxelOffset;
		float voxelDensity;
		Vector3 moveOffset;
		float voxelSceneSize;
		UINT texId;
		UINT texIdPrev;
		UINT texIdOccupancy;
		UINT texIdPrevOccupancy;
		UINT resIdDataBuffer;
	};

	XM_ALIGNED_STRUCT(16) SceneVoxelCbuffer
	{
		Vector2 middleConeRatioDistance = { 1, 0.9f };
		Vector2 sideConeRatioDistance = { 2, 0.8f };

		SceneVoxelChunkInfo cascadeInfo[CascadesCount];
	}
	cbufferData;

	CbufferView cbuffer;
	void updateCBufferCascade(SceneVoxelChunkInfo& info, Vector3 diff, IsoSeparateVoxelCascade& cascade);
	void updateCBuffer(const Vector3& cameraPosition, const IsoSeparateVoxelTracingParams& params);

	void voxelizeCascade(ID3D12GraphicsCommandList* commandList, TextureStatePair& voxelScene, TextureStatePair& prevVoxelScene, TextureStatePair& voxelOccupancy, TextureStatePair& prevVoxelOccupancy, GpuTextureStates& viewportOutput, IsoSeparateVoxelCascade& cascade, const RenderContext& ctx);
	void bounceCascade(CommandsData& commands, IsoSeparateVoxelCascade& cascade, const RenderContext& ctx);
	void renderShadowMap(ID3D12GraphicsCommandList* commandList, float extends, const RenderContext& ctx);

	UINT buildCounter[CascadesCount]{};
	bool reset = false;

	IsoSeparateBounceVoxelsCS bouncesCS;
	IsoSeparateOpacityGridCS opacityGridCS;
	Generate3DMips3xCS computeMips;
	ClearBufferComputeShader clearBufferCS;

	GpuTexture3D clearColorTexture;
	GpuTexture3D clearOccupancyTexture;

	ShadowMap shadowMap;
	RenderObjectsVisibilityData shadowRenderablesData;
	RenderObjectsStorage* shadowRenderables{};
	RenderQueue* shadowQueue{};
	RenderQueue* sceneQueue{};

	RenderWorld* renderWorld{};
	ShadowMaps* shadowMaps{};
	const FrameParameters* frameParams{};
};