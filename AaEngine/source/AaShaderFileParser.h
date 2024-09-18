#pragma once

#include <map>
#include <string>
#include <vector>

struct ConstantDefault
{
	std::string name;
	std::vector<float> data;
};

struct shaderRef
{
	std::string file;
	std::string entry;
	std::string profile;
	std::vector<std::pair<std::string, std::string>> defines;

	std::map<std::string, std::vector<ConstantDefault>> privateCbuffers;
};

struct shaderRefMaps
{
	std::map<std::string, shaderRef> vertexShaderRefs;
	std::map<std::string, shaderRef> pixelShaderRefs;
};

namespace AaShaderFileParser
{
	shaderRefMaps parseAllShaderFiles(std::string directory, bool subFolders = false);
};
