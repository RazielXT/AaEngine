#include "SelectionTool.h"
#include "Editor/EditorSelection.h"
#include "ApplicationCore.h"
#include "Scene/Camera.h"
#include "ImGuizmo.h"
#include "Scene/Collection/SceneCollection.h"
#include "Resources/Model/GltfLoader.h"
#include "RenderObject/Prefab/PrefabLoader.h"
#include "SceneParser.h"
#include "FrameCompositor/Tasks/VoxelizeSceneTask.h"
#include "Utils/Logger.h"
#include "../data/editor/fonts/IconsFontAwesome7.h"

struct ImageButtonCombo
{
	struct Option
	{
		const char* iconName;
		int value;
	};
	std::vector<Option> options;

	ImageButtonCombo()
	{
		options = {
			{ ICON_FA_ARROW_POINTER " ##Select", ImGuizmo::OPERATION::BOUNDS },
			{ ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT " ##Move", ImGuizmo::OPERATION::TRANSLATE },
			{ ICON_FA_ARROWS_ROTATE " ##Rotate", ImGuizmo::OPERATION::ROTATE },
			{ ICON_FA_PICTURE_IN_PICTURE " ##Scale", ImGuizmo::OPERATION::SCALE }
		};
	}

	bool render(bool primary)
	{
		ImGui::BeginGroup();

		UINT selected = current;
		bool clicked = false;

		for (UINT i = 0; i < options.size(); i++)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 8));

			ImGui::PushStyleColor(ImGuiCol_Button, ItemBg(primary && current == i));

			if (ImGui::Button(options[i].iconName, ImVec2(40, 40)))
			{
				selected = i;
				clicked = true;
			}
			ImGui::PopStyleColor();

			ImGui::PopStyleVar();
		}

		ImGui::EndGroup();

		active = ImGui::IsItemHovered();
		current = selected;

		return clicked;
	}

	ImVec4 ItemBg(bool active)
	{
		auto activeBg = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
		auto inactiveBg = ImGui::GetStyle().Colors[ImGuiCol_FrameBg];

		return active ? activeBg : inactiveBg;
	}

	UINT current{};
	bool active{};
}
transformButtons;

SelectionTool::SelectionTool(ApplicationCore& a, EditorSelection& sel)
	: app(a), selection(sel)
{
}

void SelectionTool::onPick(const EntityPicker::PickInfo& pickInfo, bool ctrlActive, Camera& camera)
{
	if (!assetDrop.empty())
	{
		ObjectTransformation tr;

		if (pickInfo.id)
		{
			tr.position = pickInfo.position;
			tr.position.y += 2;
		}
		else
		{
			tr.position = camera.getPosition() + camera.getCameraDirection() * 50;
		}

		ResourceUploadBatch batch(app.renderSystem.core.device);
		batch.Begin();

		if (assetDrop.ends_with(".gltf"))
		{
			SceneCollection::LoadCtx loadCtx = { batch, app.sceneMgr, app.renderSystem, app.resources, tr };
			auto scene = GltfLoader::load(assetDrop, loadCtx);
			SceneCollection::loadResource(scene, loadCtx);
		}
		else if (assetDrop.ends_with(".scene"))
		{
			SceneParser::load(assetDrop, { batch, app.sceneMgr, app.renderSystem, app.resources, app.physicsMgr, tr });
		}
		else if (assetDrop.ends_with(".prefab"))
		{
			PrefabLoader::load(assetDrop, { batch, app.sceneMgr, app.renderSystem, app.resources, tr });
		}

		auto uploadResourcesFinished = batch.End(app.renderSystem.core.commandQueue);
		uploadResourcesFinished.wait();

		assetDrop.clear();
	}
	else if (auto selectedId = pickInfo.id)
	{
		auto obj = app.sceneMgr.getObject(selectedId);
		Logger::log(std::format("Picked '{}' at ({:.2f}, {:.2f}, {:.2f})", obj.getName(), pickInfo.position.x, pickInfo.position.y, pickInfo.position.z));
		selection.select(selectedId, ctrlActive, app.sceneMgr);
	}
	else if (!ctrlActive)
	{
		selection.clear();
	}
}

bool SelectionTool::drawModeIcon(bool primary)
{
	bool changed = transformButtons.render(primary);

	overlayActive = transformButtons.active;

	return changed; // returns true when any mode button was clicked (tool should become active)
}

