#include "ImguiDebugWindowTask.h"
#include "DebugWindow.h"

ImguiDebugWindowTask::ImguiDebugWindowTask(RenderProvider p, AaSceneManager& s) : CompositorTask(p, s)
{
}

ImguiDebugWindowTask::~ImguiDebugWindowTask()
{
}

AsyncTasksInfo ImguiDebugWindowTask::initialize(CompositorPass&)
{
	return {};
}

void ImguiDebugWindowTask::run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass)
{
	pass.target.texture->PrepareAsTarget(syncCommands.commandList, provider.renderSystem->frameIndex, pass.target.previousState, false);

	imgui::DebugWindow::Get().draw(syncCommands.commandList);
}

bool ImguiDebugWindowTask::writesSyncCommands() const
{
	return true;
}
