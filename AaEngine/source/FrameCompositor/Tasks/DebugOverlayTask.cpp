#include "FrameCompositor/Tasks/DebugOverlayTask.h"
#include "Resources/Material/Material.h"

static const std::initializer_list<D3D12_SRV_DIMENSION> TextureDimensions = { D3D12_SRV_DIMENSION_TEXTURE2D, D3D12_SRV_DIMENSION_TEXTURE3D };

static bool IsUintFormat(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R32G32B32A32_UINT:
	case DXGI_FORMAT_R32G32B32_UINT:
	case DXGI_FORMAT_R16G16B16A16_UINT:
	case DXGI_FORMAT_R32G32_UINT:
	case DXGI_FORMAT_R10G10B10A2_UINT:
	case DXGI_FORMAT_R8G8B8A8_UINT:
	case DXGI_FORMAT_R16G16_UINT:
	case DXGI_FORMAT_R32_UINT:
	case DXGI_FORMAT_R8G8_UINT:
	case DXGI_FORMAT_R16_UINT:
	case DXGI_FORMAT_R8_UINT:
		return true;
	default:
		return false;
	}
}

DebugOverlayTask* instance = nullptr;

DebugOverlayTask::DebugOverlayTask(RenderProvider p, RenderWorld& w) : CompositorTask(p, w)
{
	instance = this;
}

DebugOverlayTask::~DebugOverlayTask()
{
	instance = nullptr;
}

AsyncTasksInfo DebugOverlayTask::initialize(CompositorPass& pass)
{
	auto format = pass.targets.front().texture->format;
	material = provider.resources.materials.getMaterial("TexturePreview")->Assign({}, { format });
	material3D = provider.resources.materials.getMaterial("TexturePreview3D")->Assign({}, { format });
	materialUint = provider.resources.materials.getMaterial("TexturePreviewUint")->Assign({}, { format });
	material3DUint = provider.resources.materials.getMaterial("TexturePreview3DUint")->Assign({}, { format });

	return {};
}

void DebugOverlayTask::resize(CompositorPass& pass)
{
	screenSize = { (float)pass.targets.front().texture->width, (float)pass.targets.front().texture->height };
	updateQuad();
}

void DebugOverlayTask::run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass)
{
	pass.targets.front().texture->PrepareAsRenderTarget(syncCommands.commandList, pass.targets.front().previousState);

	auto idx = currentIdx();

	if (enabled)
	{
		CommandsMarker marker(syncCommands.commandList, "DebugOverlay", PixColor::Debug);

		auto info = getCurrentDescriptor();
		bool is3D = info && info->dimension == D3D12_SRV_DIMENSION_TEXTURE3D;
		bool isUint = info && IsUintFormat(info->resource->GetDesc().Format);

		if (is3D)
		{
			TexturePreview3DData data3d;
			data3d.textureIndex = idx;
			data3d.sliceIndex = sliceIdx;
			data3d.remapMin = remapEnabled ? remapMinMax.x : 0.0f;
			data3d.remapMax = remapEnabled ? remapMinMax.y : 1.0f;
			auto* mat = isUint ? material3DUint : material3D;
			quad.Render(mat, *pass.targets.front().texture, provider, ctx, syncCommands.commandList, &data3d, sizeof(data3d));
		}
		else
		{
			TexturePreviewData data2d;
			data2d.textureIndex = idx;
			data2d.remapMin = remapEnabled ? remapMinMax.x : 0.0f;
			data2d.remapMax = remapEnabled ? remapMinMax.y : 1.0f;
			auto* mat = isUint ? materialUint : material;
			quad.Render(mat, *pass.targets.front().texture, provider, ctx, syncCommands.commandList, &data2d, sizeof(data2d));
		}
	}
}

void DebugOverlayTask::enable(bool e)
{
	enabled = e;
}

DebugOverlayTask& DebugOverlayTask::Get()
{
	return *instance;
}

