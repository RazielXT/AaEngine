#include "DebugOverlayTask.h"
#include "Material.h"

DebugOverlayTask* instance = nullptr;

DebugOverlayTask::DebugOverlayTask(RenderProvider p, SceneManager& s) : CompositorTask(p, s)
{
	instance = this;
}

DebugOverlayTask::~DebugOverlayTask()
{
	instance = nullptr;
}

AsyncTasksInfo DebugOverlayTask::initialize(CompositorPass& pass)
{
	material = provider.resources.materials.getMaterial("TexturePreview")->Assign({}, { pass.target.texture->format });

	return {};
}

void DebugOverlayTask::resize(CompositorPass& pass)
{
	screenSize = { (float)pass.target.texture->width, (float)pass.target.texture->height };
	updateQuad();
}

void DebugOverlayTask::run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass)
{
	pass.target.texture->PrepareAsRenderTarget(syncCommands.commandList, pass.target.previousState);

	auto idx = currentIdx();

	if (idx >= 0)
	{
		CommandsMarker marker(syncCommands.commandList, "DebugOverlay", PixColor::Debug);

		quad.data.textureIndex = UINT(idx);

		quad.Render(material, *pass.target.texture, provider, ctx, syncCommands.commandList);
	}
}

bool DebugOverlayTask::writesSyncCommands(CompositorPass&) const
{
	return true;
}

DebugOverlayTask& DebugOverlayTask::Get()
{
	return *instance;
}

void DebugOverlayTask::changeIdx(int next)
{
	if (next < 0)
		current = -1;
	else if (next == 0)
		current = 0;
	else if (next > current)
		current = DescriptorManager::get().nextDescriptor(current, D3D12_SRV_DIMENSION_TEXTURE2D);
	else
		current = DescriptorManager::get().previousDescriptor(current, D3D12_SRV_DIMENSION_TEXTURE2D);
}

int DebugOverlayTask::currentIdx() const
{
	return current;
}

const char* DebugOverlayTask::getCurrentIdxName() const
{
	return current >= 0 ? DescriptorManager::get().getDescriptorName(current) : nullptr;
}

bool DebugOverlayTask::isFullscreen() const
{
	return fullscreen;
}

void DebugOverlayTask::setFullscreen(bool f)
{
	fullscreen = f;
	updateQuad();
}

void DebugOverlayTask::updateQuad()
{
	if (fullscreen)
		quad.SetPosition(screenSize);
	else
		quad.SetPosition({}, 0.5f, ScreenQuad::TopRight, screenSize.x / screenSize.y);
}
