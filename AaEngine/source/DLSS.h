#pragma once

#include "directx/d3d12.h"
#include "nvsdk_ngx_params.h"
#include "MathUtils.h"
#include "UpscaleTypes.h"

class RenderSystem;

class DLSS
{
public:
	
	DLSS(RenderSystem& rs);

	void shutdown();

	bool selectMode(UpscaleMode);
	void onResize();

	bool enabled() const;
	UpscaleMode selectedMode() const;
	XMUINT2 getRenderSize() const;

	struct UpscaleInput
	{
		ID3D12Resource* unresolvedColorResource;
		ID3D12Resource* motionVectorsResource;
		ID3D12Resource* depthResource;
		ID3D12Resource* resolvedColorResource;

		float tslf;
	};
	bool upscale(ID3D12GraphicsCommandList* commandList, const UpscaleInput& input);

	XMFLOAT2 getJitter() const;

private:

	bool initLibrary();

	NVSDK_NGX_Parameter* ngxParameters = nullptr;
	NVSDK_NGX_Handle* dlssFeature = nullptr;

	struct DlssRecommendedSettings
	{
		XMUINT2 ngxRecommendedOptimalRenderSize = {};
	};

	struct UpscaleInfo
	{
		NVSDK_NGX_PerfQuality_Value type;
		DlssRecommendedSettings settings;
		float lodBias;
	};

	std::vector<UpscaleInfo> upscaleTypes = {
		{ NVSDK_NGX_PerfQuality_Value_UltraPerformance },
		{ NVSDK_NGX_PerfQuality_Value_MaxPerf },
		{ NVSDK_NGX_PerfQuality_Value_Balanced },
		{ NVSDK_NGX_PerfQuality_Value_MaxQuality },
		{ NVSDK_NGX_PerfQuality_Value_DLAA },
		{ NVSDK_NGX_PerfQuality_Value_DLAA }
	};

	UpscaleMode selectedUpscale = UpscaleMode::Off;
	bool compatible = true;

	void updateRenderSizeInfo();
	float getMipLodBias() const;

	RenderSystem& renderSystem;

	UINT frameIndex = 0;
	bool reset = true;
};