void SelectionTool::draw(const ViewportToolContext& ctx)
{
	if (ImGui::IsKeyPressed(ImGuiKey_Delete))
	{
		selection.remove(app.sceneMgr);
	}

	auto m_GizmoType = (ImGuizmo::OPERATION)transformButtons.options[transformButtons.current].value;

	if (gizmoReset || gizmoResetCache)
	{
		if (gizmoReset)
			for (auto& s : selection)
			{
				s.obj.setTransformation(s.origin);
			}

		ImGuizmo::Enable(false); //reset gizmo cache
		gizmoReset = gizmoResetCache = false;
	}
	else
		ImGuizmo::Enable(true);

	if (!selection.empty())
	{
		ImGuizmo::SetOrthographic(false);
		ImGuizmo::SetDrawlist();

		ImGuizmo::SetRect((float)ctx.viewportBounds[0].x, (float)ctx.viewportBounds[0].y, (float)ctx.viewportBounds[1].x - ctx.viewportBounds[0].x, (float)ctx.viewportBounds[1].y - ctx.viewportBounds[0].y);

		float snapValue = 0.5f; // Snap to 0.5m for translation/scale

		// Snap to 45 degrees for rotation
		if (m_GizmoType == ImGuizmo::OPERATION::ROTATE)
			snapValue = 45.0f;

		float snapValues[3] = { snapValue, snapValue, snapValue };

		XMFLOAT4X4 cameraView;
		XMStoreFloat4x4(&cameraView, ctx.camera.getViewMatrix());
		XMFLOAT4X4 cameraProjection;
		XMStoreFloat4x4(&cameraProjection, ctx.camera.getProjectionMatrixNoReverse());

		auto objTransformation = selection.back().obj.getTransformation();
		auto gizmoCenter = objTransformation.position;

		XMFLOAT4X4 transform;

		//centered gizmo
		{
			gizmoCenter = {};
			for (auto& s : selection)
			{
				gizmoCenter += s.obj.getCenterPosition();
			}
			gizmoCenter /= (float)selection.size();

			objTransformation.position = gizmoCenter;
			XMStoreFloat4x4(&transform, objTransformation.createWorldMatrix());
		}

		XMStoreFloat4x4(&transform, objTransformation.createWorldMatrix());

		XMFLOAT4X4 transformDelta{};

		static bool manipulated{};
		bool currentlyManipulated = ImGuizmo::Manipulate(&cameraView._11, &cameraProjection._11,
			m_GizmoType, selection.size() == 1 ? ImGuizmo::LOCAL : ImGuizmo::WORLD, &transform._11,
			&transformDelta._11, nullptr);

		if (gizmoActive)
		{
			XMMATRIX postTransform = XMLoadFloat4x4(&transformDelta);
			XMVECTOR translation, rotation, scale;
			XMMatrixDecompose(&scale, &rotation, &translation, postTransform);

			objTransformation = {};

			if (m_GizmoType == ImGuizmo::OPERATION::TRANSLATE)
				XMStoreFloat3(&objTransformation.position, translation);
			if (m_GizmoType == ImGuizmo::OPERATION::ROTATE)
				XMStoreFloat4(&objTransformation.orientation, rotation);
			if (m_GizmoType == ImGuizmo::OPERATION::SCALE)
				XMStoreFloat3(&objTransformation.scale, scale);

			for (auto& s : selection)
			{
				auto newTransform = s.obj.getTransformation() + objTransformation;

				if (m_GizmoType == ImGuizmo::OPERATION::ROTATE)
				{
					auto offCenter = newTransform.position - gizmoCenter;
					offCenter = objTransformation.orientation * offCenter;

					newTransform.position = gizmoCenter + offCenter;
				}

				if (m_GizmoType == ImGuizmo::OPERATION::SCALE)
				{
					auto offCenter = newTransform.position - gizmoCenter;
					offCenter = objTransformation.scale * offCenter;

					newTransform.position = gizmoCenter + offCenter;
				}

				s.obj.setTransformation(newTransform);
			}
		}

		bool wasActive = gizmoActive;
		gizmoActive = ImGuizmo::IsOver();

		if (!currentlyManipulated && manipulated)
			VoxelizeSceneTask::Get().revoxelize();

		manipulated = currentlyManipulated;
	}
}

void SelectionTool::reset()
{
	assetDrop.clear();
}
