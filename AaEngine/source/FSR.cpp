#include "FSR.h"
#include <ffx_api/ffx_upscale.hpp>
#include "RenderSystem.h"
#include "AaLogger.h"
#include <format>
#include "AaCamera.h"

static void FfxMsgCallback(uint32_t type, const wchar_t* message)
{
	size_t len = wcstombs(nullptr, message, 0) + 1;

	std::string buffer;
	buffer.resize(len);
	wcstombs(buffer.data(), message, len);

	if (type == FFX_API_MESSAGE_TYPE_ERROR)
	{
		AaLogger::logError(buffer);
	}
	else if (type == FFX_API_MESSAGE_TYPE_WARNING)
	{
		AaLogger::logWarning(buffer);
	}
}

void FSR::init(RenderSystem* rs)
{
	renderSystem = rs;

	for (auto& mode : upscaleTypes)
	{
		mode.lodBias = std::log2f(1.f / mode.scale) - 1.f + std::numeric_limits<float>::epsilon();
	}
}

void FSR::shutdown()
{
	if (m_UpscalingContext)
	{
		ffx::DestroyContext(m_UpscalingContext);
		m_UpscalingContext = nullptr;
	}
}

void FSR::selectMode(UpscaleMode m)
{
	selectedUpscale = m;

	if (m == UpscaleMode::Off)
	{
		shutdown();
		return;
	}

	if (!m_UpscalingContext)
		initializeContext();
	else
		reloadUpscaleParams();
}

void FSR::onScreenResize()
{
	if (!m_UpscalingContext)
		return;

	shutdown();
	initializeContext();
}

void FSR::initializeContext()
{
	if (!enabled())
		return;

	ffx::CreateBackendDX12Desc backendDesc{};
	backendDesc.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_DX12;
	backendDesc.device = renderSystem->device;

	ffx::CreateContextDescUpscale createFsr{};

	auto size = renderSystem->getOutputSize();
	createFsr.maxUpscaleSize = { size.x, size.y };
	createFsr.maxRenderSize = { size.x, size.y };
	createFsr.flags |= FFX_UPSCALE_ENABLE_DEPTH_INVERTED;// | FFX_UPSCALE_ENABLE_DEPTH_INFINITE;

#if defined(_DEBUG)
	createFsr.flags |= FFX_UPSCALE_ENABLE_DEBUG_CHECKING;
	createFsr.fpMessage = &FfxMsgCallback;
#endif

	// Create the FSR context
	{
		ffx::ReturnCode retCode = ffx::CreateContext(m_UpscalingContext, nullptr, createFsr, backendDesc);

		if (retCode != ffx::ReturnCode::Ok)
		{
			AaLogger::logError(std::format("FSR ffx::CreateContext error code {}", (int)retCode));
			return;
		}
	}

	reloadUpscaleParams();
	updateJitter();

// 	FfxApiEffectMemoryUsage gpuMemoryUsageUpscaler;
// 	ffx::QueryDescUpscaleGetGPUMemoryUsage upscalerGetGPUMemoryUsage{};
// 	upscalerGetGPUMemoryUsage.gpuMemoryUsageUpscaler = &gpuMemoryUsageUpscaler;
// 
// 	ffx::Query(m_UpscalingContext, upscalerGetGPUMemoryUsage);
}

void FSR::reloadUpscaleParams()
{
	reset = true;

	auto size = renderSystem->getOutputSize();

	for (auto& mode : upscaleTypes)
	{
		mode.renderSize = { UINT(size.x / mode.scale), UINT(size.y / mode.scale) };
	}
}

bool FSR::enabled() const
{
	return selectedUpscale != UpscaleMode::Off;
}

UpscaleMode FSR::selectedMode() const
{
	return selectedUpscale;
}

DirectX::XMUINT2 FSR::getRenderSize() const
{
	return upscaleTypes[(int)selectedUpscale].renderSize;
}

float FSR::getMipLodBias() const
{
	return upscaleTypes[(int)selectedUpscale].lodBias;
}

