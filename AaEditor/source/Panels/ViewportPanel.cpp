#include "ViewportPanel.h"
#include "Editor/EditorSelection.h"
#include "Editor/ImguiPanelViewport.h"
#include "imgui.h"
#include "ImGuizmo.h"
#include "Editor/EntityPicker.h"
#include "Scene/SceneCollection.h"
#include "Resources/Model/GltfLoader.h"
#include "RenderObject/Prefab/PrefabLoader.h"
#include "SceneParser.h"
#include "FrameCompositor/Tasks/VoxelizeSceneTask.h"
#include "Utils/Logger.h"

void ViewportPanel::DescriptorHeapAllocator::Create(ID3D12Device* device, ID3D12DescriptorHeap* heap)
{
	IM_ASSERT(Heap == nullptr && FreeIndices.empty());
	Heap = heap;
	D3D12_DESCRIPTOR_HEAP_DESC desc = heap->GetDesc();
	HeapType = desc.Type;
	HeapStartCpu = Heap->GetCPUDescriptorHandleForHeapStart();
	HeapStartGpu = Heap->GetGPUDescriptorHandleForHeapStart();
	HeapHandleIncrement = device->GetDescriptorHandleIncrementSize(HeapType);
	FreeIndices.reserve((int)desc.NumDescriptors);
	for (int n = desc.NumDescriptors; n > 0; n--)
		FreeIndices.push_back(n);
}

void ViewportPanel::DescriptorHeapAllocator::Destroy()
{
	Heap = nullptr;
	FreeIndices.clear();
}

void ViewportPanel::DescriptorHeapAllocator::Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle)
{
	IM_ASSERT(FreeIndices.Size > 0);
	int idx = FreeIndices.back();
	FreeIndices.pop_back();
	out_cpu_desc_handle->ptr = HeapStartCpu.ptr + (idx * HeapHandleIncrement);
	out_gpu_desc_handle->ptr = HeapStartGpu.ptr + (idx * HeapHandleIncrement);
}

void ViewportPanel::DescriptorHeapAllocator::Free(D3D12_CPU_DESCRIPTOR_HANDLE out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE out_gpu_desc_handle)
{
	int cpu_idx = (int)((out_cpu_desc_handle.ptr - HeapStartCpu.ptr) / HeapHandleIncrement);
	int gpu_idx = (int)((out_gpu_desc_handle.ptr - HeapStartGpu.ptr) / HeapHandleIncrement);
	IM_ASSERT(cpu_idx == gpu_idx);
	FreeIndices.push_back(cpu_idx);
}

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
			{ "Select", ImGuizmo::OPERATION::BOUNDS },
			{ "Move", ImGuizmo::OPERATION::TRANSLATE },
			{ "Rotate", ImGuizmo::OPERATION::ROTATE },
			{ "Scale", ImGuizmo::OPERATION::SCALE }
		};
	}

	bool render(std::map<std::string, FileTexture*>& icons)
	{
		ImGui::BeginGroup();

		UINT selected = current;

		for (UINT i = 0; i < options.size(); i++)
		{
			if (ImGui::ImageButton(options[i].iconName, (ImTextureID)icons[options[i].iconName]->srvHandle.ptr, ImVec2(32, 32), ImVec2(0, 0), ImVec2(1, 1), ItemBg(i)))
			{
				selected = i;
			}
		}

		ImGui::EndGroup();

		active = ImGui::IsItemHovered();
		bool changed = selected != current;
		current = selected;

		return changed;
	}

	ImVec4 ItemBg(int idx)
	{
		auto activeBg = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
		auto inactiveBg = ImVec4(0, 0, 0, 0);

		return current == idx ? activeBg : inactiveBg;
	}

	UINT current{};
	bool active{};
}
transformButtons;

ViewportPanel::ViewportPanel(ApplicationCore& a, EditorSelection& sel, ImguiPanelViewport& rpv, DescriptorHeapAllocator& alloc, RenderCore& r)
	: app(a), selection(sel), renderPanelViewport(rpv), srvDescHeapAlloc(alloc), renderer(r)
{
}

void ViewportPanel::scheduleViewportPick()
{
	auto [mx, my] = ImGui::GetMousePos();
	mx -= viewportBounds[0].x;
	my -= viewportBounds[0].y;
	XMUINT2 viewportSize = { viewportBounds[1].x - viewportBounds[0].x, viewportBounds[1].y - viewportBounds[0].y };
	my = viewportSize.y - my;
	auto mouseX = (UINT)mx;
	auto mouseY = (UINT)my;

	if (mouseX >= 0 && mouseY >= 0 && mouseX < (int)viewportSize.x && mouseY < (int)viewportSize.y)
	{
		EntityPicker::Get().scheduleNextPick({ mouseX, viewportSize.y - mouseY });
		scenePickScheduled = true;
	}
}

