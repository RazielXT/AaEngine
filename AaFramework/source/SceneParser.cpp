#pragma once

#include "SceneParser.h"
#include "Directories.h"
#include "FileLogger.h"
#include "GraphicsResources.h"
#include <filesystem>
#include <algorithm>

#define PUGIXML_HEADER_ONLY
#include "pugixml.hpp"

using namespace pugi;

static SceneParser::Ctx* ctx{};
static SceneParser::Result* parseResult{};
static std::string loadFolder;

struct SceneNode
{
	ObjectTransformation transformation;
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

Order ParseRenderQueue(std::string_view renderQueue)
{
	if (renderQueue == "skiesEarly")
		return Order(5);

	if (renderQueue == "Water")
		return Order::Transparent;

	if (auto q = strtoul(renderQueue.data(), 0, 10))
	{
		if (q <= 100)
			return Order(q);
	}

	return Order::Normal;
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

struct ExtensionFunction
{
	std::string name;
	std::map<std::string, std::string> parameters;
};

std::vector<ExtensionFunction> parseExtensionFunctions(const std::string& input)
{
	std::vector<ExtensionFunction> functions;
	std::stringstream ss(input);
	std::string line;

	while (std::getline(ss, line, ';'))
	{
		std::stringstream lineStream(line);
		std::string functionName;
		std::getline(lineStream, functionName, '(');

		std::string params;
		std::getline(lineStream, params, ')');

		ExtensionFunction func;
		func.name = functionName;

		std::stringstream paramStream(params);
		std::string param;

		while (std::getline(paramStream, param, ','))
		{
			param.erase(std::remove_if(param.begin(), param.end(), ::isspace), param.end());

			size_t colonPos = param.find(':');
			if (colonPos != std::string::npos)
			{
				auto type = param.substr(0, colonPos);
				auto value = param.substr(colonPos + 1);

				func.parameters[type] = value;
			}
			else
			{
				func.parameters[std::to_string(func.parameters.size())] = param;
			}
		}

		functions.push_back(func);
	}

	return functions;
}

void loadExtensions(const xml_node& element, SceneEntity* ent, SceneNode* node)
{
	auto extensions = parseExtensionFunctions(element.text().get());

	for (auto& e : extensions)
	{
		if (e.name == "addGrass")
		{
			//parseResult->grassTasks.emplace_back(ent, ent->getWorldBoundingBox());
		}
	}
}

void loadEntityUserData(const xml_node& element, SceneEntity* ent, SceneNode* node)
{
	pugi::xml_document subdoc;
	if (subdoc.load_string(element.text().get()))
	{
		if (auto physicalElement = subdoc.child("PhysicalBody"))
		{
			if (auto extensionElement = physicalElement.child("Extension"))
				loadExtensions(extensionElement, ent, node);

			std::string_view type = physicalElement.child("BodyType").child_value();
			auto mass = physicalElement.child("Mass").text().as_float();

			if (type == "box")
			{
				auto bbox = ent->getBoundingBox();

				if (mass == 0)
					ctx->physicsMgr.createStaticBox(bbox.Extents, node->transformation, bbox.Center);
				else
				{
					auto id = ctx->physicsMgr.createDynamicBox(bbox.Extents, node->transformation, bbox.Center, mass);
					ctx->physicsMgr.dynamicBodies.push_back({ ent, id });
				}
			}
			else if (type == "sphere")
			{
				auto radius = ent->getBoundingBox().Extents.x;

				auto id = ctx->physicsMgr.createDynamicSphere(radius, node->transformation.position, Vector3::Zero);
				ctx->physicsMgr.dynamicBodies.push_back({ ent, id });
			}
			else if (type == "conv")
			{
				auto model = ent->geometry.getModel();
				auto id = ctx->physicsMgr.createConvexBody(model->positions, node->transformation, mass);
				ctx->physicsMgr.dynamicBodies.push_back({ ent, id });
			}
			else if (type == "tree")
			{
				auto model = ent->geometry.getModel();
				ctx->physicsMgr.createMeshBody(model->positions, model->indices, node->transformation);
			}
		}
	}
}

void loadEntity(const xml_node& entityElement, SceneNode* node, bool visible)
{
	auto name = GetStringAttribute(entityElement, "name");
	auto mesh = GetStringAttribute(entityElement, "meshFile");
	auto renderQueue = ParseRenderQueue(entityElement.attribute("renderQueue").value());

	FileLogger::log("Loading entity " + name + " with mesh file " + mesh);
	SceneEntity* ent{};

	for (const auto& element : entityElement.child("subentities").children())
	{
		auto materialName = GetStringAttribute(element, "materialName");
		auto material = ctx->resources.materials.getMaterial(materialName, ctx->batch);
		auto model = ctx->resources.models.getModel(mesh, ctx->batch, { ResourceGroup::General, loadFolder });

		if (material->HasInstancing())
		{
			auto& instanceDescription = parseResult->instanceDescriptions[material];
			instanceDescription.material = material;
			instanceDescription.model = model;
			instanceDescription.objects.emplace_back(node->transformation);

			break;
		}

		if (material->IsTransparent() && renderQueue < Order::Transparent)
			renderQueue = Order::Transparent;

		ent = ctx->sceneMgr.createEntity(name, node->transformation, *model, renderQueue);
		ent->material = material;
		//ent->setVisible(visible);

		break;
	}

	if (const auto& element = entityElement.child("userData"))
	{
		loadEntityUserData(element, ent, node);
	}
}

void loadNode(const xml_node& nodeElement, SceneNode* parentNode = nullptr)
{
	SceneNode node;

	if (!parentNode)
	{
		node.transformation.position = LoadXYZ(nodeElement.child("position"));
		node.transformation.orientation = LoadRotation(nodeElement.child("rotation"));
		node.transformation.scale = LoadXYZ(nodeElement.child("scale"));
	}
	else
		node = *parentNode;

	auto name = nodeElement.attribute("name").value();
	bool visible = nodeElement.attribute("visibility").value() != std::string("hidden");

	for (const auto& element : nodeElement.children())
	{
		std::string elementName = element.name();

		if (elementName == "entity")
			loadEntity(element, &node, visible);
		if (elementName == "node")
			loadNode(element, &node);
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

SceneParser::Result SceneParser::load(std::string name, Ctx parseCtx)
{
	SceneParser::Result result;
	parseResult = &result;
	ctx = &parseCtx;

	loadFolder = SCENE_DIRECTORY + name + "/";
	auto filename = findFileWithExtension(loadFolder, ".scene");

	if (filename.empty())
		return result;

	if (xml_document doc; doc.load_file(filename.c_str()))
	{
		const auto& scene = doc.child("scene");

		for (const auto& node : scene.child("nodes").children())
		{
			loadNode(node);
		}
	}

	parseCtx.sceneMgr.skybox.setMaterial("Sky", parseCtx.sceneMgr.getQueue(MaterialTechnique::Default, Order::Post)->targetFormats);

	return result;
}
