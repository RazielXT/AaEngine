#include "VctSection.h"

#include "ApplicationCore.h"
#include "FrameCompositor/Tasks/SceneRenderTask.h"
#include "FrameCompositor/Tasks/VoxelizeSceneTask.h"
#include "imgui.h"
#include <algorithm>

void VctSection::draw(ApplicationCore& app)
{
	if (ImGui::Checkbox("Show voxels", &showVoxels))
		SceneRenderTask::Get().showVoxels(showVoxels);

	if (showVoxels)
	{
		if (ImGui::InputInt("Show voxels index", &showVoxelsIndex))
		{
			showVoxelsIndex = std::clamp(showVoxelsIndex, 0, 3);
			app.resources.materials.getMaterial("VisualizeVoxelTexture")->SetParameter("VoxelIndex", &showVoxelsIndex, 1);
		}
		if (ImGui::InputInt("Show voxels MIP", &showVoxelsMip))
		{
			showVoxelsMip = std::clamp(showVoxelsMip, 0, 7);
			app.resources.materials.getMaterial("VisualizeVoxelTexture")->SetParameter("VoxelMip", &showVoxelsMip, 1);
		}
	}

	if (ImGui::Button("Regenerate voxels"))
		VoxelizeSceneTask::Get().revoxelize();

	auto& params = VoxelizeSceneTask::Get().params;
	ImGui::SliderFloat("middleConeRatio", &params.middleConeRatioDistance.x, 0.0f, 5.f);
	ImGui::SliderFloat("middleConeDistance", &params.middleConeRatioDistance.y, 0.0f, 5.f);
	ImGui::SliderFloat("sideConeRatio", &params.sideConeRatioDistance.x, 0.0f, 5.f);
	ImGui::SliderFloat("sideConeDistance", &params.sideConeRatioDistance.y, 0.0f, 5.f);
}