void ViewportPanel::resetOutputDescriptor()
{
	if (renderOutputHandleGpu.ptr)
	{
		srvDescHeapAlloc.Free(renderOutputHandleCpu, renderOutputHandleGpu);

		renderOutputHandleGpu = {};
		renderOutputHandleCpu = {};
	}
}

void ViewportPanel::reset()
{
	scenePickScheduled = false;
}

void ViewportPanel::ensureOutputDescriptor()
{
	if (!renderOutputHandleGpu.ptr)
	{
		auto renderOutput = app.compositor->getTexture("Output");
		srvDescHeapAlloc.Alloc(&renderOutputHandleCpu, &renderOutputHandleGpu);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Format = renderOutput->texture->GetDesc().Format;

		renderer.device->CreateShaderResourceView(renderOutput->texture.Get(), &srvDesc, renderOutputHandleCpu);
	}
}

void ViewportPanel::loadIcons()
{
	std::string path = "../data/editor/icons/";

	const std::vector<std::pair<std::string, std::string>> iconList =
	{
		{ "Select", "select.png" },
		{ "Move", "move.png" },
		{ "Rotate", "rotate.png" },
		{ "Scale", "scale.png" },
	};

	auto& loader = app.resources.textures;

	ResourceUploadBatch batch(renderer.device);
	batch.Begin();

	for (auto&[name, file] : iconList)
	{
		icons[name] = loader.loadFile(*renderer.device, batch, path + file);
	}

	auto uploadResourcesFinished = batch.End(renderer.commandQueue);
	uploadResourcesFinished.wait();
}

void ViewportPanel::initializeIconViews()
{
	for (auto&[name,icon] : icons)
	{
		srvDescHeapAlloc.Alloc(&icon->handle, &icon->srvHandle);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Format = icon->texture->GetDesc().Format;

		renderer.device->CreateShaderResourceView(icon->texture.Get(), &srvDesc, icon->handle);
	}
}

ObjectTransformation ViewportPanel::draw(Camera& camera)
{
	ObjectTransformation objTransformation{};

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
	ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoDecoration);

	auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
	auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
	auto viewportOffset = ImGui::GetWindowPos();
	viewportBounds[0] = { UINT(viewportMinRegion.x + viewportOffset.x), UINT(viewportMinRegion.y + viewportOffset.y) };
	viewportBounds[1] = { UINT(viewportMaxRegion.x + viewportOffset.x), UINT(viewportMaxRegion.y + viewportOffset.y) };

	auto viewportContent = ImGui::GetContentRegionAvail();
	viewportPanelSize = { (UINT)viewportContent.x, (UINT)viewportContent.y };

	hovered = ImGui::IsWindowHovered();
	active = hovered || ImGui::IsWindowFocused();

	ensureOutputDescriptor();

	ImGui::Image(ImTextureID(renderOutputHandleGpu.ptr), ImVec2{ (float)renderPanelViewport.getWidth(), (float)renderPanelViewport.getHeight() });

	bool ctrlActive = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl);

	if (scenePickScheduled)
	{
		auto& pickInfo = EntityPicker::Get().getLastPick();

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
				SceneCollection::loadScene(scene, loadCtx);
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

		scenePickScheduled = false;
		assetDrop.clear();
	}

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_MODEL"))
		{
			assetDrop = (const char*)payload->Data;
			scheduleViewportPick();
		}

		ImGui::EndDragDropTarget();
	}

	if (ImGui::IsKeyPressed(ImGuiKey_Delete))
	{
		selection.remove(app.sceneMgr);
	}

	static auto m_GizmoType = ImGuizmo::OPERATION::BOUNDS;

	// on top of viewport
	ImGui::SetCursorPosY(ImGui::GetCursorStartPos().y + 5);
	ImGui::SetCursorPosX(ImGui::GetCursorStartPos().x + 5);

	if (transformButtons.render(icons))
		m_GizmoType = (ImGuizmo::OPERATION)transformButtons.options[transformButtons.current].value;

	overlayActive = transformButtons.active;

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

		ImGuizmo::SetRect((float)viewportBounds[0].x, (float)viewportBounds[0].y, (float)viewportBounds[1].x - viewportBounds[0].x, (float)viewportBounds[1].y - viewportBounds[0].y);

		float snapValue = 0.5f; // Snap to 0.5m for translation/scale

		// Snap to 45 degrees for rotation
		if (m_GizmoType == ImGuizmo::OPERATION::ROTATE)
			snapValue = 45.0f;

		float snapValues[3] = { snapValue, snapValue, snapValue };

		XMFLOAT4X4 cameraView;
		XMStoreFloat4x4(&cameraView, camera.getViewMatrix());
		XMFLOAT4X4 cameraProjection;
		XMStoreFloat4x4(&cameraProjection, camera.getProjectionMatrixNoReverse());

		objTransformation = selection.back().obj.getTransformation();
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

	ImGui::End();
	ImGui::PopStyleVar();

	return objTransformation;
}
