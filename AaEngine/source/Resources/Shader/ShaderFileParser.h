#pragma once

#include <map>
#include <string>
#include <vector>
#include "Utils/ConfigParser.h"
#include "Resources/Shader/ShaderType.h"

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

	using Defines = std::vector<std::pair<std::string, std::string>>;
	Defines defines;
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
	void ParseShaderDefines(ShaderRef::Defines&, const Config::Object& obj);

	shaderRefMaps parseAllShaderFiles(std::string directory, bool subFolders = false);
};
