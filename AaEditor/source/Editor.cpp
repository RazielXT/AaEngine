#include "Editor.h"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"
#include "../data/editor/icons/IconsFontAwesome7.h"
#include "ApplicationCore.h"
#include "Editor/EntityPicker.h"
#include "ImGuizmo.h"
#include "RenderCore/UpscaleTypes.h"
#include "FrameCompositor/Tasks/DebugOverlayTask.h"
#include "FrameCompositor/Tasks/VoxelizeSceneTask.h"
#include "FrameCompositor/Tasks/SceneRenderTask.h"
#include "PhysicsRenderTask.h"
#include "Utils/SystemUtils.h"
#include "App/Directories.h"
#include "Scene/SceneCollection.h"
#include "Resources/Model/GltfLoader.h"
#include "RenderObject/Prefab/PrefabLoader.h"
#include "SceneParser.h"
#include "NodeGraph.h"
#include <format>
#include <algorithm>
#include <filesystem>

// Store frame times in a buffer
float frameTimes[200] = { 0.0f };
int frameChartIndex = 0;
float maxFrameTime = 1 / 30.f;

void UpdateFrameTimeBuffer()
{
	float frameTime = ImGui::GetIO().DeltaTime;

	maxFrameTime = max(frameTime, maxFrameTime);
	maxFrameTime = std::clamp(maxFrameTime, 1 / 30.f, 1 / 10.f);

	frameTimes[frameChartIndex] = frameTime;
	frameChartIndex = (frameChartIndex + 1) % std::size(frameTimes);
}

static ID3D12DescriptorHeap* g_pd3dSrvDescHeap = nullptr;
static CommandsData commands;

struct ExampleDescriptorHeapAllocator
{
	ID3D12DescriptorHeap* Heap = nullptr;
	D3D12_DESCRIPTOR_HEAP_TYPE  HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
	D3D12_CPU_DESCRIPTOR_HANDLE HeapStartCpu;
	D3D12_GPU_DESCRIPTOR_HANDLE HeapStartGpu;
	UINT                        HeapHandleIncrement;
	ImVector<int>               FreeIndices;

	void Create(ID3D12Device* device, ID3D12DescriptorHeap* heap)
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
	void Destroy()
	{
		Heap = nullptr;
		FreeIndices.clear();
	}
	void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle)
	{
		IM_ASSERT(FreeIndices.Size > 0);
		int idx = FreeIndices.back();
		FreeIndices.pop_back();
		out_cpu_desc_handle->ptr = HeapStartCpu.ptr + (idx * HeapHandleIncrement);
		out_gpu_desc_handle->ptr = HeapStartGpu.ptr + (idx * HeapHandleIncrement);
	}
	void Free(D3D12_CPU_DESCRIPTOR_HANDLE out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE out_gpu_desc_handle)
	{
		int cpu_idx = (int)((out_cpu_desc_handle.ptr - HeapStartCpu.ptr) / HeapHandleIncrement);
		int gpu_idx = (int)((out_gpu_desc_handle.ptr - HeapStartGpu.ptr) / HeapHandleIncrement);
		IM_ASSERT(cpu_idx == gpu_idx);
		FreeIndices.push_back(cpu_idx);
	}
};

ExampleDescriptorHeapAllocator g_pd3dSrvDescHeapAlloc;

Editor::Editor(ApplicationCore& a, ImguiPanelViewport& v) : app(a), renderer(a.renderSystem.core), renderPanelViewport(v)
{
#ifdef _DEBUG
	state.limitFrameRate = true;
#endif

	lookupAssets();
}

void Editor::initializeUi(const TargetWindow& v)
{
	renderPanelViewport.set(v.getHwnd(), 640, 480);
	lastViewportPanelSize = viewportPanelSize = { renderPanelViewport.getWidth(), renderPanelViewport.getHeight() };

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImNodes::CreateContext();

	auto& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	auto scaling = GetDpiForWindow(v.getHwnd()) * 0.01f;
	io.Fonts->AddFontFromFileTTF(R"(c:\Windows\Fonts\segoeui.ttf)", 16.0f * scaling);

	ImFontConfig config{};
	config.MergeMode = true;
	config.PixelSnapH = true;
	const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
	io.Fonts->AddFontFromFileTTF(R"(../data/editor/icons/Font Awesome 7 Free-Solid-900.otf)", 16.0f * scaling, &config, icon_ranges);

	//ImGuiStyle& style = ImGui::GetStyle();

	loadIcons();
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = 64 + (UINT)icons.size();
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		if (renderer.device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dSrvDescHeap)) != S_OK)
			return;

		g_pd3dSrvDescHeap->SetName(L"ImguiDescriptors");
		g_pd3dSrvDescHeapAlloc.Create(renderer.device, g_pd3dSrvDescHeap);
	}
	initializeIconViews();

	ImGui_ImplWin32_Init(v.getHwnd());
}

