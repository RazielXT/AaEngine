#ifndef _AA_MODEL_LOADER_
#define _AA_MODEL_LOADER_

#include <string>
#include "AaRenderSystem.h"
#include "ObjFileParser.h"
#include "OgreXMLFileParser.h"
#include "AaBinaryModelLoader.h"

class AaModelLoader
{
public:
	AaModelLoader(AaRenderSystem* mRenderSystem);
	~AaModelLoader();

	AaModelInfo* loadModel(std::string filename);

private:
	ObjFileParser* objParser;
	OgreXMLFileParser* ogreXmlParser;
	AaBinaryModelLoader* binaryModelLoader;
};

#endif