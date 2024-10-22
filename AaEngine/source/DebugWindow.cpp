#include "DebugWindow.h"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"
#include "AaRenderSystem.h"
#include "AaSceneManager.h"
#include "AaMaterialResources.h"

bool stopUpdatingVoxel = false;

namespace imgui
{
	static ID3D12DescriptorHeap* g_pd3dSrvDescHeap = nullptr;

	void DebugWindow::init(AaRenderSystem* renderer)
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();

		{
			D3D12_DESCRIPTOR_HEAP_DESC desc = {};
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			desc.NumDescriptors = 1;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			if (renderer->device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dSrvDescHeap)) != S_OK)
				return;

			g_pd3dSrvDescHeap->SetName(L"ImguiDescriptors");
		}

		ImGui_ImplWin32_Init(renderer->getWindow()->getHwnd());
		ImGui_ImplDX12_Init(renderer->device, renderer->FrameCount,
			DXGI_FORMAT_R8G8B8A8_UNORM, g_pd3dSrvDescHeap,
			g_pd3dSrvDescHeap->GetCPUDescriptorHandleForHeapStart(),
			g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart());
	}

	void DebugWindow::deinit()
	{
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

		g_pd3dSrvDescHeap->Release();
	}

	void DebugWindow::draw(ID3D12GraphicsCommandList* commandList)
	{
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("Debug");

		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		if (ImGui::Button("Reload shaders"))
			state.reloadShaders = true;

		if (ImGui::Button("stopUpdatingVoxel"))
			stopUpdatingVoxel = !stopUpdatingVoxel;
		
		const char* scenes[] = {
			"test",
			"testCubes",
			"voxelRoom"
		};
		static int currentScene = 0;
		if (ImGui::Combo("Scene", &currentScene, scenes, std::size(scenes)))
			state.changeScene = scenes[currentScene];

		ImGui::End();

		ImGui::Render();

		ID3D12DescriptorHeap* descriptorHeaps[] = { g_pd3dSrvDescHeap };
		commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
	}
}
