#pragma once

#include "Utils/MathUtils.h"
#include "Scene/RenderObject.h"
#include <optional>
#include <string>

struct SceneSnapshot
{
	bool save(const std::string& path) const;
	bool load(const std::string& path);

	struct Camera
	{
		Vector3 position;
		Vector3 direction;
	};

	std::optional<Camera> camera;

	struct Entity
	{
		std::string name;
		ObjectTransformation tr;
	};
	struct Resource
	{
		std::string file;
		std::vector<Entity> entities;
	};

	std::vector<Resource> resources;

	struct Environment
	{
		float timeOfDay;
	};

	Environment environment;
};