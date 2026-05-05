#include "RenderObject/Prefab/PrefabLoader.h"
#include "Utils/ConfigParser.h"
#include "Resources/GraphicsResources.h"
#include "Scene/RenderWorld.h"
#include "Utils/Logger.h"
#include <format>

void PrefabLoader::load(const std::string& path, SceneCollection::LoadCtx ctx)
{
	auto objects = Config::Parse(path);

	for (auto& obj : objects)
	{
		auto& name = obj.value;
		MaterialInstance* material{};
		VertexBufferModel* model{};

		ObjectTransformation tr = ctx.placement;

		for (auto& param : obj.children)
		{
			if (param.type == "scale")
			{
				if (param.params.empty())
					tr.scale = Vector3(std::stof(param.value));
				else if (param.params.size() == 2)
					tr.scale = Vector3(std::stof(param.value), std::stof(param.params[0]), std::stof(param.params[1]));
			}
			else if (param.type == "material")
			{
				material = ctx.resources.materials.getMaterial(param.value, ctx.batch);
			}
			else if (param.type == "model")
			{
				model = ctx.resources.models.getModel(param.value, ctx.batch);
			}
		}

		if (name.empty() || !material || !model)
		{
			Logger::logWarning(std::format("Failed to load prefab {} {}", path, obj.value));
			continue;
		}

		auto e = ctx.renderWorld.createEntity(tr, *model);
		e->material = material;
	}
}
