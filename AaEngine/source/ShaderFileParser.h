#pragma once

#include <map>
#include <string>
#include <vector>
#include "ConfigParser.h"

enum class ShaderType
{
	Vertex,
	Pixel,
	Compute,
	COUNT,
	None
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
	std::map<std::string, ShaderRef> shaderRefs[(int)ShaderType::COUNT];

	struct ShaderCustomization
	{
		std::string sourceName;
		ShaderRef customization;
	};
	std::map<std::string, ShaderCustomization> shaderCustomizations[(int)ShaderType::COUNT];
};

namespace ShaderFileParser
{
	void ParseShaderParams(ShaderRef& ref, const Config::Object& obj);
	void ParseShaderDefines(ShaderRef& ref, const Config::Object& obj);
	ShaderType ParseShaderType(const std::string& type);

	shaderRefMaps parseAllShaderFiles(std::string directory, bool subFolders = false);
};
