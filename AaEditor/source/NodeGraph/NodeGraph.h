#pragma once

#include "Node.h"
#include "Nodes/ConstantNode.h"
#include "Nodes/MathNodes.h"
#include "Nodes/TextureNodes.h"
#include "Nodes/OutputNode.h"
#include <memory>
#include <vector>

class NodeGraph
{
public:

	std::unordered_map<int, std::unique_ptr<Node>> nodes;

	int CreateNode(std::unique_ptr<Node> n)
	{
		n->SetId(nodeSrc);

		auto id = n->id;
		nodes[id] = std::move(n);

		return id;
	}

	Node* GetNode(int id)
	{
		return nodes[id].get();
	}

	void RemoveNode(int id)
	{
		DestroyNodePins(id);
		nodes.erase(id);
	}

	struct Link
	{
		int id;
		int outputPinId;
		int inputPinId;
	};
	std::vector<Link> links;

	int GetPinNodeId(int pinId)
	{
		return pinId - (pinId % 10);
	}

	Pin* GetNodePin(int pinId)
	{
		int nodeId = GetPinNodeId(pinId);
		auto node = GetNode(nodeId);

		auto pinIdx = pinId - nodeId - 1;
		if (pinIdx < node->inputs.size())
			return &node->inputs[pinIdx];

		pinIdx -= (int)node->inputs.size();

		return &node->outputs[pinIdx];
	}

	void ConnectPins(int outputPinId, int inputPinId)
	{
		auto outputPin = GetNodePin(outputPinId);
		auto inputPin = GetNodePin(inputPinId);

		if (inputPin->connectedTo)
			DestroyLinksWithInputId(inputPinId);

		inputPin->connectedTo = outputPin;

		auto linkId = (outputPinId << 16) + inputPinId;
		links.emplace_back(linkId, outputPinId, inputPinId);
	}

	int GetLinkInputId(int linkId)
	{
		return linkId & 0xFFFF;
	}
	int GetLinkOutputId(int linkId)
	{
		return linkId >> 16;
	}

	void DestroyLinksWithInputId(int inputId)
	{
		for (auto it = links.begin(); it != links.end();)
		{
			if (GetLinkInputId(it->id) == inputId)
				it = links.erase(it);
			else
				it++;
		}
	}

	void DestroyLinksWithOutputId(int outputId)
	{
		for (auto it = links.begin(); it != links.end();)
		{
			if (GetLinkOutputId(it->id) == outputId)
			{
				auto pin = GetNodePin(GetLinkInputId(it->id));
				pin->connectedTo = nullptr;
				it = links.erase(it);
			}
			else
				it++;
		}
	}

	void DestroyLink(int linkId)
	{
		auto outputPinId = GetLinkOutputId(linkId >> 16);
		auto inputPinId = GetLinkInputId(linkId);

		GetNodePin(inputPinId)->connectedTo = nullptr;

		for (auto it = links.begin(); it != links.end(); it++)
		{
			if (it->id == linkId)
			{
				links.erase(it);
				break;
			}
		}
	}

	void DestroyNodePins(int nodeId)
	{
		auto node = GetNode(nodeId);

		int pinIdx = nodeId + 1;
		for (auto& pin : node->inputs)
		{
			DestroyLinksWithInputId(pinIdx++);
		}
		for (auto& pin : node->outputs)
		{
			DestroyLinksWithOutputId(pinIdx++);
		}
	}

protected:
	NodeSource nodeSrc;
};
