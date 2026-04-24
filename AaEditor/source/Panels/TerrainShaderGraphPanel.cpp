#include "TerrainShaderGraphPanel.h"
#include "imgui.h"
#include "imnodes/imnodes.h"
#include "NodeGraph/NodeGraph.h"
#include "Resources/Shader/ShaderDefines.h"

TerrainShaderGraphPanel::TerrainShaderGraphPanel(ShaderDefines& sd) : shaderDefines(sd)
{
}

void TerrainShaderGraphPanel::draw()
{
	static NodeGraph graph;

	static int selectedNodesCount{};
	static int selectedLinksCount{};
	selectedNodesCount = ImNodes::NumSelectedNodes();
	selectedLinksCount = ImNodes::NumSelectedLinks();

	ImNodes::BeginNodeEditor();

	if (ImGui::BeginPopupContextWindow("node_editor_context", ImGuiPopupFlags_MouseButtonRight))
	{
		if (ImGui::BeginMenu("Add"))
		{
			int addedNodeId{};

			if (ImGui::BeginMenu("Constant"))
			{
				if (ImGui::MenuItem("Float"))
					addedNodeId = graph.CreateNode(std::make_unique<ConstantFloatNode>());
				else if (ImGui::MenuItem("Swizzle"))
					addedNodeId = graph.CreateNode(std::make_unique<SwizzleNode>());

				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Math"))
			{
				if (ImGui::MenuItem("Add"))
					addedNodeId = graph.CreateNode(std::make_unique<AddNode>());
				else if (ImGui::MenuItem("Multiply"))
					addedNodeId = graph.CreateNode(std::make_unique<MulNode>());
				else if (ImGui::MenuItem("Sin"))
					addedNodeId = graph.CreateNode(std::make_unique<SinNode>());
				else if (ImGui::MenuItem("Pow"))
					addedNodeId = graph.CreateNode(std::make_unique<PowNode>());
				else if (ImGui::MenuItem("Saturate"))
					addedNodeId = graph.CreateNode(std::make_unique<SaturateNode>());

				ImGui::EndMenu();
			}
			if (ImGui::MenuItem("Texture"))
				addedNodeId = graph.CreateNode(std::make_unique<TextureSampleNode>());
			if (ImGui::MenuItem("Output"))
				addedNodeId = graph.CreateNode(std::make_unique<FloatOutputNode>([this](std::string func)
					{
						shaderDefines.setDefine("HEIGHT_FUNC", func);
					}));

			if (addedNodeId)
			{
				ImNodes::SetNodeScreenSpacePos(addedNodeId, ImGui::GetWindowPos());
				ImNodes::SnapNodeToGrid(addedNodeId);
			}

			ImGui::EndMenu();
		}
		if ((selectedNodesCount || selectedLinksCount ) && ImGui::MenuItem("Delete"))
		{
			if (selectedNodesCount)
			{
				std::vector<int> selection(selectedNodesCount);
				ImNodes::GetSelectedNodes(selection.data());
				ImNodes::ClearNodeSelection();

				for (auto& s : selection)
				{
					graph.RemoveNode(s);
				}
			}
			if (selectedLinksCount)
			{
				std::vector<int> selection(selectedLinksCount);
				ImNodes::GetSelectedLinks(selection.data());
				ImNodes::ClearLinkSelection();

				for (auto& s : selection)
				{
					graph.DestroyLink(s);
				}
			}
		}

		ImGui::EndPopup();
	}

	for (auto& node : graph.nodes)
	{
		node.second->Draw();
	}

	for (auto& link : graph.links)
	{
		ImNodes::Link(link.id, link.outputPinId, link.inputPinId);
	}

	ImNodes::EndNodeEditor();

	{
		int start_attr, end_attr;
		if (ImNodes::IsLinkCreated(&start_attr, &end_attr))
		{
			graph.ConnectPins(start_attr, end_attr);
		}
	}
}
