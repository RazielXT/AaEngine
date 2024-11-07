#include "DebugOverlayTask.h"
#include "AaMaterial.h"
#include "AaMaterialResources.h"
#include "DebugWindow.h"

DebugOverlayTask::DebugOverlayTask(RenderProvider p, AaSceneManager& s) : CompositorTask(p, s)
{
}

DebugOverlayTask::~DebugOverlayTask()
{
}

AsyncTasksInfo DebugOverlayTask::initialize(CompositorPass& pass)
{
	material = AaMaterialResources::get().getMaterial("TexturePreview")->Assign({}, pass.target.texture->formats);

	return {};
}

void DebugOverlayTask::resize(CompositorPass& pass)
{
	quad.SetPosition({ }, 0.5f, ScreenQuad::TopRight, pass.target.texture->width / float(pass.target.texture->height));
}

void DebugOverlayTask::run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass)
{
	pass.target.texture->PrepareAsTarget(syncCommands.commandList, provider.renderSystem->frameIndex, pass.target.previousState, false, false);

	auto idx = imgui::DebugWindow::Get().state.TexturePreviewIndex;

	if (idx >= 0)
	{
		quad.data.textureIndex = UINT(idx);

		quad.Render(material, provider, ctx, syncCommands.commandList);
	}
}

bool DebugOverlayTask::isSync() const
{
	return true;
}
