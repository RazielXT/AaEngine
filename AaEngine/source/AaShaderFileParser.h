#pragma once

#include <map>
#include <string>
#include <vector>

enum ShaderType
{
	ShaderTypeVertex,
	ShaderTypePixel,
	ShaderType_COUNT
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
	shaderRefMaps parseAllShaderFiles(std::string directory, bool subFolders = false);
};
