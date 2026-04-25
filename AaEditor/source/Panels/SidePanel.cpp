#include "SidePanel.h"
#include "SceneTreePanel.h"
#include "Editor/DebugState.h"
#include "ApplicationCore.h"
#include "Editor/EditorSelection.h"
#include "imgui.h"
#include "RenderCore/UpscaleTypes.h"
#include "FrameCompositor/Tasks/DebugOverlayTask.h"
#include "FrameCompositor/Tasks/VoxelizeSceneTask.h"
#include "FrameCompositor/Tasks/SceneRenderTask.h"
#include "PhysicsRenderTask.h"
#include <algorithm>

SidePanel::SidePanel(ApplicationCore& a, EditorSelection& sel, DebugState& st, SceneTreePanel& tree)
	: app(a), selection(sel), state(st), sceneTree(tree)
{
}

void SidePanel::draw(const ObjectTransformation& objTransformation)
{
	ImGui::Begin("SidePanel");

	if (ImGui::CollapsingHeader("Rendering"))
	{
		if (ImGui::Button("Reload shaders"))
			state.reloadShaders = true;

		state.wireframeChange = ImGui::Checkbox("Render wireframe", &state.wireframe);

		ImGui::Combo("DLSS", &state.DlssMode, UpscaleModeNames, std::size(UpscaleModeNames));
		ImGui::Combo("FSR", &state.FsrMode, UpscaleModeNames, std::size(UpscaleModeNames));
	}

	if (ImGui::CollapsingHeader("Scene"))
	{
		sceneTree.draw();

		if (!selection.empty())
		{
			ImGui::Text("Selected count %d", selection.size());

			auto& selected = selection.back();
			ImGui::Text("EntityId %#010x", selected.obj.id);
			ImGui::Text("Entity name %s", selected.obj.getName());
			ImGui::Text("Entity pos %f %f %f", objTransformation.position.x, objTransformation.position.y, objTransformation.position.z);
			ImGui::Text("Entity vertex count %d", selected.obj.entity->geometry.vertexCount ? selected.obj.entity->geometry.vertexCount : selected.obj.entity->geometry.indexCount);
		}
	}

	if (ImGui::CollapsingHeader("Defines"))
	{
		auto knownDefines = app.resources.shaders.getKnownDefines();
		auto setDefines = app.resources.shaderDefines.getDefines();

		for (const auto& value : knownDefines)
		{
			bool isActive = setDefines.contains(value);

			if (ImGui::Checkbox(value.c_str(), &isActive))
			{
				app.resources.shaderDefines.setDefine(value, isActive);
			}
		}
	}

	if (ImGui::CollapsingHeader("Terrain"))
	{
		static bool debugLod = false;
		if (ImGui::Checkbox("Debug LOD", &debugLod))
		{
			app.resources.shaderDefines.setDefine("GRID_LOD_DEBUG", debugLod);
		}

		static bool debugIndex = false;
		if (ImGui::Checkbox("Debug grid Index", &debugIndex))
		{
			app.resources.shaderDefines.setDefine("GRID_INDEX_DEBUG", debugIndex);
		}

		static bool debugVegIndex = false;
		if (ImGui::Checkbox("Debug vegetation Index", &debugVegIndex))
		{
			app.resources.shaderDefines.setDefine("BILLBOARD_DEBUG_COLOR", debugVegIndex);
		}
		
		static bool updateGrid = true;
		if (ImGui::Checkbox("Updating LOD", &updateGrid))
		{
			app.sceneMgr.water.enableLodUpdating(updateGrid);
			app.sceneMgr.terrain.updateLod = updateGrid;
		}

		static bool updateWater = true;
		if (ImGui::Checkbox("Update water", &updateWater))
			app.sceneMgr.water.enableWaterUpdating(updateWater);
	}

	if (ImGui::CollapsingHeader("VCT"))
	{
		static bool showVoxels = false;
		if (ImGui::Checkbox("Show voxels", &showVoxels))
			SceneRenderTask::Get().showVoxels(showVoxels);

		if (showVoxels)
		{
			static int showVoxelsIndex = 0;
			if (ImGui::InputInt("Show voxels index", &showVoxelsIndex))
			{
				showVoxelsIndex = std::clamp(showVoxelsIndex, 0, 3);
				app.resources.materials.getMaterial("VisualizeVoxelTexture")->SetParameter("VoxelIndex", &showVoxelsIndex, 1);
			}
			static int showVoxelsMip = 0;
			if (ImGui::InputInt("Show voxels MIP", &showVoxelsMip))
			{
				showVoxelsMip = std::clamp(showVoxelsMip, 0, 7);
				app.resources.materials.getMaterial("VisualizeVoxelTexture")->SetParameter("VoxelMip", &showVoxelsMip, 1);
			}
		}

		if (ImGui::Button("Regenerate voxels"))
			VoxelizeSceneTask::Get().revoxelize();

		auto& state = VoxelizeSceneTask::Get().params;
		ImGui::SliderFloat("middleConeRatio", &state.middleConeRatioDistance.x, 0.0f, 5.f);
		ImGui::SliderFloat("middleConeDistance", &state.middleConeRatioDistance.y, 0.0f, 5.f);
		ImGui::SliderFloat("sideConeRatio", &state.sideConeRatioDistance.x, 0.0f, 5.f);
		ImGui::SliderFloat("sideConeDistance", &state.sideConeRatioDistance.y, 0.0f, 5.f);
	}

	if (ImGui::CollapsingHeader("Physics"))
	{
		const char* physicsDraw[] = {
			"Off",
			"Wireframe",
			"Solid"
		};
		static int physicsDrawIdx = 0;
		if (ImGui::Combo("Draw physics", &physicsDrawIdx, physicsDraw, std::size(physicsDraw)))
			((PhysicsRenderTask*)app.compositor->getTask("RenderPhysics"))->setMode((PhysicsRenderTask::Mode)physicsDrawIdx);

		if (ImGui::Button("Big physics"))
			app.physicsMgr.test();
	}

	if (ImGui::CollapsingHeader("Texture overlay"))
	{
		auto& overlayTask = DebugOverlayTask::Get();
		static bool enabledTexture = false;
		if (ImGui::Checkbox("Enable texture overlay", &enabledTexture))
			overlayTask.enable(enabledTexture);

		if (enabledTexture)
		{
			int next = overlayTask.currentIdx();
			if (ImGui::InputInt("Texture idx", &next))
				overlayTask.changeIdx(next);

			if (next >= 0)
			{
				bool f = overlayTask.isFullscreen();
				if (ImGui::Checkbox("Texture preview fullscreen", &f))
					overlayTask.setFullscreen(f);

				if (auto name = overlayTask.getCurrentIdxName())
					ImGui::Text("Texture: %s", name);
			}
		}
	}

	if (ImGui::CollapsingHeader("Sky"))
	{
		static float latitude = 0.0f;
		static float timeOfDay = 9.0f;
		static int dayOfYear = 172;

		bool change = ImGui::SliderFloat("Time of Day", &timeOfDay, 0.0f, 24.0f);
		change |= ImGui::SliderFloat("Latitude", &latitude, -XM_PIDIV2, XM_PIDIV2);
		change |= ImGui::SliderInt("Day of Year", &dayOfYear, 1, 365);

		if (change)
		{
			float declination = XMConvertToRadians(-23.44f) * std::cos(XM_2PI / 365.0f * (dayOfYear + 10));
			float hourAngle = (timeOfDay - 12.0f) * (XM_PI / 12.0f);

			float sinAlt = std::sin(latitude) * std::sin(declination) +
				std::cos(latitude) * std::cos(declination) * std::cos(hourAngle);
			float altitude = std::asin(std::clamp(sinAlt, -1.0f, 1.0f));

			float cosAz = (std::sin(declination) - std::sin(altitude) * std::sin(latitude)) /
				(std::cos(altitude) * std::cos(latitude));

			float azimuth = std::acos(std::clamp(cosAz, -1.0f, 1.0f));
			if (hourAngle > 0) azimuth = XM_2PI - azimuth;

			Vector3 sunPos;
			sunPos.x = std::cos(altitude) * std::sin(azimuth);
			sunPos.y = std::sin(altitude);
			sunPos.z = std::cos(altitude) * std::cos(azimuth);

			app.lights.directionalLight.direction = -sunPos;
			app.lights.directionalLight.direction.Normalize();

			VoxelizeSceneTask::Get().revoxelize();
		}

		if (ImGui::ColorEdit3("Sun Color", &app.lights.directionalLight.color.x))
		{
			VoxelizeSceneTask::Get().revoxelize();
		}

		static float moonPhase = 0;
		if (ImGui::SliderFloat("Moon phase", &moonPhase, -1, 1))
		{
			auto material = app.resources.materials.getMaterial("Moon");
			material->SetParameter("MoonPhase", &moonPhase, 1);
		}

		ImGui::SliderFloat("Clouds amount", &app.params.sun.CloudsAmount, -1, 1);
		ImGui::SliderFloat("Clouds density", &app.params.sun.CloudsDensity, 0, 1);
		ImGui::SliderFloat("Clouds speed", &app.params.sun.CloudsSpeed, 0, 0.02f);
	}

	ImGui::End();
}
