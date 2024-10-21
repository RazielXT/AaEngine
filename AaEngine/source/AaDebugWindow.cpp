#include "AaDebugWindow.h"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"
#include "AaRenderSystem.h"
#include "AaSceneManager.h"

void DebugWindow::init(AaSceneManager* sceneMgr, AaRenderSystem* renderer)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(renderer->getWindow()->getHwnd());
	ImGui_ImplDX11_Init(renderer->getDevice(), renderer->getContext());

	scene = sceneMgr;
}

void DebugWindow::deinit()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

float stepping = 0.07f;
float stepping2 = 0.03f;
float lightPower = 5.0f;
float middleCone = 0;
float sideCone = 0;
float radius = 1;

extern bool giOneStep;
extern bool giKeepStepping;

void DebugWindow::draw()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("GI");

	if (giKeepStepping)
	{
		if (ImGui::Button("Stop bouncing"))
			giKeepStepping = false;
	}
	else
	{
		if (ImGui::Button("Keep bouncing"))
			giKeepStepping = true;

		ImGui::SameLine();

		if (ImGui::Button("One bounce"))
			giOneStep = true;
	}

	if (ImGui::SliderFloat("GI weight", &stepping, 0.0f, 0.15f))
	{
		AaMaterialResources::get().getMaterial("VoxelizationMat")->setMaterialConstant("PerMaterial", "stepping", ShaderTypePixel, &stepping);
	}

	if (ImGui::SliderFloat("Diffuse weight", &stepping2, 0.0f, 0.5f))
	{
		AaMaterialResources::get().getMaterial("VoxelizationMat")->setMaterialConstant("PerMaterial", "stepping2", ShaderTypePixel, &stepping2);
	}

	if (ImGui::SliderFloat("Light power", &lightPower, 0.0f, 10.f))
	{
		AaMaterialResources::get().getMaterial("LightVoxelizationMat")->setMaterialConstant("PerMaterial", "lightPower", ShaderTypePixel, &lightPower);
	}

// 	if (ImGui::SliderFloat("Cone radius", &radius, 0.0f, 5.f))
// 	{
// 		AaMaterialResources::get().getMaterial("VoxelizationMat")->setMaterialConstant("PerMaterial", "radius", ShaderTypePixel, &radius);
// 	}

	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	ImGui::End();

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}
