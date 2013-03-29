#ifndef _AA_FILE_PARSER_
#define _AA_FILE_PARSER_

#include "AaMaterial.h"
#include "AaLogger.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include "StringFunctions.h"

struct textureInfo
{
	std::string file;
	std::string filtering;
};

struct materialInfo
{
	std::string name;
	std::string ps_ref;
	std::string vs_ref;
	std::string gs_ref;
	std::vector<textureInfo> vstextures;
	std::vector<textureInfo> pstextures;
	std::vector<std::string> psuavs;
	std::map<std::string,std::vector<float>> defaultParams;
	RS_DESC renderStateDesc;
};

struct shaderRefMaps
{
	std::map<std::string,shaderRef> vertexShaderRefs;
	std::map<std::string,shaderRef> pixelShaderRefs;
};

class AaMaterialFileParser
{
public:

	AaMaterialFileParser() {}

	shaderRefMaps parseAllShaderFiles(std::string directory, bool subFolders=false);
	shaderRefMaps parseShaderFile(std::string file);

	std::vector<materialInfo> parseAllMaterialFiles(std::string directory, bool subFolders=false);
	std::vector<materialInfo> parseMaterialFile(std::string file);

protected:

	//recognize constants names, get size of whole buffer
	//short getConstBufferSize(std::vector<std::string>* constants, BUFFER_TYPE type);
	StringFunctions strFunctions;
};

#endif