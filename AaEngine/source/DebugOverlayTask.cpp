#include "DebugOverlayTask.h"
#include "AaMaterial.h"
#include "AaMaterialResources.h"
#include "DebugWindow.h"

DebugOverlayTask::DebugOverlayTask(RenderProvider p) : provider(p)
{
}

DebugOverlayTask::~DebugOverlayTask()
{
}

AsyncTasksInfo DebugOverlayTask::initialize(RenderTargetTexture* target)
{
	material = AaMaterialResources::get().getMaterial("TexturePreview")->Assign({}, target->formats);

	return {};
}

void DebugOverlayTask::resize(RenderTargetTexture* target)
{
	quad.SetPosition({ }, 0.5f, ScreenQuad::TopRight, target->width / float(target->height));
}

void DebugOverlayTask::prepare(RenderContext& ctx, CommandsData& syncCommands)
{
	auto idx = imgui::DebugWindow::Get().state.TexturePreviewIndex;

	if (idx >= 0)
	{
		quad.data.textureIndex = UINT(idx);

		quad.Render(material, provider, ctx, syncCommands.commandList);
	}
}
