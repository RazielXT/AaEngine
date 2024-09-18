#pragma once

#include "AaSceneParser.h"
#include <algorithm>
#include "Xml.h"
#include "Directories.h"
#include "AaLogger.h"

struct SceneNode
{
	XMFLOAT3 position;
	XMFLOAT4 orientation;
	XMFLOAT3 scale;
};

const XmlParser::Element* IterateChildElements(const XmlParser::Element* xmlElement, const XmlParser::Element* childElement)
{
	if (xmlElement != nullptr)
	{
		if (childElement == nullptr)
			childElement = xmlElement->firstNode();
		else
			childElement = childElement->nextSibling();

		return childElement;
	}

	return nullptr;
}

bool getBool(std::string_view value)
{
	if (value == "false" || value == "no" || value == "0")
		return false;

	return true;
}

float GetFloatAttribute(const XmlParser::Element* xmlElement, const char* name, float defaultValue = 0)
{
	return xmlElement->attributeAs<float>(name, defaultValue);
}

std::string GetStringAttribute(const XmlParser::Element* xmlElement, const char* name)
{
	auto strView = xmlElement->attribute(name);
	return { strView.begin(), strView.end() };
}

XMFLOAT3 LoadXYZ(const XmlParser::Element* objectElement)
{
	XMFLOAT3 xyz;
	xyz.x = GetFloatAttribute(objectElement, "x");
	xyz.y = GetFloatAttribute(objectElement, "y");
	xyz.z = GetFloatAttribute(objectElement, "z");
	return xyz;
}

LightType ParseLightType(std::string_view type)
{
	std::string typeLower(type.begin(), type.end());
	std::transform(typeLower.begin(), typeLower.end(), typeLower.begin(), ::tolower);

	if (typeLower == "point")
		return LightType_Point;
	if (typeLower == "directional")
		return LightType_Directional;
	if (typeLower == "spot")
		return LightType_Spotlight;

	return LightType_Point;
}

XMFLOAT4 LoadRotation(const XmlParser::Element* objectElement)
{
	XMFLOAT4 rotation(0, 0, 0, 1);

	rotation.x = GetFloatAttribute(objectElement, "qx", 1);
	rotation.y = GetFloatAttribute(objectElement, "qy", 0);
	rotation.z = GetFloatAttribute(objectElement, "qz", 1);
	rotation.w = GetFloatAttribute(objectElement, "qw", 0);

	return rotation;
}

XMFLOAT3 LoadColor(const XmlParser::Element* objectElement)
{
	XMFLOAT3 color;
	color.x = GetFloatAttribute(objectElement, "r", 0);
	color.y = GetFloatAttribute(objectElement, "g", 0);
	color.z = GetFloatAttribute(objectElement, "b", 0);
	return color;
}

bool AllDigits(std::string text)
{
	for (char i : text)
	{
		if (!isdigit(i))
			return false;
	}

	return true;
}

uint8_t ParseRenderQueue(std::string_view renderQueue)
{
	if (renderQueue == "skiesEarly")
		return 0;

	if (renderQueue == "main")
		return 5;

	if (renderQueue == "queue6")
		return 6;

	if (auto q = strtoul(renderQueue.data(), 0, 10))
	{
		if (q < 10)
			return q;
	}

	return 5;
}

void loadLight(const XmlParser::Element* lightElement, SceneNode* node, AaSceneManager* mSceneMgr)
{
	Light light{};
	light.type = ParseLightType(lightElement->attribute("type"));
	light.position = node->position;
	XMFLOAT3 unitV(0, 0, 1);
	XMVECTOR v = XMVector3Rotate(XMLoadFloat3(&unitV), XMLoadFloat4(&node->orientation));
	XMStoreFloat3(&light.direction, v);

	const XmlParser::Element* childElement = {};
	while (childElement = IterateChildElements(lightElement, childElement))
	{
		auto elementName = childElement->value();

		if (elementName == "colourDiffuse")
			light.color = LoadColor(childElement);
	}
}

void loadEntity(const XmlParser::Element* entityElement, SceneNode* node, bool visible, AaSceneManager* mSceneMgr)
{
	auto name = GetStringAttribute(entityElement, "name");
	auto file = GetStringAttribute(entityElement, "meshFile");
	auto renderQueue = ParseRenderQueue(entityElement->attribute("renderQueue"));

	AaLogger::log("Loading entity " + name + " with mesh file " + file);

	auto subentityElement = entityElement->firstNode("subentities");
	const XmlParser::Element* childElement = nullptr;
	while (childElement = IterateChildElements(subentityElement, childElement))
	{
		auto ent = mSceneMgr->createEntity(name, GetStringAttribute(childElement, "materialName"), renderQueue);
		ent->setModel(file);
		ent->setPosition(node->position);
		ent->setScale(node->scale);
		ent->setQuaternion(node->orientation);
		ent->visible = visible;

		break;
	}
}

void loadNode(const XmlParser::Element* nodeElement, AaSceneManager* mSceneMgr)
{
	SceneNode node;
	auto name = nodeElement->attribute("name");
	bool visible = nodeElement->attribute("visibility") != "hidden";

	//first properties, then content
	const XmlParser::Element* childElement = nullptr;

	while (childElement = IterateChildElements(nodeElement, childElement))
	{
		auto elementName = childElement->name;

		if (elementName == "position")
			node.position = (LoadXYZ(childElement));
		else if (elementName == "rotation")
			node.orientation = (LoadRotation(childElement));
		else if (elementName == "scale")
			node.scale = (LoadXYZ(childElement));
	}

	childElement = nullptr;
	while (childElement = IterateChildElements(nodeElement, childElement))
	{
		auto elementName = childElement->name;

		if (elementName == "entity")
			loadEntity(childElement, &node, visible, mSceneMgr);
		else if (elementName == "light")
			loadLight(childElement, &node, mSceneMgr);
	}
}

void SceneParser::load(std::string filename, AaSceneManager* mSceneMgr)
{
	filename.insert(0, MODEL_DIRECTORY);

	XmlParser::Document document;
	document.load(filename.c_str());
	auto mainElement = document.getRoot();
	auto nodesElement = mainElement->firstNode("nodes");

	const XmlParser::Element* childElement = {};
	while (childElement = IterateChildElements(nodesElement, childElement))
	{
		auto elementName = childElement->name;

		if (elementName == "node")
			loadNode(childElement, mSceneMgr);
	}
}
