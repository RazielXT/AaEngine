#pragma once

#include "directx/d3d12.h"
#include <ffx_api/dx12/ffx_api_dx12.hpp>
#include "MathUtils.h"
#include "UpscaleTypes.h"

class RenderSystem;
class Camera;

class FSR
{
public:
	
	FSR(RenderSystem& rs);

	void shutdown();

	void selectMode(UpscaleMode);
	void onResize();

	bool enabled() const;
	UpscaleMode selectedMode() const;
	XMUINT2 getRenderSize() const;
	float getMipLodBias() const;

	struct UpscaleInput
	{
		ID3D12Resource* unresolvedColorResource;
		ID3D12Resource* motionVectorsResource;
		ID3D12Resource* depthResource;
		ID3D12Resource* resolvedColorResource;

		float tslf;
	};
	bool upscale(ID3D12GraphicsCommandList* commandList, const UpscaleInput& input, Camera& camera);

	XMFLOAT2 getJitter() const;

private:

	void initializeContext();
	void reloadUpscaleParams();

	ffx::Context m_UpscalingContext{};
	RenderSystem& renderSystem;

	UpscaleMode selectedUpscale = UpscaleMode::Off;

	struct UpscaleInfo
	{
		float scale{};
		float lodBias{};
		XMUINT2 renderSize{};
	};

	std::vector<UpscaleInfo> upscaleTypes = {
		{ 3.0f },
		{ 2.0f },
		{ 1.7f },
		{ 1.5f },
		{ 1.0f },
		{ 1.0f }
	};

	UINT frameIndex = 0;
	bool reset = true;

	void updateJitter();
	XMFLOAT2 m_Jitter{};
};
