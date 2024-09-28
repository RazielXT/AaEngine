#include "AaShaderFileParser.h"
#include "AaLogger.h"
#include <filesystem>
#include "ConfigParser.h"

void parseShaderFile(shaderRefMaps& shds, const std::string& file)
{
	auto objects = Config::Parse(file);

	for (auto& obj : objects)
	{
		if (obj.type == "vertex_shader" || obj.type == "pixel_shader")
		{
			ShaderType type = ShaderTypePixel;
			if (obj.type == "vertex_shader") type = ShaderTypeVertex;

			ShaderRef shader;

			if (obj.params.size() > 1 && obj.params[0] == ":")
			{
				auto parentName = obj.params[1];
				if (!parentName.empty())
				{
					for (auto& s : shds.shaderRefs[type])
					{
						if (s.first == parentName)
							shader = s.second;
					}
				}
			}

			for (auto& param : obj.children)
			{
				if (param.type == "file")
				{
					shader.file = param.value;
				}
				else if (param.type == "entry")
				{
					shader.entry = param.value;
				}
				else if (param.type == "profile")
				{
					shader.profile = param.value;
				}
				else if (param.type == "defines")
				{
					auto addDefine = [&](const std::string& token)
						{
							size_t pos = token.find('=');
							if (pos != std::string::npos) {
								std::string key = token.substr(0, pos);
								std::string value = token.substr(pos + 1);
								shader.defines.emplace_back(key, value);
							}
							else {
								shader.defines.emplace_back(token, "1");
							}
						};

					addDefine(param.value);
					for (auto& p : param.params)
						addDefine(p);
				}
			}

			shds.shaderRefs[type][obj.value] = shader;
		}
	}
}

void parseAllShaderFiles(shaderRefMaps& shaders, std::string directory, bool subFolders /*= false*/)
{
	for (const auto& entry : std::filesystem::directory_iterator(directory))
	{
		if (entry.is_directory() && subFolders)
		{
			parseAllShaderFiles(shaders, entry.path().generic_string(), subFolders);
		}
		else if (entry.is_regular_file() && entry.path().generic_string().ends_with(".shader"))
		{
			AaLogger::log("Parsing " + entry.path().generic_string());
			parseShaderFile(shaders, entry.path().generic_string());
		}
	}
}

shaderRefMaps AaShaderFileParser::parseAllShaderFiles(std::string directory, bool subFolders)
{
	shaderRefMaps shaders;

	if (!std::filesystem::exists(directory))
		return shaders;

	parseAllShaderFiles(shaders, directory, subFolders);

	size_t c = 0;
	for (auto s : shaders.shaderRefs)
		c += s.size();

	AaLogger::log("Total shaders found: " + std::to_string(c));

	return shaders;
}
