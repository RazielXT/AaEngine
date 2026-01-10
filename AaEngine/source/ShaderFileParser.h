#pragma once

#include <map>
#include <string>
#include <vector>
#include "ConfigParser.h"
#include "ShaderType.h"

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
	ShaderTypesArray<std::map<std::string, ShaderRef>> shaderRefs;

	struct ShaderCustomization
	{
		std::string sourceName;
		ShaderRef customization;
	};
	ShaderTypesArray<std::map<std::string, ShaderCustomization>> shaderCustomizations;
};

namespace ShaderFileParser
{
	void ParseShaderParams(ShaderRef& ref, const Config::Object& obj);
	void ParseShaderDefines(ShaderRef& ref, const Config::Object& obj);

	shaderRefMaps parseAllShaderFiles(std::string directory, bool subFolders = false);
};
