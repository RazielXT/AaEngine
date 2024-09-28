#include "AaModelResources.h"
#include "AaLogger.h"
#include "Directories.h"
#include <sys/stat.h>
#include "OgreMeshFileParser.h"
#include <memory>

static AaModelResources* instance = nullptr;

std::unique_ptr<GraphicsMemory> graphicsMemory;

AaModelResources::AaModelResources(AaRenderSystem* mRS)
{
	mRenderSystem = mRS;
	graphicsMemory = std::make_unique<GraphicsMemory>(mRenderSystem->device);

	if (instance)
		throw std::exception("Duplicate AaModelResources");

	instance = this;
}

AaModelResources::~AaModelResources()
{
	for (auto& it : mModelMap)
		delete it.second;

	graphicsMemory.reset();

	instance = nullptr;
}

AaModelResources& AaModelResources::get()
{
	return *instance;
}

AaModel* AaModelResources::getModel(const std::string& filename)
{
	auto it = mModelMap.find(filename);

	if (it == mModelMap.end())
	{
		AaModel* m = loadModel(filename);
		if (m)
			mModelMap[filename] = m;

		return m;
	}

	return it->second;
}

AaModel* AaModelResources::loadModel(const std::string& filename)
{
	AaModel* model{};

	AaLogger::log("Loading mesh file " + filename);

	std::string filepath = MODEL_DIRECTORY + filename;

	if (filepath.ends_with("mesh"))
	{
		struct stat attrib;
		if (stat(filepath.c_str(), &attrib) == 0)
		{
			ResourceUploadBatch resourceUpload(mRenderSystem->device);
			resourceUpload.Begin();

			OgreMeshFileParser::ParseOptions o;
			o.batch = &resourceUpload;
			o.device = mRenderSystem->device;

			auto mesh = OgreMeshFileParser::load(filepath, o);

			if (!mesh.submeshes.empty())
			{
				model = mesh.submeshes[0].model;
				model->bbox = mesh.boundingBox;
			}

			auto uploadResourcesFinished = resourceUpload.End(mRenderSystem->commandQueue);
			uploadResourcesFinished.wait();
		}
	}

	if (!model)
		AaLogger::logError("Model " + filename + " failed to load");

	return model;
}
