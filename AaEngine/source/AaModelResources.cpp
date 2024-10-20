#include "AaModelResources.h"
#include "AaLogger.h"
#include "Directories.h"
#include <sys/stat.h>
#include "OgreMeshFileParser.h"
#include <memory>
#include "BinaryModelLoader.h"

static AaModelResources* instance = nullptr;

AaModelResources::AaModelResources(AaRenderSystem* mRS)
{
	mRenderSystem = mRS;

	if (instance)
		throw std::exception("Duplicate AaModelResources");

	instance = this;
}

AaModelResources::~AaModelResources()
{
	for (auto& it : loadedModels)
		delete it.second;

	instance = nullptr;
}

AaModelResources& AaModelResources::get()
{
	return *instance;
}

AaModel* AaModelResources::getModel(const std::string& filename, ModelLoadContext& ctx)
{
	auto it = loadedModels.find(filename);

	if (it == loadedModels.end())
	{
		AaModel* m = loadModel(filename, ctx);
		if (m)
			loadedModels[filename] = m;

		return m;
	}

	return it->second;
}

void AaModelResources::clear()
{
	for (auto& it : loadedModels)
		delete it.second;

	loadedModels.clear();
}

AaModel* AaModelResources::loadModel(const std::string& filename, ModelLoadContext& ctx)
{
	AaModel* model{};

	AaLogger::log("Loading mesh file " + filename);

	std::string filepath = ctx.folder + filename;

	if (filename.ends_with("mesh"))
	{
		ModelParseOptions o;
		o.batch = &ctx.batch;
		o.device = mRenderSystem->device;

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
		AaLogger::logError("Model " + filename + " failed to load");

	return model;
}

ModelLoadContext::ModelLoadContext(AaRenderSystem* rs) : batch(rs->device)
{
}
