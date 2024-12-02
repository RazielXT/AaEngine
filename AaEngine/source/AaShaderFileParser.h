#pragma once

#include <map>
#include <string>
#include <vector>
#include "ConfigParser.h"

enum ShaderType
{
	ShaderTypeVertex,
	ShaderTypePixel,
	ShaderTypeCompute,
	ShaderType_COUNT,
	ShaderTypeNone
};

struct ConstantDefault
{
	std::string name;
	std::vector<float> data;
};

struct ShaderRef
{
	std::string file;
	std::string entry;
	std::string profile;
	std::vector<std::pair<std::string, std::string>> defines;
};

struct shaderRefMaps
{
	std::map<std::string, ShaderRef> shaderRefs[ShaderType_COUNT];
};

namespace AaShaderFileParser
{
	void ParseShaderParams(ShaderRef& ref, const Config::Object& obj);
	ShaderType ParseShaderType(const std::string& type);

	shaderRefMaps parseAllShaderFiles(std::string directory, bool subFolders = false);
};
