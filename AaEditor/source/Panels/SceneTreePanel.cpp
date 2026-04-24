#include "SceneTreePanel.h"
#include "Editor/EditorSelection.h"
#include "imgui.h"
#include "../data/editor/icons/IconsFontAwesome7.h"

static bool NodeMatchesFilter(SceneGraphNode& node, ImGuiTextFilter& filter)
{
	// Does this node match?
	if (filter.PassFilter(node.name))
		return true;

	// Do any children match?
	for (auto& child : node.children)
		if (NodeMatchesFilter(child, filter))
			return true;

	return false;
}

SceneTreePanel::SceneTreePanel(SceneManager& sm, EditorSelection& sel) : sceneMgr(sm), selection(sel)
{
}

void SceneTreePanel::drawNode(SceneGraphNode& node, ImGuiTextFilter& filter, ObjectId& selectedObjectId)
{
	if (filter.IsActive() && !NodeMatchesFilter(node, filter))
		return;

	ImGuiTreeNodeFlags flags =
		ImGuiTreeNodeFlags_OpenOnArrow |
		ImGuiTreeNodeFlags_OpenOnDoubleClick |
		(node.children.empty() ? ImGuiTreeNodeFlags_Leaf : 0);

	if (filter.IsActive())
		ImGui::SetNextItemOpen(true);

	// Highlight if selected
	for (auto& id : selection.getIds())
	{
		if (node.id == id)
			flags |= ImGuiTreeNodeFlags_Selected;
	}

	const char* icon = !node.children.empty() ? ICON_FA_FOLDER : ICON_FA_CUBE;

	bool open = ImGui::TreeNodeEx(
		(void*)(intptr_t)node.id.value,
		flags,
		"%s %s", icon, node.name
	);

	// Handle selection
	if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
		selectedObjectId = node.id;
	if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && !(flags & ImGuiTreeNodeFlags_Selected)) // only select with right click, dont unselect
		selectedObjectId = node.id;

	if (ImGui::BeginPopupContextItem())
	{
		if (ImGui::MenuItem("Delete"))
		{
			if (node.children.empty())
				selection.remove(sceneMgr);
			else
				selection.removeNode(node, sceneMgr);
		}

		ImGui::EndPopup();
	}

	// Draw children
	if (open)
	{
		for (auto& child : node.children)
			drawNode(child, filter, selectedObjectId);

		ImGui::TreePop();
	}
}

void SceneTreePanel::draw()
{
	static ImGuiTextFilter filter;
	filter.Draw("Search");

	ObjectId selectedObjectId = 0xFFFFFFFF;

	float height = ImGui::GetContentRegionAvail().y * 0.5f;
	ImGui::BeginChild("SceneTreeRegion", ImVec2(0, height), true);

	for (auto& node : sceneMgr.graph.nodes)
	{
		drawNode(node, filter, selectedObjectId);
	}

	ImGui::EndChild();

	if (selectedObjectId.value != 0xFFFFFFFF)
		selection.select(selectedObjectId, ImGui::IsKeyDown(ImGuiKey_LeftCtrl), sceneMgr);
}
