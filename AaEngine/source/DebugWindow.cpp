#include "DebugWindow.h"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"
#include "RenderSystem.h"
#include "SceneManager.h"
#include "MaterialResources.h"
#include "DebugOverlayTask.h"
#include "SystemUtils.h"

imgui::DebugWindow* instance{};

bool ImguiUsesInput()
{
	if (instance && ImGui::GetIO().WantCaptureMouse)
		return true;

	return false;
}
extern float jitterScaleX;
extern float jitterScaleY;

namespace imgui
{
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

	static ExampleDescriptorHeapAllocator srvDescHeapAlloc;
	static ID3D12DescriptorHeap* srvDescHeap = nullptr;

	DebugState DebugWindow::state;

	imgui::DebugWindow& DebugWindow::Get()
	{
		return *instance;
	}

	DebugWindow::DebugWindow(RenderSystem& renderer)
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();

		{
			D3D12_DESCRIPTOR_HEAP_DESC desc = {};
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			desc.NumDescriptors = 64;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			if (renderer.core.device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&srvDescHeap)) != S_OK)
				return;

			srvDescHeap->SetName(L"ImguiDescriptors");
			srvDescHeapAlloc.Create(renderer.core.device, srvDescHeap);
		}

		ImGui_ImplWin32_Init(renderer.viewport.getHwnd());
		
		ImGui_ImplDX12_InitInfo init_info = {};
		init_info.Device = renderer.core.device;
		init_info.CommandQueue = renderer.core.commandQueue;
		init_info.NumFramesInFlight = FrameCount;
		init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		init_info.DSVFormat = DXGI_FORMAT_UNKNOWN;
		init_info.SrvDescriptorHeap = srvDescHeap;
		init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle) { return srvDescHeapAlloc.Alloc(out_cpu_handle, out_gpu_handle); };
		init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) { return srvDescHeapAlloc.Free(cpu_handle, gpu_handle); };
		ImGui_ImplDX12_Init(&init_info);

		instance = this;
	}

	DebugWindow::~DebugWindow()
	{
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

		srvDescHeap->Release();

		instance = nullptr;
	}

	void DebugWindow::draw(ID3D12GraphicsCommandList* commandList, MaterialResources& materials)
	{
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("Debug");

		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		if (ImGui::Button("Reload shaders"))
			state.reloadShaders = true;

		ImGui::Text("VRAM used %uMB", GetGpuMemoryUsage());

// 		ImGui::SliderFloat("jitterScaleX", &jitterScaleX, -3.0f, 3.f);
// 		ImGui::SliderFloat("jitterScaleY", &jitterScaleY, -3.0f, 3.f);

		{
			auto& overlayTask = DebugOverlayTask::Get();

			int next = overlayTask.currentIdx();
			if (ImGui::InputInt("Texture preview", &next))
				overlayTask.changeIdx(next);

			if (auto name = overlayTask.getCurrentIdxName())
				ImGui::Text("Texture: %s", name);
		}

		if (ImGui::CollapsingHeader("GI"))
		{
			if (ImGui::Button(state.stopUpdatingVoxel ? "startUpdatingVoxel" : "stopUpdatingVoxel"))
				state.stopUpdatingVoxel = !state.stopUpdatingVoxel;

// 			static float voxelLightPower = [&] { float f; materials.getMaterial("WhiteVCTLight")->GetParameter(FastParam::Emission, &f); return f; }();
//
// 			if (ImGui::SliderFloat("Light power", &voxelLightPower, 0.0f, 10.f))
// 			{
// 				materials.getMaterial("WhiteVCTLight")->SetParameter(FastParam::Emission, &voxelLightPower);
// 			}

// 			ImGui::SliderFloat("middleConeRatio", &state.middleConeRatioDistance.x, 0.0f, 5.f);
// 			ImGui::SliderFloat("middleConeDistance", &state.middleConeRatioDistance.y, 0.0f, 5.f);
// 			ImGui::SliderFloat("sideConeRatio", &state.sideConeRatioDistance.x, 0.0f, 5.f);
// 			ImGui::SliderFloat("sideConeDistance", &state.sideConeRatioDistance.y, 0.0f, 5.f);
		}

		if (ImGui::CollapsingHeader("SSAO"))
		{
			ImGui::SliderFloat("Accentuation", &state.ssao.Accentuation, 0.0f, 1.f);
			ImGui::SliderFloat("RejectionFalloff", &state.ssao.RejectionFalloff, 1.0f, 10.f);
			ImGui::SliderFloat("BlurTolerance", &state.ssao.BlurTolerance, -8.0f, -1.f);
			ImGui::SliderFloat("NoiseFilterTolerance", &state.ssao.NoiseFilterTolerance, -8.0f, 0.f);
			ImGui::SliderFloat("UpsampleTolerance", &state.ssao.UpsampleTolerance, -12.0f, -1.f);
		}

		ImGui::Combo("DLSS", &state.DlssMode, UpscaleModeNames, std::size(UpscaleModeNames));
		ImGui::Combo("FSR", &state.FsrMode, UpscaleModeNames, std::size(UpscaleModeNames));

		const char* scenes[] = {
			"test",
			"testCubes",
			"voxelRoom",
			"voxelOutside"
		};
		static int currentScene = 0;
		if (ImGui::Combo("Scene", &currentScene, scenes, std::size(scenes)))
			state.changeScene = scenes[currentScene];

		ImGui::End();

		ImGui::Render();

		ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescHeap };
		commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
	}
}
