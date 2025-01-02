#include "ImguiDebugWindowTask.h"
#include "DebugWindow.h"

ImguiDebugWindowTask::ImguiDebugWindowTask(RenderProvider p, SceneManager& s) : CompositorTask(p, s)
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
	CommandsMarker marker(syncCommands.commandList, "Imgui");

	pass.target.texture->PrepareAsTarget(syncCommands.commandList, pass.target.previousState);

	imgui::DebugWindow::Get().draw(syncCommands.commandList, provider.resources.materials);
}

bool ImguiDebugWindowTask::writesSyncCommands(CompositorPass&) const
{
	return true;
}
