#include "Editor.h"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"
#include "ApplicationCore.h"
#include "EntityPicker.h"
#include "ImGuizmo.h"
#include <format>
#include "UpscaleTypes.h"
#include "DebugOverlayTask.h"
#include "DebugWindow.h"
#include "VoxelizeSceneTask.h"
#include <algorithm>
#include "PhysicsRenderTask.h"
#include "RenderWireframeTask.h"

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
}

void Editor::initialize(TargetWindow& v)
{
	renderPanelViewport.set(v.getHwnd(), 640, 480);
	lastViewportPanelSize = viewportPanelSize = { renderPanelViewport.getWidth(), renderPanelViewport.getHeight() };

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	auto& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	auto scaling = GetDpiForWindow(v.getHwnd()) * 0.01f;
	io.Fonts->AddFontFromFileTTF(R"(c:\Windows\Fonts\segoeui.ttf)", 16.0f * scaling);

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

	ImGui_ImplDX12_InitInfo init_info = {};
	init_info.Device = renderer.device;
	init_info.CommandQueue = renderer.commandQueue;
	init_info.NumFramesInFlight = FrameCount;
	init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	init_info.DSVFormat = DXGI_FORMAT_UNKNOWN;
	init_info.SrvDescriptorHeap = g_pd3dSrvDescHeap;
	init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle) { return g_pd3dSrvDescHeapAlloc.Alloc(out_cpu_handle, out_gpu_handle); };
	init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) { return g_pd3dSrvDescHeapAlloc.Free(cpu_handle, gpu_handle); };
	ImGui_ImplDX12_Init(&init_info);

	commands = renderer.CreateCommandList(L"Editor");
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
	target.PrepareAsTarget(commands.commandList, D3D12_RESOURCE_STATE_PRESENT, true);

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
			updateEntitySelect = true;
		}

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
	updateEntitySelect = false;
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
			if (ImGui::ImageButton(options[i].iconName, (ImTextureID)icons[options[i].iconName]->srvHandles.ptr, ImVec2(32, 32), ImVec2(0, 0), ImVec2(1, 1), ItemBg(i)))
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

	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

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

	if (updateEntitySelect)
	{
		auto& pickInfo = EntityPicker::Get().getLastPick();

		if (auto selectedId = pickInfo.id)
		{
			if (addTree)
			{
				ObjectTransformation tr;
				tr.position = pickInfo.position;
				tr.position.y -= 2;
				tr.orientation = Quaternion::CreateFromYawPitchRoll(getRandomFloat(0, XM_2PI), 0, 0);
				tr.scale = Vector3(getRandomFloat(1.8f, 2.2f));

				if (addTreeNormals)
					tr.orientation *= Quaternion::FromToRotation(Vector3::UnitY, pickInfo.normal);

				static int t = 0;

				auto model = app.resources.models.getLoadedModel("treeBunchBigLeafs.mesh", ResourceGroup::Core);
				auto tree = app.sceneMgr.createEntity("Tree" + std::to_string(t), tr, *model);
				tree->material = app.resources.materials.getMaterial("TreeBranch");

				auto modelTrunk = app.resources.models.getLoadedModel("treeBunchBigTrunk.mesh", ResourceGroup::Core);
				auto treeTrunk = app.sceneMgr.createEntity("TreeTrunk" + std::to_string(t), tr, *modelTrunk);
				treeTrunk->material = app.resources.materials.getMaterial("TreeTrunk");

				t++;
			}
			else
			{
				selectionPosition = pickInfo.position;
				bool newObject = true;

				if (!ctrlActive)
				{
					selection.clear();
				}
				else
				{
					for (auto& s : selection)
					{
						if (s.obj.id == selectedId)
							newObject = false;
					}
				}

				if (newObject)
				{
					auto obj = app.sceneMgr.getObject(selectedId);
					if (obj)
					{
						selection.emplace_back(obj.getTransformation(), obj);
					}
				}
			}
		}
		else if (!ctrlActive)
			selection.clear();

		updateEntitySelect = false;

		EntityPicker::Get().active.clear();
		for (auto& s : selection)
		{
			EntityPicker::Get().active.push_back(s.obj.id);
		}
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

		if (true) //centered gizmo
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

		ImGuizmo::Manipulate(&cameraView._11, &cameraProjection._11,
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

				s.obj.setTransformation(newTransform);
			}
		}

		gizmoActive = ImGuizmo::IsOver();

		if (ImGuizmo::IsUsing())
			VoxelizeSceneTask::Get().revoxelize();
	}

	ImGui::End();
	ImGui::PopStyleVar();

	ImGui::Begin("Debug2");
	ImGui::Text("Viewport size %d %d", viewportPanelSize.x, viewportPanelSize.y);

	if (!selection.empty())
	{
		ImGui::Text("Selected count %d", selection.size());
		ImGui::Text("EntityId %#010x", selection.back().obj.id);
		ImGui::Text("Entity name %s", selection.back().obj.getName());
		ImGui::Text("Entity pos %f %f %f", objTransformation.position.x, objTransformation.position.y, objTransformation.position.z);
		ImGui::Text("Selection pos %f %f %f", selectionPosition.x, selectionPosition.y, selectionPosition.z);
	}

	ImGui::Checkbox("Add Tree", &addTree);
	if (addTree)
		ImGui::Checkbox("Add Tree on normals", &addTreeNormals);

	ImGui::NewLine();

	if (ImGui::Button("Reload shaders"))
		state.reloadShaders = true;

	if (ImGui::Button("Rebuild terrain"))
		app.sceneMgr.terrain.rebuild();

	auto& wireframeTask = RenderWireframeTask::Get();
	ImGui::Checkbox("Render wireframe", &wireframeTask.enabled);

	ImGui::Combo("DLSS", &state.DlssMode, UpscaleModeNames, std::size(UpscaleModeNames));
	ImGui::Combo("FSR", &state.FsrMode, UpscaleModeNames, std::size(UpscaleModeNames));

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

	const char* scenes[] = {
		"test",
		"testCubes",
		"voxelRoom",
		"voxelOutside",
		"voxelOutsideBig",
		"voxelOutsideBigCave"
	};
	static int currentScene = 0;
	if (ImGui::Combo("Scene", &currentScene, scenes, std::size(scenes)))
		state.changeScene = scenes[currentScene];

	{
		auto& overlayTask = DebugOverlayTask::Get();

		int next = overlayTask.currentIdx();
		if (ImGui::InputInt("Texture preview", &next))
			overlayTask.changeIdx(next);

		if (auto name = overlayTask.getCurrentIdxName())
			ImGui::Text("Texture: %s", name);
	}

	static bool showVoxels = false;
	if (ImGui::Checkbox("Show voxels", &showVoxels))
		VoxelizeSceneTask::Get().showVoxelsInfo(showVoxels);

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
		VoxelizeSceneTask::Get().clear();

	if (ImGui::CollapsingHeader("Sun"))
	{
		static int sunYaw = 0;
		static int lastYaw = sunYaw;
		static int sunPitch = 0;
		static int lastPitch = sunPitch;
		if (ImGui::InputInt("Yaw", &sunYaw, 1, 3) || ImGui::InputInt("Pitch", &sunPitch, 1, 3))
		{
			auto rotation = Quaternion::CreateFromYawPitchRoll(XMConvertToRadians((float)lastYaw - sunYaw), XMConvertToRadians((float)lastPitch - sunPitch), 0);
			app.lights.directionalLight.direction = rotation * app.lights.directionalLight.direction;
			lastPitch = sunPitch;
			lastYaw = sunYaw;
		}
	}
	{
		ImGui::Checkbox("Limit framerate", &state.limitFrameRate);

		UpdateFrameTimeBuffer();

		ImGui::PlotLines("Frame Times", frameTimes, std::size(frameTimes), frameChartIndex, NULL, 0.0f, maxFrameTime, ImVec2(0, 80));
	}
	ImGui::End();
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
		g_pd3dSrvDescHeapAlloc.Alloc(&icon->handle, &icon->srvHandles);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Format = icon->texture->GetDesc().Format;

		renderer.device->CreateShaderResourceView(icon->texture.Get(), &srvDesc, icon->handle);
	}
}
