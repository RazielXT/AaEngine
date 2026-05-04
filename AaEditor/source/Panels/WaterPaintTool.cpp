#include "WaterPaintTool.h"
#include "imgui.h"
#include "../data/editor/fonts/IconsFontAwesome7.h"
#include "Utils/Logger.h"

void WaterPaintTool::onPick(const EntityPicker::PickInfo& pickInfo, bool ctrlActive, Camera& camera)
{
	if (pickInfo.id)
	{
		const char* action = (mode == Mode::Add) ? "Adding" : "Removing";
		Logger::log(std::format("{} water at ({:.2f}, {:.2f}, {:.2f}) radius {:.1f}",
			action, pickInfo.position.x, pickInfo.position.y, pickInfo.position.z, brushRadius));
	}
}

void WaterPaintTool::draw(const ViewportToolContext& ctx)
{
	auto activeBg = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
	auto inactiveBg = ImGui::GetStyle().Colors[ImGuiCol_FrameBg];

	ImGui::SameLine();
	//ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	ImGui::BeginGroup();

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 8));

	// Add water button
	ImGui::PushStyleColor(ImGuiCol_Button, mode == Mode::Add ? activeBg : inactiveBg);
	if (ImGui::Button(ICON_FA_DROPLET " ##water_add", ImVec2(40, 40)))
		mode = Mode::Add;
	ImGui::PopStyleColor();

	// Remove water button
	ImGui::PushStyleColor(ImGuiCol_Button, mode == Mode::Remove ? activeBg : inactiveBg);
	if (ImGui::Button(ICON_FA_DROPLET_SLASH " ##water_remove", ImVec2(40, 40)))
		mode = Mode::Remove;
	ImGui::PopStyleColor();

	ImGui::PopStyleVar();

	ImGui::EndGroup();

	toolbarHovered = ImGui::IsItemHovered();
}
