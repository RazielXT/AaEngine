#include "AaModelLoader.h"
#include "AaLogger.h"
#include "GlobalDefinitions.h"
#include <sys/stat.h>
#include <time.h>


AaModelLoader::AaModelLoader(AaRenderSystem* mRenderSystem)
{
	objParser=new ObjFileParser(mRenderSystem);
	ogreXmlParser=new OgreXMLFileParser(mRenderSystem);
	binaryModelLoader= new AaBinaryModelLoader(mRenderSystem);
}

AaModelLoader::~AaModelLoader()
{
	delete objParser;
	delete ogreXmlParser;
}

AaModelInfo* AaModelLoader::loadModel(std::string filename)
{
	AaModelInfo* model;

	filename.insert(0,MODEL_DIRECTORY);

	if(boost::ends_with(filename,".model"))
		model=binaryModelLoader->load(filename);
	else
	if(boost::ends_with(filename,".obj"))
		model=objParser->loadOBJ(filename);
	else
	if(boost::ends_with(filename,".xml"))
		model=ogreXmlParser->loadOgreXML(filename,true,true);
	else
	{
		std::string tempModelName = filename + ".model";
		std::string tempXmlName = filename + ".xml";
		struct stat attrib,attrib2;  //File attribute structs
		int r1 = stat( tempModelName.c_str(), &attrib);
		int r2 = stat( tempXmlName.c_str(), &attrib2);

		if(r1==0 && r2!=0)
		{
			model=binaryModelLoader->load(tempModelName);
		}
		else
		if(r1!=0 && r2==0)
		{
			model=ogreXmlParser->loadOgreXML(tempXmlName,true,true);
		}
		else
		if(r1==0 && r2==0)
		{
			if(attrib.st_mtime>attrib2.st_mtime)
				model=binaryModelLoader->load(tempModelName);
			else
				model=ogreXmlParser->loadOgreXML(tempXmlName,true,true);
		}
		else
		{
			AaLogger::getLogger()->writeMessage("ERROR Model "+filename+" not loaded, unknown type!");
			model=NULL;
		}
	}
	


	return model;
}