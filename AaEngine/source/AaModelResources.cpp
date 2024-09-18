#include "AaModelResources.h"
#include "AaLogger.h"
#include "Directories.h"
#include <sys/stat.h>
#include "AaBinaryModelLoader.h"
#include "OgreXMLFileParser.h"
#include "OgreMeshFileParser.h"

AaModelResources* instance = nullptr;

AaModelResources::AaModelResources(AaRenderSystem* mRS)
{
	mRenderSystem = mRS;

	if (instance)
		throw std::exception("Duplicate AaModelResources");

	instance = this;
}

AaModelResources::~AaModelResources()
{
	for (auto& it : mModelMap)
		delete it.second;

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
			auto mesh = OgreMeshFileParser::load(filepath, { mRenderSystem });

			if (!mesh.submeshes.empty())
			{
				model = mesh.submeshes[0].model;
				model->bbox = mesh.boundingBox;
			}
		}
	}

	if (!model && filepath.ends_with("mesh"))
	{
		std::string tempModelName = filepath + ".model";
		std::string tempXmlName = filepath + ".xml";
		struct stat attrib, attrib2;
		int r1 = stat(tempModelName.c_str(), &attrib);
		int r2 = stat(tempXmlName.c_str(), &attrib2);

		if (r1 != r2)
			filepath = (r1 == 0) ? tempModelName : tempXmlName;
		else if (r1 == 0 && r2 == 0)
			filepath = (attrib.st_mtime > attrib2.st_mtime) ? tempModelName : tempXmlName;
	}

	if (filepath.ends_with("model"))
		model = AaBinaryModelLoader::load(filepath, mRenderSystem);
	if (filepath.ends_with("xml"))
		model = OgreXMLFileParser::load(filepath, mRenderSystem, true, true);

 	if (!model)
 		AaLogger::logError("Model " + filename + " failed to load");

	return model;
}
