#include "Resources/Model/ModelResources.h"
#include "Utils/Logger.h"
#include "App/Directories.h"
#include <sys/stat.h>
#include "Resources/Model/OgreMeshFileParser.h"
#include <memory>
#include "Resources/Model/BinaryModelLoader.h"
#include <filesystem>

static ModelResources* instance = nullptr;
const std::string CoreGroup = "meshes/core";

ModelResources::ModelResources(RenderSystem& rs) : device(*rs.core.device)
{
	if (instance)
		throw std::exception("Duplicate AaModelResources");

	instance = this;

	ResourceUploadBatch batch(rs.core.device);
	batch.Begin();

	preloadFolder(batch, { CoreGroup, true });

	auto uploadResourcesFinished = batch.End(rs.core.commandQueue);
	uploadResourcesFinished.wait();
}

ModelResources::~ModelResources()
{
	for (auto it = groups.begin(); it != groups.end(); it++)
	{
		for (auto& m : it->models)
			delete m.second;
	}

	instance = nullptr;
}

VertexBufferModel* ModelResources::getModel(const std::string& filename, ResourceUploadBatch& batch, const ModelLoadContext& ctx)
{
	auto& models = getGroupLibrary(ctx);
	{
		auto it = models.find(filename);

		if (it != models.end())
			return it->second;
	}

	VertexBufferModel* m = loadModel(filename, batch, ctx);
	if (m)
		models[filename] = m;

	return m;
}

VertexBufferModel* ModelResources::getModel(const std::string& path, ResourceUploadBatch& batch)
{
	auto separator = path.find_last_of('/');

	if (separator != std::string::npos)
	{
		auto folder = path.substr(0, separator);
		auto filename = path.substr(separator + 1);

		return getModel(filename, batch, { folder });
	}

	return getCoreModel(path);
}

VertexBufferModel* ModelResources::getCoreModel(const std::string& filename)
{
	auto& models = getGroupLibrary({ CoreGroup });
	auto it = models.find(filename);

	return it == models.end() ? nullptr : it->second;
}

void ModelResources::addLoadedModel(const std::string& name, VertexBufferModel* m, const std::string& group)
{
	auto& models = getGroupLibrary({ group });
	auto& model = models[name];

	if (model)
		Logger::logError("Duplicate mesh " + name);

	model = m;
}

VertexBufferModel* ModelResources::addLoadedModel(const std::string& name, const std::string& group)
{
	auto& models = getGroupLibrary({ group });
	auto& model = models[name];

	if (model)
		return nullptr;

	return model = new VertexBufferModel();
}

UINT ModelResources::preloadFolder(ResourceUploadBatch& batch, const ModelLoadContext& ctx)
{
	UINT c{};
	std::error_code ec;

	for (const auto& entry : std::filesystem::directory_iterator(SCENE_DIRECTORY + ctx.folder, ec))
	{
		if (entry.is_regular_file())
		{
			getModel(entry.path().filename().generic_string(), batch, ctx);
			c++;
		}
	}

	return c;
}

void ModelResources::clear()
{
	for (auto it = groups.begin(); it != groups.end();)
	{
		if (it->persistent)
			it++;
		else
		{
			for (auto& m : it->models)
				delete m.second;

			it = groups.erase(it);
		}
	}
}

VertexBufferModel* ModelResources::loadModel(const std::string& filename, ResourceUploadBatch& batch, const ModelLoadContext& ctx)
{
	VertexBufferModel* model{};

	Logger::log("Loading mesh file " + filename);

	std::string filepath = SCENE_DIRECTORY + ctx.folder;
	if (!filepath.ends_with('\\')) filepath += '\\';
	filepath += filename;

	if (filename.ends_with("mesh"))
	{
		ModelParseOptions o;
		o.batch = &batch;
		o.device = &device;

		struct stat attrib;
		if (stat(filepath.c_str(), &attrib) == 0)
		{
			auto mesh = OgreMeshFileParser::load(filepath, o);

			if (!mesh.submeshes.empty())
			{
				model = mesh.submeshes[0].model;
				model->bbox = mesh.boundingBox;
			}
		}
		else if (stat((filepath + ".model").c_str(), &attrib) == 0)
		{
			model = BinaryModelLoader::load(filepath + ".model", o);
		}
	}

	if (!model)
		Logger::logError("Model " + filename + " failed to load");

	return model;
}

ModelResources::ModelLibrary& ModelResources::getGroupLibrary(const ModelLoadContext& ctx)
{
	for (auto& g : groups)
	{
		if (g.name == ctx.folder)
			return g.models;
	}

	return groups.emplace_back(ctx.folder, ctx.persistent).models;
}
