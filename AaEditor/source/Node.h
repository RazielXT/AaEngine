#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "imnodes/imnodes.h"
#include "ShaderGen.h"

class Node;

struct NodeSource
{
	int idCounter = 10;
};

struct Pin
{
	std::string name;
	ValueType type;

	Node* owner = nullptr;
	// only for input, output can be connected more times
	Pin* connectedTo = nullptr;
};

class Node
{
public:
	int id{};
	std::string displayName;

	std::vector<Pin> inputs;
	std::vector<Pin> outputs;

	Node() = default;
	virtual ~Node() = default;

	void SetId(NodeSource& src)
	{
		id = src.idCounter;
		src.idCounter += 10;
	}
	void SetId(int nodeId)
	{
		id = nodeId;
	}

	// Codegen: returns variable name for output pin index
	virtual std::string Emit(ShaderGenContext& ctx) = 0;

	virtual void Draw()
	{
		BeginDraw();
		DrawTitle();
		DrawPins();
		EndDraw();
	}

protected:

	void BeginDraw()
	{
		ImNodes::PushColorStyle(ImNodesCol_TitleBar, titleColor[0]);
		ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, titleColor[1]);
		ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, titleColor[2]);
		ImNodes::BeginNode(id);
	}
	void EndDraw()
	{
		ImNodes::EndNode();
		ImNodes::PopColorStyle();
		ImNodes::PopColorStyle();
		ImNodes::PopColorStyle();
	}
	void DrawTitle()
	{
		ImNodes::BeginNodeTitleBar();
		// 		ImGui::SetNextItemWidth(100.0f);
		// 		static char elementName[15]{};
		// 		ImGui::InputText("##simple node :)", elementName, 15);
		ImGui::TextUnformatted(displayName.c_str());
		ImNodes::EndNodeTitleBar();
	}
	void DrawPins()
	{
		DrawInputPins();
		DrawOutputPins();
	}
	void DrawInputPins()
	{
		int pinId = id + 1;
		for (auto& i : inputs)
		{
			ImNodes::BeginInputAttribute(pinId++);
			if (!i.name.empty())
				ImGui::Text(i.name.c_str());
			ImNodes::EndInputAttribute();
		}
	}
	void DrawOutputPins()
	{
		int pinId = id + 1 + (int)inputs.size();
		for (auto& o : outputs)
		{
			ImNodes::BeginOutputAttribute(pinId++);
			ImGui::Indent(40);
			if (!o.name.empty())
				ImGui::Text(o.name.c_str());
			ImNodes::EndOutputAttribute();
		}
	}

	UINT titleColor[3] = { IM_COL32(41, 74, 122, 255), IM_COL32(66, 100, 200, 255), IM_COL32(66, 150, 250, 255) };
	void setTitleColor(UINT r, UINT g, UINT b)
	{
		r = min(r, 150); g = min(g, 150); b = min(b, 150);
		titleColor[0] = IM_COL32(r, g, b, 255);
		titleColor[1] = IM_COL32(r * 1.3, g * 1.3, b * 1.3, 255);
		titleColor[2] = IM_COL32(r * 1.6, g * 1.6, b * 1.6, 255);
	}

	std::string EmitInput(Pin& input, ShaderGenContext& ctx)
	{
		if (!input.connectedTo)
			return "";

		auto it = ctx.pinVars.find(input.connectedTo);
		if (it != ctx.pinVars.end())
			return it->second;

		Node* node = input.connectedTo->owner;
		std::string var = node->Emit(ctx);
		ctx.pinVars[input.connectedTo] = var;
		return var;
	}
};
