#include "DebugWindow.h"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"
#include "AaRenderSystem.h"
#include "AaSceneManager.h"
#include "AaMaterialResources.h"

bool stopUpdatingVoxel = false;

float voxelSteppingBounces = 0.075f;
float voxelSteppingDiffuse = 0.0f;
Vector2 middleConeRatioDistance = { 1.05f, 1.5f };
Vector2 sideConeRatioDistance = { 2.2f, 5.f };

XMFLOAT3 currentCamPos{};

imgui::DebugWindow* instance{};

bool ImguiUsesInput()
{
	if (instance && ImGui::GetIO().WantCaptureMouse)
		return true;

	return false;
}

namespace imgui
{
	static ID3D12DescriptorHeap* g_pd3dSrvDescHeap = nullptr;

	imgui::DebugWindow& DebugWindow::Get()
	{
		return *instance;
	}

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

		instance = this;
	}

	void DebugWindow::deinit()
	{
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

		g_pd3dSrvDescHeap->Release();

		instance = nullptr;
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

		ImGui::Text("Cam pos %.1f %.1f %.1f", currentCamPos.x, currentCamPos.y, currentCamPos.z);

		int next = state.TexturePreviewIndex;
		if (ImGui::InputInt("Texture preview", &next))
		{
			if (next < 0)
				state.TexturePreviewIndex = -1;
			else if (next == 0)
				state.TexturePreviewIndex = 0;
			else if (next > state.TexturePreviewIndex)
				state.TexturePreviewIndex =	ResourcesManager::get().nextDescriptor(state.TexturePreviewIndex, D3D12_SRV_DIMENSION_TEXTURE2D);
			else
				state.TexturePreviewIndex = ResourcesManager::get().previousDescriptor(state.TexturePreviewIndex, D3D12_SRV_DIMENSION_TEXTURE2D);
		}

		if (ImGui::Button(stopUpdatingVoxel ? "startUpdatingVoxel" : "stopUpdatingVoxel"))
			stopUpdatingVoxel = !stopUpdatingVoxel;
		
		ImGui::SliderFloat("GI weight", &voxelSteppingBounces, 0.0f, 0.15f);
		ImGui::SliderFloat("Diffuse weight", &voxelSteppingDiffuse, 0.0f, 0.5f);

		static float voxelLightPower = []{ float f; AaMaterialResources::get().getMaterial("WhiteVCTLight")->GetParameter(FastParam::Emission, &f); return f; }();

		if (ImGui::SliderFloat("Light power", &voxelLightPower, 0.0f, 10.f))
		{
			AaMaterialResources::get().getMaterial("WhiteVCTLight")->SetParameter(FastParam::Emission, &voxelLightPower);
		}

		ImGui::SliderFloat("middleConeRatio", &middleConeRatioDistance.x, 0.0f, 5.f);
		ImGui::SliderFloat("middleConeDistance", &middleConeRatioDistance.y, 0.0f, 5.f);
		ImGui::SliderFloat("sideConeRatio", &sideConeRatioDistance.x, 0.0f, 5.f);
		ImGui::SliderFloat("sideConeDistance", &sideConeRatioDistance.y, 0.0f, 5.f);

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

		ID3D12DescriptorHeap* descriptorHeaps[] = { g_pd3dSrvDescHeap };
		commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
	}
}
