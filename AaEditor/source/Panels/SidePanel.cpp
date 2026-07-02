#include "SidePanel.h"
#include "SceneTreePanel.h"
#include "Editor/DebugState.h"
#include "Viewport/ViewportPanel.h"
#include "Viewport/SplineRoadTool.h"
#include "WaterPaintTool.h"
#include "ApplicationCore.h"
#include "Editor/EditorSelection.h"
#include "imgui.h"
#include "RenderCore/UpscaleTypes.h"
#include "Physics/Render/PhysicsRenderTask.h"
#include "Resources/Shader/ShaderResources.h"
#include "FrameCompositor/Tasks/SceneRenderTask.h"

static WaterPaintTool* waterPaintTool;

SidePanel::SidePanel(ApplicationCore& a, EditorSelection& sel, DebugState& st, SceneTreePanel& tree, ViewportPanel& vp)
	: app(a), selection(sel), state(st), sceneTree(tree), viewportPanel(vp)
{
	waterPaintTool = new WaterPaintTool(a, viewportPanel);
	splineRoadTool = std::make_unique<SplineRoadTool>(a, viewportPanel);
}

SidePanel::~SidePanel() = default;

void SidePanel::draw()
{
	ImGui::Begin("SidePanel");

	if (ImGui::CollapsingHeader("Rendering"), ImGuiTreeNodeFlags_DefaultOpen)
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
			ImGui::Text("Entity name %s", selected.obj.getName().c_str());
			auto objTransformation = selected.obj.getTransformation();
			ImGui::Text("Entity pos %f %f %f", objTransformation.position.x, objTransformation.position.y, objTransformation.position.z);
			ImGui::Text("Entity vertex count %d", selected.obj.entity->geometry.vertexCount ? selected.obj.entity->geometry.vertexCount : selected.obj.entity->geometry.indexCount);
		}
	}

	if (ImGui::CollapsingHeader("Material") && !selection.empty())
	{
		auto& selected = selection.back();
		auto* entity = selected.obj.entity;
		auto* material = entity->material;

		if (material)
		{
			auto showParamEdit = [entity](const ResourcesInfo::ParamInfo& param)
				{
					if (param.type == D3D_SVT_FLOAT)
					{
						UINT floatCount = param.size / sizeof(float);
						float values[4]{};
						entity->GetMaterialParam(param.name, values);

						bool changed = false;
						if (floatCount == 1)
							changed = ImGui::DragFloat(param.name.c_str(), values, 0.01f);
						else if (floatCount == 2)
							changed = ImGui::DragFloat2(param.name.c_str(), values, 0.01f);
						else if (floatCount == 3 && param.name.size() >= 5 && param.name.compare(param.name.size() - 5, 5, "Color") == 0)
							changed = ImGui::ColorEdit3(param.name.c_str(), values);
						else if (floatCount == 3)
							changed = ImGui::DragFloat3(param.name.c_str(), values, 0.01f);
						else if (floatCount == 4)
							changed = ImGui::DragFloat4(param.name.c_str(), values, 0.01f);

						if (changed)
							entity->Material().setParam(param.name, values, param.size);
					}
					else if (param.type == D3D_SVT_UINT)
					{
						UINT uintCount = param.size / sizeof(UINT);
						UINT values[4]{};
						entity->GetMaterialParam(param.name, values);

						bool changed = false;
						if (uintCount == 1)
							changed = ImGui::DragInt(param.name.c_str(), (int*)values);
						else if (uintCount == 2)
							changed = ImGui::DragInt2(param.name.c_str(), (int*)values);
						else if (uintCount == 3)
							changed = ImGui::DragInt3(param.name.c_str(), (int*)values);
						else if (uintCount == 4)
							changed = ImGui::DragInt4(param.name.c_str(), (int*)values);

						if (changed)
							entity->Material().setParam(param.name, values, param.size);
					}
				};

			auto& params = material->GetParamsBuffer();

			std::unordered_set<std::string> shownParams;

			for (auto& param : params)
			{
				shownParams.insert(param.name);
				showParamEdit(param);
			}

// 			if (entity->materialOverride)
// 				for (auto& edit : entity->materialOverride->params)
// 				{
// 					if (shownParams.find(edit.name) == shownParams.end())
// 					{
// 						shownParams.insert(edit.name);
// 						showParamEdit(ResourcesInfo::ParamInfo{ edit.name, "", edit.sizeBytes});
// 					}
// 				}

			for (auto& [name, values] : material->GetBase()->GetDefaultParams())
			{
				if (shownParams.find(name) == shownParams.end())
				{
					shownParams.insert(name);
					showParamEdit(ResourcesInfo::ParamInfo{ name, "", (UINT)(values.size() * sizeof(float)), {}, D3D_SVT_FLOAT });
				}
			}
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
			app.resources.shaderDefines.setDefine("CHUNK_DEBUG_COLOR", debugVegIndex);
		}
		
		static bool updateGrid = true;
		if (ImGui::Checkbox("Updating LOD", &updateGrid))
		{
			app.renderWorld.water.enableLodUpdating(updateGrid);
			app.renderWorld.terrain.updateLod = updateGrid;
		}

		static bool updateVegetationCulling = true;
		if (ImGui::Checkbox("Vegetation culling", &updateVegetationCulling))
		{
			app.renderWorld.vegetation.enableUpdating(updateVegetationCulling);
			app.renderWorld.grass.enableUpdating(updateVegetationCulling);
		}

		static bool updateWater = true;
		if (ImGui::Checkbox("Update water", &updateWater))
			app.renderWorld.water.enableWaterUpdating(updateWater);

		bool waterPaintActive = viewportPanel.getActiveTool() == waterPaintTool;
		if (ImGui::Checkbox("Water paint mode", &waterPaintActive))
		{
			viewportPanel.setActiveTool(waterPaintActive ? waterPaintTool : nullptr);
		}

		if (waterPaintActive)
		{
			const char* modeLabel = waterPaintTool->getMode() == WaterPaintTool::Mode::Add ? "Add" : "Remove";
			ImGui::Text("  Mode: %s", modeLabel);
		}

		const char* viewId[] = {
			"Default",
			"ShadowCascade0",
			"ShadowCascade1",
			"ShadowCascade2",
			"ShadowCascade3",
		};
		const RenderViewId viewIdValues[] = {
			RenderViewId_Default,
			RenderViewId_ShadowCascade0,
			RenderViewId_ShadowCascade1,
			RenderViewId_ShadowCascade2,
			RenderViewId_ShadowCascade3,
		};
		static int viewIdIdx = 0;
		if (ImGui::Combo("Draw view", &viewIdIdx, viewId, std::size(viewId)))
		{
			((SceneRenderTask*)app.compositor->getTask("SceneRender"))->setViewOverride(viewIdValues[viewIdIdx]);
		}
	}

	if (ImGui::CollapsingHeader("Spline roads"))
	{
		splineRoadSection.draw(*splineRoadTool, viewportPanel);
	}

	if (ImGui::CollapsingHeader("VGI"))
	{
		vgiSection.draw(app, state);
	}

	if (ImGui::CollapsingHeader("Physics"))
	{
		static bool updatePhysics = true;
		if (ImGui::Checkbox("Update physics", &updatePhysics))
			app.physicsMgr.enableUpdate(updatePhysics);

		const char* physicsDraw[] = {
			"Off",
			"Wireframe",
			"Solid"
		};
		static int physicsDrawIdx = 0;
		if (ImGui::Combo("Draw physics", &physicsDrawIdx, physicsDraw, std::size(physicsDraw)))
		{
			state.wireframePhysicsChange = physicsDrawIdx;
		}

		if (ImGui::Button("Big physics"))
			app.physicsMgr.test();
	}

	if (ImGui::CollapsingHeader("Texture overlay"))
	{
		textureOverlaySection.draw();
	}

	if (ImGui::CollapsingHeader("Sky"))
	{
		skySection.draw(app);
	}

	if (ImGui::CollapsingHeader("Interaction"))
	{
		const char* modes[] = { "Editor", "Motorbike", "Walking (1st Person)", "Walking (3rd Person)" };
		int currentMode = (int)state.interactionMode;

		if (ImGui::Combo("Mode", &currentMode, modes, std::size(modes)))
		{
			state.interactionMode = (InteractionMode)currentMode;
			state.interactionModeChanged = true;
		}

		if (state.interactionMode == InteractionMode::WalkingFirstPerson || state.interactionMode == InteractionMode::WalkingThirdPerson)
		{
			ImGui::Checkbox("Show player body", &state.showPlayerBody);
		}
	}

	ImGui::End();
}
