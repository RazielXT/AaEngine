#include "Editor.h"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"
#include "../data/editor/fonts/IconsFontAwesome7.h"
#include "ApplicationCore.h"
#include "ImGuizmo.h"
#include "imnodes/imnodes.h"
#include "Utils/SystemUtils.h"

Editor::Editor(ApplicationCore& a, ImguiPanelViewport& v) : app(a), renderer(a.renderSystem.core), renderPanelViewport(v),
	viewportPanel(a, selection, v, srvDescHeapAlloc, a.renderSystem.core),
	sceneTreePanel(a.sceneMgr, selection),
	terrainShaderGraphPanel(a.resources.shaderDefines),
	sidePanel(a, selection, state, sceneTreePanel, viewportPanel)
{
#ifdef _DEBUG
	state.limitFrameRate = true;
#endif
}

void Editor::initializeUi(const TargetWindow& v)
{
	renderPanelViewport.set(v.getHwnd(), 640, 480);
	lastViewportPanelSize = { renderPanelViewport.getWidth(), renderPanelViewport.getHeight() };

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
	io.Fonts->AddFontFromFileTTF(R"(../data/editor/fonts/Font Awesome 7 Free-Solid-900.otf)", 16.0f * scaling, &config, icon_ranges);

	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = 64;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		if (renderer.device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&srvDescHeap)) != S_OK)
			return;

		srvDescHeap->SetName(L"ImguiDescriptors");
		srvDescHeapAlloc.Create(renderer.device, srvDescHeap);
	}

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
	init_info.SrvDescriptorHeap = srvDescHeap;
	init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle) { return static_cast<ViewportPanel::DescriptorHeapAllocator*>(info->UserData)->Alloc(out_cpu_handle, out_gpu_handle); };
	init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) { return static_cast<ViewportPanel::DescriptorHeapAllocator*>(info->UserData)->Free(cpu_handle, gpu_handle); };
	init_info.UserData = &srvDescHeapAlloc;
	ImGui_ImplDX12_Init(&init_info);

	commands = renderer.CreateCommandList(L"Editor", PixColor::Editor);
}

void Editor::updateViewportSize()
{
	static bool lastSizeChanged = false;

	auto viewportPanelSize = viewportPanel.getViewportSize();

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
	auto viewportPanelSize = viewportPanel.getViewportSize();
	renderPanelViewport.set(renderPanelViewport.getHwnd(), viewportPanelSize.x, viewportPanelSize.y);

	for (auto l : renderPanelViewport.listeners)
		l->onViewportResize(viewportPanelSize.x, viewportPanelSize.y);

	viewportPanel.resetOutputDescriptor();
}

void Editor::render(Camera& camera)
{
	auto marker = renderer.StartCommandList(commands, srvDescHeap);
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

	srvDescHeapAlloc.Destroy();
	srvDescHeap->Release();

	commands.deinit();
}

bool Editor::onClick(MouseButton button)
{
	auto* tool = viewportPanel.getActiveTool();

	if (button == MouseButton::Right)
	{
		if (tool && tool->isInteracting())
		{
			tool->cancel();
			return true;
		}
	}

	if (tool && (tool->isInteracting() || tool->isOverlayActive()))
		return false;

	if (button == MouseButton::Left)
	{
		viewportPanel.scheduleViewportPick();

		return true;
	}

	return false;
}

bool Editor::keyPressed(int key)
{
	return false;
}

bool Editor::keyReleased(int key)
{
	return false;
}

void Editor::reset()
{
	selection.clear();
	viewportPanel.reset();
}

void Editor::prepareElements(Camera& camera)
{
	ImGui::Begin("Runtime");

	ImGui::Text("%.1f FPS (%.2f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
	ImGui::SameLine();
	ImGui::Checkbox("Limit framerate", &state.limitFrameRate);

	ImGui::Text("VRAM used %uMB", GetGpuMemoryUsage());
	ImGui::Text("RAM used %uMB", GetSystemMemoryUsage());

	ImGui::End();

	ImGui::Begin("Assets");
	assetBrowserPanel.draw();
	ImGui::End();

	ImGui::Begin("Terrain");
	terrainShaderGraphPanel.draw();
	ImGui::End();

	ImGui::Begin("Logs");
	logPanel.draw();
	ImGui::End();

	viewportPanel.draw(camera);
	sidePanel.draw();
}
