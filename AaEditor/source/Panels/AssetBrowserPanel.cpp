#include "AssetBrowserPanel.h"
#include "imgui.h"
#include "../data/editor/fonts/IconsFontAwesome7.h"
#include "App/Directories.h"
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

static std::vector<fs::path> FindFilesRecursive(const fs::path& root, const std::vector<std::string>& extensions)
{
	std::vector<fs::path> result;

	if (!fs::exists(root) || !fs::is_directory(root))
		return result;

	for (auto& entry : fs::recursive_directory_iterator(root))
	{
		if (!entry.is_regular_file())
			continue;

		std::string ext = entry.path().extension().string();

		// Normalize extension to lowercase
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

		for (const auto& wanted : extensions)
		{
			if (ext == wanted)
			{
				result.push_back(entry.path());
				break;
			}
		}
	}

	return result;
}

AssetBrowserPanel::AssetBrowserPanel()
{
	lookupAssets();
}

void AssetBrowserPanel::lookupAssets()
{
	for (auto& file : FindFilesRecursive(SCENE_DIRECTORY, { ".scene" , ".gltf", ".prefab" }))
	{
		AssetItem asset;
		asset.path = file.string();

		if (file.extension() == ".scene")
			asset.name = file.parent_path().filename().string();
		else
			asset.name = file.filename().string();

		if (file.extension() == ".scene")
			asset.type = AssetType::Scene;
		else if (file.extension() == ".prefab")
			asset.type = AssetType::Prefab;
		else
			asset.type = AssetType::Model;

		assets.push_back(asset);
	}
}

void AssetBrowserPanel::draw()
{
	drawTopBar();

	const float padding = 5.0f;      // spacing between cells
	const float iconSize = 54;      // size of the icon inside the cell
	const float cellSize = iconSize * 2;      // width & height of each grid cell

	float panelWidth = ImGui::GetContentRegionAvail().x;
	int columns = (int)(panelWidth / (cellSize + padding));
	if (columns < 1) columns = 1;

	int index = 0;

	for (const auto& asset : assets)
	{
		if (!passesFilter(asset))
			continue;

		ImGui::PushID(index);

		if (index % columns != 0)
			ImGui::SameLine();

		ImGui::BeginGroup();

		// Background button (full cell)
		ImGui::Button("##bg", ImVec2(cellSize, cellSize));

		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::TextUnformatted(asset.path.c_str());
			ImGui::EndTooltip();
		}

		// Drag-drop source (attached to the button)
		if (ImGui::BeginDragDropSource())
		{
			ImGui::SetDragDropPayload("ASSET_MODEL",
				asset.path.c_str(),
				asset.path.size() + 1);

			ImGui::Text("Add %s", asset.name.c_str());
			ImGui::EndDragDropSource();
		}

		// Get cell rect
		ImVec2 cellMin = ImGui::GetItemRectMin();
		ImVec2 cellMax = ImGui::GetItemRectMax();

		// Icon position (centered horizontally, slightly above center vertically)
		float iconY = cellMin.y + cellSize * 0.25f;
		float iconX = cellMin.x + (cellSize - iconSize) * 0.5f;

		ImGui::SetCursorScreenPos(ImVec2(iconX, iconY));

		const char* icon = ICON_FA_CUBE;
		if (asset.type == AssetType::Scene)   icon = ICON_FA_CUBES_STACKED;
		if (asset.type == AssetType::Prefab)  icon = ICON_FA_CUBES;

		ImGui::SetWindowFontScale(2.0f);
		ImGui::Text(icon);
		ImGui::SetWindowFontScale(1.0f);

		// Text position (centered horizontally, near bottom but not touching)
		float textY = cellMin.y + cellSize * 0.65f;
		float textX = cellMin.x + 4.0f;

		ImGui::SetCursorScreenPos(ImVec2(textX, textY));
		ImGui::PushTextWrapPos(cellMin.x + cellSize - 4.0f);
		ImGui::SetWindowFontScale(0.8f);
		ImGui::Text(asset.name.c_str());
		ImGui::SetWindowFontScale(1.0f);
		ImGui::PopTextWrapPos();

		ImGui::EndGroup();
		ImGui::PopID();

		index++;
	}
}

bool AssetBrowserPanel::passesFilter(const AssetItem& asset) const
{
	// Category filtering
	if (!filter.showModel && asset.type == AssetType::Model)
		return false;
	if (!filter.showScene && asset.type == AssetType::Scene)
		return false;
	if (!filter.showPrefab && asset.type == AssetType::Prefab)
		return false;

	// Search filtering
	if (filter.search[0] != '\0')
	{
		std::string nameLower = asset.name;
		std::string searchLower = filter.search;

		std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
		std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

		if (nameLower.find(searchLower) == std::string::npos)
			return false;
	}

	return true;
}

void AssetBrowserPanel::drawTopBar()
{
	ImGui::BeginGroup();

	// Search box
	ImGui::SetNextItemWidth(200);
	ImGui::InputTextWithHint("##search", "Search...", filter.search, sizeof(filter.search));

	ImGui::SameLine(0, 20);

	// Category toggles
	auto ToggleButton = [&](const char* label, bool& value)
		{
			bool wasValue = value;
			if (wasValue)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 1.0f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.4f, 0.9f, 1.0f));
			}

			if (ImGui::Button(label))
				value = !value;

			if (wasValue)
				ImGui::PopStyleColor(3);
		};

	ToggleButton("Model", filter.showModel);
	ImGui::SameLine();
	ToggleButton("Scene", filter.showScene);
	ImGui::SameLine();
	ToggleButton("Prefab", filter.showPrefab);

	ImGui::EndGroup();

	ImGui::Separator();
}
