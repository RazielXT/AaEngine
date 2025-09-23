#include "DLSS.h"
#include "nvsdk_ngx.h"
#include "nvsdk_ngx_helpers.h"
#include "FileLogger.h"
#include <format>
#include <vector>
#include "RenderSystem.h"

static const char* DlssProjectId = "75C7DC62-22B9-49E9-A4B0-4311100B6F76"; //random

DLSS::DLSS(RenderSystem& rs) : renderSystem(rs)
{
}

bool DLSS::initLibrary()
{
	if (ngxParameters)
		return true;

	NVSDK_NGX_Result Result = NVSDK_NGX_D3D12_Init_with_ProjectID(DlssProjectId, NVSDK_NGX_ENGINE_TYPE_CUSTOM, "1.0", L".", renderSystem.core.device);
	if (NVSDK_NGX_FAILED(Result))
	{
		FileLogger::logWarning(std::format("NVSDK_NGX_D3D12_Init_with_ProjectID failed, code = {}", (int)Result));
		return false;
	}

	Result = NVSDK_NGX_D3D12_GetCapabilityParameters(&ngxParameters);
	if (NVSDK_NGX_FAILED(Result))
	{
		FileLogger::logWarning(std::format("NVSDK_NGX_GetCapabilityParameters failed, code = {}", (int)Result));
		shutdown();
		return false;
	}

	int dlssAvailable = 0;
	NVSDK_NGX_Result ResultDLSS = ngxParameters->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &dlssAvailable);
	if (ResultDLSS != NVSDK_NGX_Result_Success || !dlssAvailable)
	{
		NVSDK_NGX_Result FeatureInitResult = NVSDK_NGX_Result_Fail;
		NVSDK_NGX_Parameter_GetI(ngxParameters, NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult, (int*)&FeatureInitResult);
		FileLogger::logWarning(std::format("NVIDIA DLSS not available on this hardward/platform., FeatureInitResult = {}", (int)FeatureInitResult));
		shutdown();
		return false;
	}

	updateRenderSizeInfo();

	return true;
}

void DLSS::updateRenderSizeInfo()
{
	XMUINT2 windowSize = { renderSystem.viewport.getWidth(), renderSystem.viewport.getHeight() };

	uint32_t tmpU;
	float tmpF;

	for (auto& upscaleInfo : upscaleTypes)
	{
		NVSDK_NGX_Result Result = NGX_DLSS_GET_OPTIMAL_SETTINGS(ngxParameters,
			windowSize.x, windowSize.y, upscaleInfo.type,
			&upscaleInfo.settings.ngxRecommendedOptimalRenderSize.x, &upscaleInfo.settings.ngxRecommendedOptimalRenderSize.y,
			&tmpU, &tmpU, &tmpU, &tmpU, &tmpF);

		upscaleInfo.lodBias = std::log2f(float(upscaleInfo.settings.ngxRecommendedOptimalRenderSize.x) / windowSize.x) - 1.0f;
	}

	//off
	upscaleTypes.back().lodBias = 0;
}

float DLSS::getMipLodBias() const
{
	return std::log2f(getRenderSize().x / float(renderSystem.getOutputSize().x)) - 1.0f;
}

DirectX::XMFLOAT2 DLSS::getJitter() const
{
	// Halton jitter
	XMFLOAT2 Result(0.0f, 0.0f);

	constexpr int BaseX = 2;
	int Index = frameIndex + 1;
	float InvBase = 1.0f / BaseX;
	float Fraction = InvBase;
	while (Index > 0)
	{
		Result.x += (Index % BaseX) * Fraction;
		Index /= BaseX;
		Fraction *= InvBase;
	}

	constexpr int BaseY = 3;
	Index = frameIndex + 1;
	InvBase = 1.0f / BaseY;
	Fraction = InvBase;
	while (Index > 0)
	{
		Result.y += (Index % BaseY) * Fraction;
		Index /= BaseY;
		Fraction *= InvBase;
	}

	Result.x -= 0.5f;
	Result.y -= 0.5f;

	return Result;
}

void DLSS::shutdown()
{
	NVSDK_NGX_D3D12_DestroyParameters(ngxParameters);
	NVSDK_NGX_D3D12_Shutdown1(nullptr);
}

