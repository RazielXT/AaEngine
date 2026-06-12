#include "FrameCompositor/Tasks/ImguiDebugWindowTask.h"
#include "App/DebugWindow.h"

ImguiDebugWindowTask::ImguiDebugWindowTask(RenderProvider p, RenderWorld& w) : CompositorTask(p, w)
{
}

ImguiDebugWindowTask::~ImguiDebugWindowTask()
{
}

void ImguiDebugWindowTask::recordCommands(RenderContext& ctx, CommandsData& commands, CompositorPass& pass)
{
	CommandsMarker marker(commands.commandList, "Imgui", PixColor::Orange);

	pass.targets.front().texture->PrepareAsRenderTarget(commands.commandList, pass.targets.front().previousState);

	imgui::DebugWindow::Get().draw(commands.commandList, provider.resources.materials);
}
