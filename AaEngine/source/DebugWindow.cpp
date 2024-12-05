#include "DebugWindow.h"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"
#include "RenderSystem.h"
#include "SceneManager.h"
#include "AaMaterialResources.h"

XMFLOAT3 currentCamPos{};

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
	static ID3D12DescriptorHeap* g_pd3dSrvDescHeap = nullptr;
	DebugState DebugWindow::state;

	imgui::DebugWindow& DebugWindow::Get()
	{
		return *instance;
	}

	void DebugWindow::init(RenderSystem* renderer)
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

		ImGui::Text("VRAM used %uMB", state.vramUsage);

// 		ImGui::SliderFloat("jitterScaleX", &jitterScaleX, -3.0f, 3.f);
// 		ImGui::SliderFloat("jitterScaleY", &jitterScaleY, -3.0f, 3.f);

		int next = state.TexturePreviewIndex;
		if (ImGui::InputInt("Texture preview", &next))
		{
			if (next < 0)
				state.TexturePreviewIndex = -1;
			else if (next == 0)
				state.TexturePreviewIndex = 0;
			else if (next > state.TexturePreviewIndex)
				state.TexturePreviewIndex =	DescriptorManager::get().nextDescriptor(state.TexturePreviewIndex, D3D12_SRV_DIMENSION_TEXTURE2D);
			else
				state.TexturePreviewIndex = DescriptorManager::get().previousDescriptor(state.TexturePreviewIndex, D3D12_SRV_DIMENSION_TEXTURE2D);
		}

		if (ImGui::CollapsingHeader("GI"))
		{
			if (ImGui::Button(state.stopUpdatingVoxel ? "startUpdatingVoxel" : "stopUpdatingVoxel"))
				state.stopUpdatingVoxel = !state.stopUpdatingVoxel;

			ImGui::SliderFloat("GI weight", &state.voxelSteppingBounces, 0.0f, 0.15f);
			ImGui::SliderFloat("Diffuse weight", &state.voxelSteppingDiffuse, 0.0f, 0.5f);

			static float voxelLightPower = [] { float f; AaMaterialResources::get().getMaterial("WhiteVCTLight")->GetParameter(FastParam::Emission, &f); return f; }();

			if (ImGui::SliderFloat("Light power", &voxelLightPower, 0.0f, 10.f))
			{
				AaMaterialResources::get().getMaterial("WhiteVCTLight")->SetParameter(FastParam::Emission, &voxelLightPower);
			}

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

		ImGui::Combo("DLSS", &state.DlssMode, state.DlssModeNames, std::size(state.DlssModeNames));

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