bool DLSS::selectMode(UpscaleMode m)
{
	renderSystem.core.WaitForAllFrames();

	if (dlssFeature)
	{
		NVSDK_NGX_D3D12_ReleaseFeature(dlssFeature);
		dlssFeature = nullptr;
	}

	if (m != UpscaleMode::Off && compatible && !initLibrary())
	{
		compatible = false;
		return false;
	}

	selectedUpscale = m;

	DescriptorManager::get().initializeSamplers(getMipLodBias());

	if (selectedUpscale == UpscaleMode::Off)
		return true;

	reset = true;

	int DlssCreateFeatureFlags = NVSDK_NGX_DLSS_Feature_Flags_None;
	DlssCreateFeatureFlags |= NVSDK_NGX_DLSS_Feature_Flags_MVLowRes;
	//DlssCreateFeatureFlags |= isContentHDR ? NVSDK_NGX_DLSS_Feature_Flags_IsHDR : 0;
	DlssCreateFeatureFlags |= NVSDK_NGX_DLSS_Feature_Flags_DepthInverted;
	//DlssCreateFeatureFlags |= enableAutoExposure ? NVSDK_NGX_DLSS_Feature_Flags_AutoExposure : 0;

	NVSDK_NGX_DLSS_Create_Params DlssCreateParams{};

	XMUINT2 windowSize = renderSystem.getOutputSize();

	auto& selectedInfo = upscaleTypes[(int)selectedUpscale];
	DlssCreateParams.Feature.InWidth = selectedInfo.settings.ngxRecommendedOptimalRenderSize.x;
	DlssCreateParams.Feature.InHeight = selectedInfo.settings.ngxRecommendedOptimalRenderSize.y;
	DlssCreateParams.Feature.InTargetWidth = windowSize.x;
	DlssCreateParams.Feature.InTargetHeight = windowSize.y;
	DlssCreateParams.Feature.InPerfQualityValue = selectedInfo.type;
	DlssCreateParams.InFeatureCreateFlags = DlssCreateFeatureFlags;

	// Select render preset (DL weights)
	const int renderPreset = NVSDK_NGX_DLSS_Hint_Render_Preset_K; // NVSDK_NGX_DLSS_Hint_Render_Preset_J;
	NVSDK_NGX_Parameter_SetUI(ngxParameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_DLAA, renderPreset);              // will remain the chosen weights after OTA
	NVSDK_NGX_Parameter_SetUI(ngxParameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Quality, renderPreset);           // ^
	NVSDK_NGX_Parameter_SetUI(ngxParameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Balanced, renderPreset);          // ^
	NVSDK_NGX_Parameter_SetUI(ngxParameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Performance, renderPreset);       // ^
	NVSDK_NGX_Parameter_SetUI(ngxParameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_UltraPerformance, renderPreset);  // ^

	{
		auto commands = renderSystem.core.CreateCommandList(L"DLSS", PixColor::Nvidia);
		renderSystem.core.StartCommandList(commands);

		const unsigned int CreationNodeMask = 1;
		const unsigned int VisibilityNodeMask = 1;
		auto ResultDLSS = NGX_D3D12_CREATE_DLSS_EXT(commands.commandList, CreationNodeMask, VisibilityNodeMask, &dlssFeature, ngxParameters, &DlssCreateParams);

		if (NVSDK_NGX_FAILED(ResultDLSS))
		{
			FileLogger::logError(std::format("Failed to create DLSS Features = {}", (int)ResultDLSS));
			compatible = false;
			return false;
		}

		renderSystem.core.ExecuteCommandList(commands);
		renderSystem.core.WaitForCurrentFrame();
		commands.deinit();
	}

	return true;
}

void DLSS::onResize()
{
	if (enabled())
	{
		updateRenderSizeInfo();

		selectMode(selectedUpscale);
	}
}

bool DLSS::enabled() const
{
	return selectedUpscale != UpscaleMode::Off;
}

UpscaleMode DLSS::selectedMode() const
{
	return selectedUpscale;
}

DirectX::XMUINT2 DLSS::getRenderSize() const
{
	return upscaleTypes[(int)selectedUpscale].settings.ngxRecommendedOptimalRenderSize;
}

bool DLSS::upscale(ID3D12GraphicsCommandList* commandList, const UpscaleInput& input)
{
	if (!dlssFeature)
		return false;

	NVSDK_NGX_D3D12_DLSS_Eval_Params D3D12DlssEvalParams;

	memset(&D3D12DlssEvalParams, 0, sizeof(D3D12DlssEvalParams));

	D3D12DlssEvalParams.Feature.pInColor = input.unresolvedColorResource;
	D3D12DlssEvalParams.Feature.pInOutput = input.resolvedColorResource;
	D3D12DlssEvalParams.pInDepth = input.depthResource;
	D3D12DlssEvalParams.pInMotionVectors = input.motionVectorsResource;
	//D3D12DlssEvalParams.pInExposureTexture = input.exposureResource;

	auto jitter = getJitter();
	D3D12DlssEvalParams.InJitterOffsetX = jitter.x * 0.5f;
	D3D12DlssEvalParams.InJitterOffsetY = jitter.y * 0.5f;

	D3D12DlssEvalParams.InReset = reset;
	reset = false;

	D3D12DlssEvalParams.InFrameTimeDeltaInMsec = input.tslf * 1000.f;
	D3D12DlssEvalParams.InMVScaleX = 1;
	D3D12DlssEvalParams.InMVScaleY = 1;
	NVSDK_NGX_Coordinates renderingOffset = {};
	D3D12DlssEvalParams.InColorSubrectBase = renderingOffset;
	D3D12DlssEvalParams.InDepthSubrectBase = renderingOffset;
	D3D12DlssEvalParams.InTranslucencySubrectBase = renderingOffset;
	D3D12DlssEvalParams.InMVSubrectBase = renderingOffset;

	NVSDK_NGX_Dimensions renderingSize = { getRenderSize().x, getRenderSize().y };
	D3D12DlssEvalParams.InRenderSubrectDimensions = renderingSize;

	NVSDK_NGX_Result Result = NGX_D3D12_EVALUATE_DLSS_EXT(commandList, dlssFeature, ngxParameters, &D3D12DlssEvalParams);

	frameIndex = (frameIndex + 1) % 64;

	return Result == NVSDK_NGX_Result_Success;
}