void Editor::initializeRenderer(DXGI_FORMAT format)
{
	ImGui_ImplDX12_InitInfo init_info = {};
	init_info.Device = renderer.device;
	init_info.CommandQueue = renderer.commandQueue;
	init_info.NumFramesInFlight = FrameCount;
	init_info.RTVFormat = format;
	init_info.DSVFormat = DXGI_FORMAT_UNKNOWN;
	init_info.SrvDescriptorHeap = g_pd3dSrvDescHeap;
	init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle) { return g_pd3dSrvDescHeapAlloc.Alloc(out_cpu_handle, out_gpu_handle); };
	init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) { return g_pd3dSrvDescHeapAlloc.Free(cpu_handle, gpu_handle); };
	ImGui_ImplDX12_Init(&init_info);

	commands = renderer.CreateCommandList(L"Editor", PixColor::Editor);
}

void Editor::updateViewportSize()
{
	static bool lastSizeChanged = false;

	if ((viewportPanelSize.x != lastViewportPanelSize.x || viewportPanelSize.y != lastViewportPanelSize.y) &&
		(viewportPanelSize.x > 0 && viewportPanelSize.y > 0))
	{
		lastSizeChanged = true;
	}
	// only call after resizing stopped
	else if (lastSizeChanged)
	{
		lastSizeChanged = false;
		resetViewportOutput();
	}

	lastViewportPanelSize = viewportPanelSize;
}

void Editor::resetViewportOutput()
{
	renderPanelViewport.set(renderPanelViewport.getHwnd(), viewportPanelSize.x, viewportPanelSize.y);

	for (auto l : renderPanelViewport.listeners)
		l->onViewportResize(viewportPanelSize.x, viewportPanelSize.y);

	if (renderOutputTextureImguiHandleGpu.ptr)
	{
		g_pd3dSrvDescHeapAlloc.Free(renderOutputTextureImguiHandleCpu, renderOutputTextureImguiHandleGpu);

		renderOutputTextureImguiHandleGpu = {};
		renderOutputTextureImguiHandleCpu = {};
	}
}

void Editor::render(Camera& camera)
{
	auto marker = renderer.StartCommandList(commands, g_pd3dSrvDescHeap);
	auto& target = renderer.backbuffer[renderer.frameIndex];
	target.PrepareAsRenderTarget(commands.commandList, D3D12_RESOURCE_STATE_PRESENT, true);

	{
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();

		ImGui::DockSpaceOverViewport();
		prepareElements(camera);

		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commands.commandList);
	}

	target.PrepareToPresent(commands.commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	marker.close();
	renderer.ExecuteCommandList(commands);
}

void Editor::deinit()
{
	renderer.WaitForAllFrames();

	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();

	ImNodes::DestroyContext();
	ImGui::DestroyContext();

	g_pd3dSrvDescHeapAlloc.Destroy();
	g_pd3dSrvDescHeap->Release();

	commands.deinit();
}

XMUINT2 lastScheduled;
bool gizmoActive = false;
bool gizmoReset = false;
bool gizmoResetCache = false;
bool overlayActive = false;

bool Editor::onClick(MouseButton button)
{
	if (button == MouseButton::Right)
	{
		if (gizmoActive)
		{
			gizmoReset = true;
			return true;
		}
	}

	if (gizmoActive || overlayActive)
		return false;

	if (button == MouseButton::Left)
	{
		scheduleViewportPick();

		return true;
	}

	return false;
}

bool Editor::keyPressed(int key)
{
	if (key == VK_CONTROL)
		ctrlActive = true;

	return false;
}

