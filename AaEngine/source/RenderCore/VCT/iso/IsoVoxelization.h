#pragma once

#include "RenderCore/VCT/iso/IsoVoxelCascade.h"
#include "RenderCore/ShadowMaps.h"
#include "Resources/Compute/GenerateMipsComputeShader.h"
#include "Resources/Compute/ClearBufferCS.h"
#include "Resources/Compute/ComputeShader.h"
#include "Scene/RenderObject.h"
#include "Resources/Shader/ShaderDataBuffers.h"
#include <span>

struct RenderQueue;
class SceneManager;
struct RenderContext;
struct CommandsData;
struct FrameParameters;
struct GraphicsResources;
class RenderSystem;

struct VoxelTracingParams
{
	float voxelSteppingBounces = 0.075f;
	float voxelSteppingDiffuse = 0.0f;
	Vector2 middleConeRatioDistance = { 1.05f, 1.5f };
	Vector2 sideConeRatioDistance = { 2.2f, 5.f };
};

class IsoBounceVoxelsCS : public ComputeShader
{
public:

	void dispatch(ID3D12GraphicsCommandList* commandList, const std::span<float>& data, const ShaderTextureView&, const ShaderTextureView&, D3D12_GPU_VIRTUAL_ADDRESS);
};

class IsoVoxelization
{
public:

	static constexpr UINT CascadesCount = 4;

	void initialize(RenderSystem& renderSystem, const FrameParameters& params, GraphicsResources& resources, ShadowMaps& shadowMaps, SceneManager& sceneMgr, DXGI_FORMAT outputFormat);
	void shutdown();

	void clear(ID3D12GraphicsCommandList* c);
	void revoxelize();

	void voxelizeCascades(CommandsData& commands, GpuTextureStates& viewportOutput, const RenderContext& ctx);
	void runBounces(CommandsData& computeCommands, const RenderContext& ctx, const VoxelTracingParams& params);
	void transitionForCompute(ID3D12GraphicsCommandList* commandList);

	IsoVoxelCascade voxelCascades[CascadesCount];

private:

	XM_ALIGNED_STRUCT(16) SceneVoxelChunkInfo
	{
		Vector3 voxelOffset;
		float voxelDensity;
		Vector3 moveOffset;
		float voxelSceneSize;
		UINT texId;
		UINT texIdPrev;
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
	void updateCBufferCascade(SceneVoxelChunkInfo& info, Vector3 diff, IsoVoxelCascade& cascade);
	void updateCBuffer(const Vector3& cameraPosition, const VoxelTracingParams& params);

	void voxelizeCascade(ID3D12GraphicsCommandList* commandList, TextureStatePair& voxelScene, TextureStatePair& prevVoxelScene, GpuTextureStates& viewportOutput, IsoVoxelCascade& cascade, const RenderContext& ctx);
	void bounceCascade(CommandsData& commands, IsoVoxelCascade& cascade, const RenderContext& ctx);
	void renderShadowMap(ID3D12GraphicsCommandList* commandList, float extends, const RenderContext& ctx);

	UINT buildCounter[CascadesCount]{};
	bool reset = false;

	IsoBounceVoxelsCS bouncesCS;
	Generate3DMips3xCS computeMips;
	ClearBufferComputeShader clearBufferCS;

	GpuTexture3D clearSceneTexture;

	ShadowMap shadowMap;
	RenderObjectsVisibilityData shadowRenderablesData;
	RenderObjectsStorage* shadowRenderables{};
	RenderQueue* shadowQueue{};
	RenderQueue* sceneQueue{};

	SceneManager* sceneMgr{};
	ShadowMaps* shadowMaps{};
	const FrameParameters* frameParams{};
};
