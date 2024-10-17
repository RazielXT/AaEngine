#pragma once

#include "SceneParser.h"
#include <algorithm>
#include "Directories.h"
#include "AaLogger.h"
#include <filesystem>

#define PUGIXML_HEADER_ONLY
#include "pugixml.hpp"
#include "AaModelResources.h"
#include "AaMaterialResources.h"

using namespace pugi;

static ModelLoadContext* ctx{};

struct SceneNode
{
	XMFLOAT3 position;
	XMFLOAT4 orientation;
	XMFLOAT3 scale;
};

bool getBool(std::string_view value)
{
	if (value == "false" || value == "no" || value == "0")
		return false;

	return true;
}

float GetFloatAttribute(const xml_node& node, const char* name, float defaultValue = 0)
{
	return node.attribute(name).as_float(defaultValue);
}

std::string GetStringAttribute(const xml_node& node, const char* name)
{
	return node.attribute(name).value();
}

XMFLOAT3 LoadXYZ(const xml_node& node)
{
	XMFLOAT3 xyz;
	xyz.x = GetFloatAttribute(node, "x");
	xyz.y = GetFloatAttribute(node, "y");
	xyz.z = GetFloatAttribute(node, "z");
	return xyz;
}

// LightType ParseLightType(std::string_view type)
// {
// 	std::string typeLower(type.begin(), type.end());
// 	std::transform(typeLower.begin(), typeLower.end(), typeLower.begin(), ::tolower);
// 
// 	if (typeLower == "point")
// 		return LightType_Point;
// 	if (typeLower == "directional")
// 		return LightType_Directional;
// 	if (typeLower == "spot")
// 		return LightType_Spotlight;
// 
// 	return LightType_Point;
// }

XMFLOAT4 LoadRotation(const xml_node& node)
{
	XMFLOAT4 rotation(0, 0, 0, 1);

	rotation.x = GetFloatAttribute(node, "qx", 1);
	rotation.y = GetFloatAttribute(node, "qy", 0);
	rotation.z = GetFloatAttribute(node, "qz", 1);
	rotation.w = GetFloatAttribute(node, "qw", 0);

	return rotation;
}

XMFLOAT3 LoadColor(const xml_node& node)
{
	XMFLOAT3 color;
	color.x = GetFloatAttribute(node, "r", 0);
	color.y = GetFloatAttribute(node, "g", 0);
	color.z = GetFloatAttribute(node, "b", 0);
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

int ParseRenderQueue(std::string_view renderQueue)
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

// void loadLight(const XmlParser::Element* lightElement, SceneNode* node, AaSceneManager* sceneMgr)
// {
// 	Light light{};
// 	light.type = ParseLightType(lightElement->attribute("type"));
// 	light.position = node->position;
// 	XMFLOAT3 unitV(0, 0, 1);
// 	XMVECTOR v = XMVector3Rotate(XMLoadFloat3(&unitV), XMLoadFloat4(&node->orientation));
// 	XMStoreFloat3(&light.direction, v);
// 
// 	const XmlParser::Element* childElement = {};
// 	while (childElement = IterateChildElements(lightElement, childElement))
// 	{
// 		auto elementName = childElement->value();
// 
// 		if (elementName == "colourDiffuse")
// 			light.color = LoadColor(childElement);
// 	}
// }

std::map<MaterialInstance*, InstanceGroupDescription> instanceDescriptions;

void loadEntity(const xml_node& entityElement, SceneNode* node, bool visible, AaSceneManager* sceneMgr)
{
	auto name = GetStringAttribute(entityElement, "name");
	auto mesh = GetStringAttribute(entityElement, "meshFile");
	auto renderQueue = ParseRenderQueue(entityElement.attribute("renderQueue").value());

	AaLogger::log("Loading entity " + name + " with mesh file " + mesh);

	for (const auto& element : entityElement.child("subentities").children())
	{
		auto materialName = GetStringAttribute(element, "materialName");
		auto material = AaMaterialResources::get().getMaterial(materialName, ctx->batch);
		auto model = AaModelResources::get().getModel(mesh, *ctx);

		if (material->HasInstancing())
		{
			auto& instanceDescription = instanceDescriptions[material];
			instanceDescription.material = material;
			instanceDescription.model = model;
			instanceDescription.objects.emplace_back(node->orientation, node->position, node->scale);

			break;
		}

		auto ent = sceneMgr->createEntity(name);
		ent->material = material;
		ent->setModel(model);
		ent->setPosition(node->position);
		ent->setScale(node->scale);
		ent->setOrientation(node->orientation);
		//ent->setVisible(visible);

		break;
	}
}

void loadNode(const xml_node& nodeElement, AaSceneManager* sceneMgr)
{
	SceneNode node;
	node.position = LoadXYZ(nodeElement.child("position"));
	node.orientation = LoadRotation(nodeElement.child("rotation"));
	node.scale = LoadXYZ(nodeElement.child("scale"));

	auto name = nodeElement.attribute("name").value();
	bool visible = nodeElement.attribute("visibility").value() != std::string("hidden");

	for (const auto& element : nodeElement.children())
	{
		std::string elementName = element.name();

		if (elementName == "entity")
			loadEntity(element, &node, visible, sceneMgr);
// 		else if (elementName == "light")
// 			loadLight(childElement, &node, sceneMgr);
	}
}

static std::string findFileWithExtension(const std::string& directoryPath, const std::string& extension)
{
	std::error_code ec;
	for (const auto& entry : std::filesystem::directory_iterator(directoryPath, ec))
	{
		if (entry.is_regular_file() && entry.path().extension() == extension)
		{
			return entry.path().string();
		}
	}
	return {};
}

void SceneParser::load(std::string name, AaSceneManager* sceneMgr, AaRenderSystem* renderSystem)
{
	auto folder = SCENE_DIRECTORY + name + "/";
	auto filename = findFileWithExtension(folder, ".scene");

	if (filename.empty())
		return;

	xml_document doc;
	xml_parse_result result = doc.load_file(filename.c_str());

	if (result)
	{
		ModelLoadContext loadCtx(renderSystem);
		ctx = &loadCtx;
		loadCtx.folder = folder;
		loadCtx.batch.Begin();

		const auto& scene = doc.child("scene");

		for (const auto& node : scene.child("nodes").children())
		{
			loadNode(node, sceneMgr);
		}

		auto uploadResourcesFinished = loadCtx.batch.End(renderSystem->commandQueue);
		uploadResourcesFinished.wait();
	}

	for (auto& desc : instanceDescriptions)
	{
		sceneMgr->instancing.build(sceneMgr, desc.second);
	}
	instanceDescriptions.clear();
}