bool Editor::keyReleased(int key)
{
	if (key == VK_CONTROL)
		ctrlActive = false;

	return false;
}

void Editor::reset()
{
	selection.clear();
	scenePickScheduled = false;
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

void Editor::scheduleViewportPick()
{
	auto [mx, my] = ImGui::GetMousePos();
	mx -= m_ViewportBounds[0].x;
	my -= m_ViewportBounds[0].y;
	XMUINT2 viewportSize = { m_ViewportBounds[1].x - m_ViewportBounds[0].x, m_ViewportBounds[1].y - m_ViewportBounds[0].y };
	my = viewportSize.y - my;
	auto mouseX = (UINT)mx;
	auto mouseY = (UINT)my;

	if (mouseX >= 0 && mouseY >= 0 && mouseX < (int)viewportSize.x && mouseY < (int)viewportSize.y)
	{
		EntityPicker::Get().scheduleNextPick({ mouseX, viewportSize.y - mouseY });
		lastScheduled = { mouseX, viewportSize.y - mouseY };
		scenePickScheduled = true;
	}
}

void Editor::prepareElements(Camera& camera)
{
// 	if (ImGui::BeginMenuBar())
// 	{
// 		if (ImGui::BeginMenu("File"))
// 		{
// 			if (ImGui::MenuItem("Open Project...", "Ctrl+O"))
// 			{
// 			}//OpenProject();
// 
// 			ImGui::Separator();
// 
// 			if (ImGui::MenuItem("New Scene", "Ctrl+N"))
// 			{
// 			}//NewScene();
// 
// 			if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
// 			{
// 			}//SaveScene();
// 
// 			if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S"))
// 			{
// 			}//SaveSceneAs();
// 
// 			ImGui::EndMenu();
// 		}
// 
// 		if (ImGui::BeginMenu("Script"))
// 		{
// 			ImGui::EndMenu();
// 		}
// 
// 		ImGui::EndMenuBar();
// 	}

	ImGui::Begin("Debug");

	ImGui::Text("%.1f FPS (%.2f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
	ImGui::SameLine();
	ImGui::Checkbox("Limit framerate", &state.limitFrameRate);

// 	UpdateFrameTimeBuffer();
// 	ImGui::PlotLines("Frame Times", frameTimes, std::size(frameTimes), frameChartIndex, NULL, 0.0f, maxFrameTime);

	ImGui::Text("VRAM used %uMB", GetGpuMemoryUsage());
	ImGui::Text("RAM used %uMB", GetSystemMemoryUsage());

	ImGui::End();

	ImGui::Begin("Assets");
	DrawAssetBrowser(assets);
	ImGui::End();

	ImGui::Begin("Terrain");
	DrawTerrainShaderGraph();
	ImGui::End();

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
	ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoDecoration);

	auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
	auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
	auto viewportOffset = ImGui::GetWindowPos();
	m_ViewportBounds[0] = { UINT(viewportMinRegion.x + viewportOffset.x), UINT(viewportMinRegion.y + viewportOffset.y) };
	m_ViewportBounds[1] = { UINT(viewportMaxRegion.x + viewportOffset.x), UINT(viewportMaxRegion.y + viewportOffset.y) };

	auto viewportContent = ImGui::GetContentRegionAvail();
	viewportPanelSize = { (UINT)viewportContent.x, (UINT)viewportContent.y };

	rendererHovered = ImGui::IsWindowHovered();
	rendererActive = rendererHovered || ImGui::IsWindowFocused();
	bool redone = false;
	if (!renderOutputTextureImguiHandleGpu.ptr)
	{
		redone = true;
		auto renderOutput = app.compositor->getTexture("Output");
		g_pd3dSrvDescHeapAlloc.Alloc(&renderOutputTextureImguiHandleCpu, &renderOutputTextureImguiHandleGpu);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Format = renderOutput->texture->GetDesc().Format;

		renderer.device->CreateShaderResourceView(renderOutput->texture.Get(), &srvDesc, renderOutputTextureImguiHandleCpu);
	}

	ImGui::Image(ImTextureID(renderOutputTextureImguiHandleGpu.ptr), ImVec2{ (float)renderPanelViewport.getWidth(), (float)renderPanelViewport.getHeight() });

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

// 				if (addTreeNormals)
// 					tr.orientation *= Quaternion::FromToRotation(Vector3::UnitY, pickInfo.normal);
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
			selectItem(selectedId, ctrlActive);
		}
		else if (!ctrlActive)
		{
			clearSelection();
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
		deleteSelectedItem();
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

	ObjectTransformation objTransformation{};

	if (!selection.empty())
	{
		ImGuizmo::SetOrthographic(false);
		ImGuizmo::SetDrawlist();

		ImGuizmo::SetRect((float)m_ViewportBounds[0].x, (float)m_ViewportBounds[0].y, (float)m_ViewportBounds[1].x - m_ViewportBounds[0].x, (float)m_ViewportBounds[1].y - m_ViewportBounds[0].y);

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

		//ImGui::Text("ImGuizmo manipulated %d", currentlyManipulated);

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
		//ImGui::Text("ImGuizmo gizmoActive %d", gizmoActive);

		if (!currentlyManipulated && manipulated)
			VoxelizeSceneTask::Get().revoxelize();

		manipulated = currentlyManipulated;
	}

	ImGui::End();
	ImGui::PopStyleVar();

	ImGui::Begin("Debug2");

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
		DrawSceneTree();

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

		static bool updateGrid = true;
		if (ImGui::Checkbox("Updating LOD", &updateGrid))
		{
			app.sceneMgr.water.enableLodUpdating(updateGrid);
			app.sceneMgr.newTerrain.updateLod = updateGrid;
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
// 		static float sunYaw = std::atan2(app.lights.directionalLight.direction.x, app.lights.directionalLight.direction.z);
// 		static float sunPitch = std::asin(app.lights.directionalLight.direction.y);
// 
// 		bool change = ImGui::SliderFloat("Sun Yaw", &sunYaw, -XM_PI, XM_PI);
// 		change |= ImGui::SliderFloat("Sun Pitch", &sunPitch, -XM_PI, 0);
// 
// 		if (change)
// 		{
// 			Vector3& direction = app.lights.directionalLight.direction;
// 			direction.x = std::cos(sunPitch) * std::sin(sunYaw);
// 			direction.y = std::sin(sunPitch);
// 			direction.z = std::cos(sunPitch) * std::cos(sunYaw);
// 			direction.Normalize();
// 
// 			VoxelizeSceneTask::Get().revoxelize();
// 		}

		static float latitude = 0.0f;    // ~45 degrees North
		static float timeOfDay = 9.0f;   // 0.0 to 24.0
		static int dayOfYear = 172;       // Summer Solstice (June 21)

		bool change = ImGui::SliderFloat("Time of Day", &timeOfDay, 0.0f, 24.0f);
		change |= ImGui::SliderFloat("Latitude", &latitude, -XM_PIDIV2, XM_PIDIV2);
		change |= ImGui::SliderInt("Day of Year", &dayOfYear, 1, 365);

		if (change)
		{
			// 1. Seasonal Declination
			float declination = XMConvertToRadians(-23.44f) * std::cos(XM_2PI / 365.0f * (dayOfYear + 10));

			// 2. Hour Angle (H) 
			// At 12.0 (Noon), H = 0. At 0.0 (Midnight), H = -PI.
			float hourAngle = (timeOfDay - 12.0f) * (XM_PI / 12.0f);

			// 3. Solar Altitude (Angle above horizon)
			float sinAlt = std::sin(latitude) * std::sin(declination) +
				std::cos(latitude) * std::cos(declination) * std::cos(hourAngle);
			float altitude = std::asin(std::clamp(sinAlt, -1.0f, 1.0f));

			// 4. Solar Azimuth (Compass direction)
			float cosAz = (std::sin(declination) - std::sin(altitude) * std::sin(latitude)) /
				(std::cos(altitude) * std::cos(latitude));

			float azimuth = std::acos(std::clamp(cosAz, -1.0f, 1.0f));
			if (hourAngle > 0) azimuth = XM_2PI - azimuth;

			// 5. Final Light Direction (The vector the light travels)
			// We negate the position to get the direction: Sun is at (x, y, z), light shines toward (-x, -y, -z)
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

void Editor::selectItem(ObjectId selectedId, bool multi)
{
	bool add = true;

	if (!multi)
	{
		selection.clear();
	}
	else
	{
		for (auto it = selection.begin(); it != selection.end(); it++)
		{
			if (it->obj.id == selectedId)
			{
				selection.erase(it);
				add = false;
				break;
			}
		}
	}

	if (add)
	{
		if (auto obj = app.sceneMgr.getObject(selectedId))
		{
			selection.emplace_back(obj.getTransformation(), obj);
		}
	}

	refreshSelectionId();
}

void Editor::deleteSelectedItem()
{
	for (auto& s : selection)
	{
		app.sceneMgr.removeEntity(s.obj.entity);
	}

	clearSelection();
}

void Editor::deleteItem(SceneGraphNode& node)
{
	for (auto& child : node.children)
	{
		app.sceneMgr.removeEntity(child.id);
	}

	clearSelection();
}

void Editor::refreshSelectionId()
{
	selectionIds.clear();
	for (auto& s : selection)
	{
		selectionIds.push_back(s.obj.id);
	}

	EntityPicker::Get().active = selectionIds;
}

void Editor::clearSelection()
{
	selection.clear();
	refreshSelectionId();
}

void Editor::loadIcons()
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

void Editor::initializeIconViews()
{
	for (auto&[name,icon] : icons)
	{
		g_pd3dSrvDescHeapAlloc.Alloc(&icon->handle, &icon->srvHandle);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Format = icon->texture->GetDesc().Format;

		renderer.device->CreateShaderResourceView(icon->texture.Get(), &srvDesc, icon->handle);
	}
}

static bool NodeMatchesFilter(SceneGraphNode& node, ImGuiTextFilter& filter)
{
	// Does this node match?
	if (filter.PassFilter(node.name))
		return true;

	// Do any children match?
	for (auto& child : node.children)
		if (NodeMatchesFilter(child, filter))
			return true;

	return false;
}

void Editor::DrawSceneTreeNode(SceneGraphNode& node, ImGuiTextFilter& filter, ObjectId& selectedObjectId)
{
	if (filter.IsActive() && !NodeMatchesFilter(node, filter))
		return;

	ImGuiTreeNodeFlags flags =
		ImGuiTreeNodeFlags_OpenOnArrow |
		ImGuiTreeNodeFlags_OpenOnDoubleClick |
		(node.children.empty() ? ImGuiTreeNodeFlags_Leaf : 0);

	if (filter.IsActive())
		ImGui::SetNextItemOpen(true);

	// Highlight if selected
	for (auto& id : selectionIds)
	{
		if (node.id == id)
			flags |= ImGuiTreeNodeFlags_Selected;
	}

	const char* icon = !node.children.empty() ? ICON_FA_FOLDER : ICON_FA_CUBE;

	bool open = ImGui::TreeNodeEx(
		(void*)(intptr_t)node.id.value,
		flags,
		"%s %s", icon, node.name
	);

	// Handle selection
	if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
		selectedObjectId = node.id;
	if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && !(flags & ImGuiTreeNodeFlags_Selected)) // only select with right click, dont unselect
		selectedObjectId = node.id;

	if (ImGui::BeginPopupContextItem())
	{
		if (ImGui::MenuItem("Delete"))
		{
			if (node.children.empty())
				deleteSelectedItem();
			else
				deleteItem(node);
		}

		ImGui::EndPopup();
	}

	// Draw children
	if (open)
	{
		for (auto& child : node.children)
			DrawSceneTreeNode(child, filter, selectedObjectId);

		ImGui::TreePop();
	}
}

void Editor::DrawSceneTree()
{
	static ImGuiTextFilter filter;
	filter.Draw("Search");

	ObjectId selectedObjectId = 0xFFFFFFFF;

	float height = ImGui::GetContentRegionAvail().y * 0.5f;
	ImGui::BeginChild("SceneTreeRegion", ImVec2(0, height), true);

	for (auto& node : app.sceneMgr.graph.nodes)
	{
		DrawSceneTreeNode(node, filter, selectedObjectId);
	}

	ImGui::EndChild();

	if (selectedObjectId.value != 0xFFFFFFFF)
		selectItem(selectedObjectId, ImGui::IsKeyDown(ImGuiKey_LeftCtrl));
}

namespace fs = std::filesystem;

static std::vector<fs::path> FindFilesRecursive(const fs::path& root, const std::vector<std::string>& extensions)
{
	std::vector<fs::path> result;

	if (!fs::exists(root) || !fs::is_directory(root))
		return result;

	for (auto& entry : fs::recursive_directory_iterator(root))
	{
		if (!entry.is_regular_file())
			continue;

		std::string ext = entry.path().extension().string();

		// Normalize extension to lowercase
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

		for (const auto& wanted : extensions)
		{
			if (ext == wanted)
			{
				result.push_back(entry.path());
				break;
			}
		}
	}

	return result;
}

void Editor::lookupAssets()
{
	for (auto& file : FindFilesRecursive(SCENE_DIRECTORY, { ".scene" , ".gltf", ".prefab" }))
	{
		AssetItem asset;
		asset.name = file.filename().string();
		asset.path = file.string();

		if (file.extension() == ".scene")
			asset.type = AssetType::Scene;
		else if (file.extension() == ".prefab")
			asset.type = AssetType::Prefab;
		else
			asset.type = AssetType::Model;

		assets.push_back(asset);
	}
}

void Editor::DrawAssetBrowser(const std::vector<AssetItem>& assets)
{
	DrawAssetBrowserTopBar(assetsFilter);

	const float padding = 10.0f;      // spacing between cells
	const float iconSize = 32;      // size of the icon inside the cell
	const float cellSize = iconSize * 4;      // width & height of each grid cell

	float panelWidth = ImGui::GetContentRegionAvail().x;
	int columns = (int)(panelWidth / (cellSize + padding));
	if (columns < 1) columns = 1;

	int index = 0;

	for (const auto& asset : assets)
	{
		if (!PassesFilter(asset, assetsFilter))
			continue;

		ImGui::PushID(index);

		if (index % columns != 0)
			ImGui::SameLine();

		ImGui::BeginGroup();

		// Background button (full cell)
		ImGui::Button("##bg", ImVec2(cellSize, cellSize));

		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::TextUnformatted(asset.path.c_str());
			ImGui::EndTooltip();
		}

		// Drag-drop source (attached to the button)
		if (ImGui::BeginDragDropSource())
		{
			ImGui::SetDragDropPayload("ASSET_MODEL",
				asset.path.c_str(),
				asset.path.size() + 1);

			ImGui::Text("Add %s", asset.name.c_str());
			ImGui::EndDragDropSource();
		}

		// Get cell rect
		ImVec2 cellMin = ImGui::GetItemRectMin();
		ImVec2 cellMax = ImGui::GetItemRectMax();

		// Icon position (centered horizontally, slightly above center vertically)
		float iconY = cellMin.y + cellSize * 0.25f;
		float iconX = cellMin.x + (cellSize - iconSize) * 0.5f;

		ImGui::SetCursorScreenPos(ImVec2(iconX, iconY));

		const char* icon = ICON_FA_CUBE;
		if (asset.type == AssetType::Scene)   icon = ICON_FA_CUBES;
		if (asset.type == AssetType::Prefab)  icon = ICON_FA_CUBES_STACKED;

		ImGui::Text("%s", icon);

		// Text position (centered horizontally, near bottom but not touching)
		float textY = cellMin.y + cellSize * 0.65f;
		float textX = cellMin.x + 4.0f;

		ImGui::SetCursorScreenPos(ImVec2(textX, textY));
		ImGui::PushTextWrapPos(cellMin.x + cellSize - 4.0f);
		ImGui::Text("%s", asset.name.c_str());
		ImGui::PopTextWrapPos();

		ImGui::EndGroup();
		ImGui::PopID();

		index++;
	}
}

bool Editor::PassesFilter(const AssetItem& asset, const AssetBrowserFilter& filter)
{
	// Category filtering
	if (!filter.showModel && asset.type == AssetType::Model)
		return false;
	if (!filter.showScene && asset.type == AssetType::Scene)
		return false;
	if (!filter.showPrefab && asset.type == AssetType::Prefab)
		return false;

	// Search filtering
	if (filter.search[0] != '\0')
	{
		std::string nameLower = asset.name;
		std::string searchLower = filter.search;

		std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
		std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

		if (nameLower.find(searchLower) == std::string::npos)
			return false;
	}

	return true;
}

void Editor::DrawAssetBrowserTopBar(AssetBrowserFilter& filter)
{
	ImGui::BeginGroup();

	// Search box
	ImGui::SetNextItemWidth(200);
	ImGui::InputTextWithHint("##search", "Search...", filter.search, sizeof(filter.search));

	ImGui::SameLine(0, 20);

	// Category toggles
	auto ToggleButton = [&](const char* label, bool& value)
		{
			bool wasValue = value;
			if (wasValue)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 1.0f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.4f, 0.9f, 1.0f));
			}

			if (ImGui::Button(label))
				value = !value;

			if (wasValue)
				ImGui::PopStyleColor(3);
		};

	ToggleButton("Model", filter.showModel);
	ImGui::SameLine();
	ToggleButton("Scene", filter.showScene);
	ImGui::SameLine();
	ToggleButton("Prefab", filter.showPrefab);

	ImGui::EndGroup();

	ImGui::Separator();
}