void DebugOverlayTask::changeIdx(UINT next)
{
	auto nextId = 0;

	if (next == 0)
		nextId = 0;
	else if (next > current)
		nextId = DescriptorManager::get().nextDescriptor(current, TextureDimensions);
	else
		nextId = DescriptorManager::get().previousDescriptor(current, TextureDimensions);

	setIdx(nextId);
}

void DebugOverlayTask::setIdx(UINT idx)
{
	auto& dm = DescriptorManager::get();

	UINT oldDepth =
		(UINT)dm.getDescriptor(current)->resource->GetDesc().DepthOrArraySize;

	// Avoid divide-by-zero and keep normalized position
	float t = (oldDepth > 1)
		? (float)sliceIdx / (float)(oldDepth - 1)
		: 0.0f;

	current = idx;

	UINT newDepth =
		(UINT)dm.getDescriptor(current)->resource->GetDesc().DepthOrArraySize;

	sliceIdx = (newDepth > 1)
		? (UINT)round(t * (float)(newDepth - 1))
		: 0;

	updateQuad();
}

UINT DebugOverlayTask::currentIdx() const
{
	return current;
}

const DescriptorManager::DescriptorInfo* DebugOverlayTask::getCurrentDescriptor() const
{
	return current >= 0 ? DescriptorManager::get().getDescriptor(current) : nullptr;
}

bool DebugOverlayTask::isCurrentTexture3D() const
{
	auto info = getCurrentDescriptor();
	return info && info->dimension == D3D12_SRV_DIMENSION_TEXTURE3D;
}

PreviewMode DebugOverlayTask::getPreviewMode() const
{
	return previewMode;
}

void DebugOverlayTask::setPreviewMode(PreviewMode mode)
{
	previewMode = mode;
	updateQuad();
}

void DebugOverlayTask::updateQuad()
{
	switch (previewMode)
	{
	case PreviewMode::Fullscreen:
		quad.SetPosition(screenSize);
		break;

	case PreviewMode::Fit:
	{
		auto info = getCurrentDescriptor();
		if (info && screenSize.x > 0.0f && screenSize.y > 0.0f)
		{
			auto desc = info->resource->GetDesc();

			const float textureWidth = (float)desc.Width;
			const float textureHeight = (float)desc.Height;

			// Match source texture pixel size in screen space without stretching.
			const float widthRatio = textureWidth / screenSize.x;
			const float heightRatio = textureHeight / screenSize.y;
			const float size = max(widthRatio, heightRatio);
			const float aspect = (screenSize.x / screenSize.y) / (textureWidth / textureHeight);

			quad.SetPosition({}, size, ScreenQuad::TopRight, aspect);
		}
		else
		{
			// Fallback to preview mode if no texture
			quad.SetPosition({}, 0.5f, ScreenQuad::TopRight, screenSize.x / screenSize.y);
		}
		break;
	}

	case PreviewMode::Preview:
	default:
		quad.SetPosition({}, 0.5f, ScreenQuad::TopRight, screenSize.x / screenSize.y);
		break;
	}
}

UINT DebugOverlayTask::getSliceIdx() const
{
	return sliceIdx;
}

void DebugOverlayTask::setSliceIdx(UINT idx)
{
	sliceIdx = idx;
}

bool DebugOverlayTask::isRemapEnabled() const
{
	return remapEnabled;
}

void DebugOverlayTask::setRemapEnabled(bool e)
{
	remapEnabled = e;
}

Vector2 DebugOverlayTask::getRemapMinMax() const
{
	return remapMinMax;
}

void DebugOverlayTask::setRemapMinMax(Vector2 v)
{
	remapMinMax = v;
}

std::vector<DescriptorManager::DescriptorInfo> DebugOverlayTask::getTextureList() const
{
	return DescriptorManager::get().getDescriptors(TextureDimensions);
}