bool FSR::upscale(ID3D12GraphicsCommandList* commandList, const UpscaleInput& input, AaCamera& camera)
{
	// FFXAPI
	// All cauldron resources come into a render module in a generic read state (ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource)
	ffx::DispatchDescUpscale dispatchUpscale{};

	dispatchUpscale.commandList = commandList;

	dispatchUpscale.color = ffxApiGetResourceDX12(input.unresolvedColorResource, FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
	dispatchUpscale.depth = ffxApiGetResourceDX12(input.depthResource, FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
	dispatchUpscale.motionVectors = ffxApiGetResourceDX12(input.motionVectorsResource, FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
	dispatchUpscale.output = ffxApiGetResourceDX12(input.resolvedColorResource, FFX_API_RESOURCE_STATE_UNORDERED_ACCESS);

// 	if (m_MaskMode != FSRMaskMode::Disabled)
// 	{
// 		dispatchUpscale.reactive = SDKWrapper::ffxGetResourceApi(m_pReactiveMask->GetResource(), FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
// 	}
// 	else
// 	{
// 		dispatchUpscale.reactive = SDKWrapper::ffxGetResourceApi(nullptr, FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
// 	}
// 
// 	if (m_UseMask)
// 	{
// 		dispatchUpscale.transparencyAndComposition =
// 			SDKWrapper::ffxGetResourceApi(m_pCompositionMask->GetResource(), FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
// 	}
// 	else
// 	{
// 		dispatchUpscale.transparencyAndComposition = SDKWrapper::ffxGetResourceApi(nullptr, FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
// 	}

	auto jitter = getJitter();

	// Jitter is calculated earlier in the frame using a callback from the camera update
	dispatchUpscale.jitterOffset.x = jitter.x;
	dispatchUpscale.jitterOffset.y = jitter.y;
	dispatchUpscale.reset = reset;
	reset = false;

	dispatchUpscale.enableSharpening = true;
	dispatchUpscale.sharpness = 0.8f;

	// Cauldron keeps time in seconds, but FSR expects milliseconds
	dispatchUpscale.frameTimeDelta = input.tslf * 1000.f;

	auto renderSize = getRenderSize();
	dispatchUpscale.motionVectorScale.x = 1;
	dispatchUpscale.motionVectorScale.y = 1;
	dispatchUpscale.renderSize.width = renderSize.x;
	dispatchUpscale.renderSize.height = renderSize.y;

	auto outputSize = renderSystem->getOutputSize();
	dispatchUpscale.upscaleSize.width = outputSize.x;
	dispatchUpscale.upscaleSize.height = outputSize.y;

	dispatchUpscale.preExposure = 1.f;

	// Setup camera params as required

	auto& cameraParam = camera.getParams();
	dispatchUpscale.cameraFovAngleVertical = XM_PIDIV2 / cameraParam.aspectRatio;

//	if (s_InvertedDepth)
	{
		dispatchUpscale.cameraFar = cameraParam.nearZ;
		dispatchUpscale.cameraNear = cameraParam.farZ; // FLT_MAX;
	}
// 	else
// 	{
// 		dispatchUpscale.cameraFar = pCamera->GetFarPlane();
// 		dispatchUpscale.cameraNear = pCamera->GetNearPlane();
// 	}

	//dispatchUpscale.flags |= FFX_UPSCALE_FLAG_DRAW_DEBUG_VIEW;

	ffx::ReturnCode retCode = ffx::Dispatch(m_UpscalingContext, dispatchUpscale);

	updateJitter();

	return retCode == ffx::ReturnCode::Ok;
}

DirectX::XMFLOAT2 FSR::getJitter() const
{
	return m_Jitter;
}

void FSR::updateJitter()
{
	int32_t jitterPhaseCount{};

	ffx::QueryDescUpscaleGetJitterPhaseCount getJitterPhaseDesc{};
	getJitterPhaseDesc.displayWidth = renderSystem->getOutputSize().x;
	getJitterPhaseDesc.renderWidth = getRenderSize().x;
	getJitterPhaseDesc.pOutPhaseCount = &jitterPhaseCount;

	auto retCode = ffx::Query(m_UpscalingContext, getJitterPhaseDesc);

	ffx::QueryDescUpscaleGetJitterOffset getJitterOffsetDesc{};
	getJitterOffsetDesc.index = ++frameIndex;
	getJitterOffsetDesc.phaseCount = jitterPhaseCount;
	getJitterOffsetDesc.pOutX = &m_Jitter.x;
	getJitterOffsetDesc.pOutY = &m_Jitter.y;

	retCode = ffx::Query(m_UpscalingContext, getJitterOffsetDesc);

// 	m_Jitter.x *= 0.5f;
// 	m_Jitter.y *= 0.5f;
}
