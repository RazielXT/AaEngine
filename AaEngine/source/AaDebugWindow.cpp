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

float stepping = 0.1f;
float stepping2 = 1.f;
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

	float stepInput = stepping;
	ImGui::SliderFloat("GI weight", &stepInput, 0.0f, 0.2f);
	if (stepInput != stepping)
	{
		stepping = stepInput;
		AaMaterialResources::get().getMaterial("VoxelizationMat")->setMaterialConstant("per_material", "stepping", ShaderTypePixel, &stepping);
	}

	float stepping2Input = stepping2;
	ImGui::SliderFloat("Diffuse weight", &stepping2Input, 0.0f, 2.f);
	if (stepping2Input != stepping2)
	{
		stepping2 = stepping2Input;
		AaMaterialResources::get().getMaterial("VoxelizationMat")->setMaterialConstant("per_material", "stepping2", ShaderTypePixel, &stepping2);
	}

	float radiusInput = radius;
	ImGui::SliderFloat("Cone radius", &radiusInput, 0.0f, 5.f);
	if (radiusInput != radius)
	{
		radius = radiusInput;
		AaMaterialResources::get().getMaterial("VoxelizationMat")->setMaterialConstant("per_material", "radius", ShaderTypePixel, &radius);
	}

// 	float sideConeInput = sideCone;
// 	ImGui::SliderFloat("sideCone", &sideConeInput, 0.0f, 0.15f);
// 	if (sideConeInput != sideCone)
// 	{
// 		sideCone = sideConeInput;
// 		scene->getMaterial("VoxelizationMat")->setMaterialConstant("sideCone", Shader_type_pixel, &sideCone);
// 	}
// 
// 	float middleConeInput = middleCone;
// 	ImGui::SliderFloat("middleCone", &middleConeInput, 0.0f, 0.15f);
// 	if (middleConeInput != middleCone)
// 	{
// 		middleCone = middleConeInput;
// 		scene->getMaterial("VoxelizationMat")->setMaterialConstant("middleCone", Shader_type_pixel, &middleCone);
// 	}

	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	ImGui::End();

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}