void Editor::DrawTerrainShaderGraph()
{
	static NodeGraph graph;

	static int selectedNodesCount{};
	static int selectedLinksCount{};
	selectedNodesCount = ImNodes::NumSelectedNodes();
	selectedLinksCount = ImNodes::NumSelectedLinks();

	ImNodes::BeginNodeEditor();

	if (ImGui::BeginPopupContextWindow("node_editor_context", ImGuiPopupFlags_MouseButtonRight))
	{
		if (ImGui::BeginMenu("Add"))
		{
			int addedNodeId{};

			if (ImGui::BeginMenu("Constant"))
			{
				if (ImGui::MenuItem("Float"))
					addedNodeId = graph.CreateNode(std::make_unique<ConstantFloatNode>());
				else if (ImGui::MenuItem("Swizzle"))
					addedNodeId = graph.CreateNode(std::make_unique<SwizzleNode>());

				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Math"))
			{
				if (ImGui::MenuItem("Add"))
					addedNodeId = graph.CreateNode(std::make_unique<AddNode>());
				else if (ImGui::MenuItem("Multiply"))
					addedNodeId = graph.CreateNode(std::make_unique<MulNode>());
				else if (ImGui::MenuItem("Sin"))
					addedNodeId = graph.CreateNode(std::make_unique<SinNode>());
				else if (ImGui::MenuItem("Pow"))
					addedNodeId = graph.CreateNode(std::make_unique<PowNode>());
				else if (ImGui::MenuItem("Saturate"))
					addedNodeId = graph.CreateNode(std::make_unique<SaturateNode>());

				ImGui::EndMenu();
			}
			if (ImGui::MenuItem("Texture"))
				addedNodeId = graph.CreateNode(std::make_unique<TextureSampleNode>());
			if (ImGui::MenuItem("Output"))
				addedNodeId = graph.CreateNode(std::make_unique<FloatOutputNode>([this](std::string func)
					{
						app.resources.shaderDefines.setDefine("HEIGHT_FUNC", func);
					}));

			if (addedNodeId)
			{
				ImNodes::SetNodeScreenSpacePos(addedNodeId, ImGui::GetWindowPos());
				ImNodes::SnapNodeToGrid(addedNodeId);
			}

			ImGui::EndMenu();
		}
		if ((selectedNodesCount || selectedLinksCount ) && ImGui::MenuItem("Delete"))
		{
			if (selectedNodesCount)
			{
				std::vector<int> selection(selectedNodesCount);
				ImNodes::GetSelectedNodes(selection.data());
				ImNodes::ClearNodeSelection();

				for (auto& s : selection)
				{
					graph.RemoveNode(s);
				}
			}
			if (selectedLinksCount)
			{
				std::vector<int> selection(selectedLinksCount);
				ImNodes::GetSelectedLinks(selection.data());
				ImNodes::ClearLinkSelection();

				for (auto& s : selection)
				{
					graph.DestroyLink(s);
				}
			}
		}

		ImGui::EndPopup();
	}

	for (auto& node : graph.nodes)
	{
		node.second->Draw();
	}

	for (auto& link : graph.links)
	{
		ImNodes::Link(link.id, link.outputPinId, link.inputPinId);
	}

	ImNodes::EndNodeEditor();

	{
		int start_attr, end_attr;
		if (ImNodes::IsLinkCreated(&start_attr, &end_attr))
		{
			graph.ConnectPins(start_attr, end_attr);
		}
	}
}
