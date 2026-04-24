#include "Editor.h"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"
#include "../data/editor/icons/IconsFontAwesome7.h"
#include "ApplicationCore.h"
#include "Editor/EntityPicker.h"
#include "ImGuizmo.h"
#include "imnodes/imnodes.h"
#include "Utils/SystemUtils.h"

void Editor::DescriptorHeapAllocator::Create(ID3D12Device* device, ID3D12DescriptorHeap* heap)
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

void Editor::DescriptorHeapAllocator::Destroy()
{
	Heap = nullptr;
	FreeIndices.clear();
}

void Editor::DescriptorHeapAllocator::Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle)
{
	IM_ASSERT(FreeIndices.Size > 0);
	int idx = FreeIndices.back();
	FreeIndices.pop_back();
	out_cpu_desc_handle->ptr = HeapStartCpu.ptr + (idx * HeapHandleIncrement);
	out_gpu_desc_handle->ptr = HeapStartGpu.ptr + (idx * HeapHandleIncrement);
}

void Editor::DescriptorHeapAllocator::Free(D3D12_CPU_DESCRIPTOR_HANDLE out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE out_gpu_desc_handle)
{
	int cpu_idx = (int)((out_cpu_desc_handle.ptr - HeapStartCpu.ptr) / HeapHandleIncrement);
	int gpu_idx = (int)((out_gpu_desc_handle.ptr - HeapStartGpu.ptr) / HeapHandleIncrement);
	IM_ASSERT(cpu_idx == gpu_idx);
	FreeIndices.push_back(cpu_idx);
}

Editor::Editor(ApplicationCore& a, ImguiPanelViewport& v) : app(a), renderer(a.renderSystem.core), renderPanelViewport(v),
	sceneTreePanel(a.sceneMgr, selection),
	terrainShaderGraphPanel(a.resources.shaderDefines),
	sidePanel(a, selection, state, sceneTreePanel)
{
#ifdef _DEBUG
	state.limitFrameRate = true;
#endif
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
		if (renderer.device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&srvDescHeap)) != S_OK)
			return;

		srvDescHeap->SetName(L"ImguiDescriptors");
		srvDescHeapAlloc.Create(renderer.device, srvDescHeap);
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
	init_info.SrvDescriptorHeap = srvDescHeap;
	init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle) { return static_cast<DescriptorHeapAllocator*>(info->UserData)->Alloc(out_cpu_handle, out_gpu_handle); };
	init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) { return static_cast<DescriptorHeapAllocator*>(info->UserData)->Free(cpu_handle, gpu_handle); };
	init_info.UserData = &srvDescHeapAlloc;
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
		srvDescHeapAlloc.Free(renderOutputTextureImguiHandleCpu, renderOutputTextureImguiHandleGpu);

		renderOutputTextureImguiHandleGpu = {};
		renderOutputTextureImguiHandleCpu = {};
	}
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
		scenePickScheduled = true;
	}
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

	ObjectTransformation objTransformation{};
	drawViewport(camera, objTransformation);
	sidePanel.draw(objTransformation);
}

void Editor::ensureViewportOutputDescriptor()
{
	if (!renderOutputTextureImguiHandleGpu.ptr)
	{
		auto renderOutput = app.compositor->getTexture("Output");
		srvDescHeapAlloc.Alloc(&renderOutputTextureImguiHandleCpu, &renderOutputTextureImguiHandleGpu);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Format = renderOutput->texture->GetDesc().Format;

		renderer.device->CreateShaderResourceView(renderOutput->texture.Get(), &srvDesc, renderOutputTextureImguiHandleCpu);
	}
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
		srvDescHeapAlloc.Alloc(&icon->handle, &icon->srvHandle);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Format = icon->texture->GetDesc().Format;

		renderer.device->CreateShaderResourceView(icon->texture.Get(), &srvDesc, icon->handle);
	}
}
