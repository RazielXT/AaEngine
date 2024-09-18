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
			bool pixel = obj.type == "pixel_shader";

			shaderRef shader;

			if (obj.params.size() > 1 && obj.params[0] == ":")
			{
				auto parentName = obj.params[1];
				if (!parentName.empty())
				{
					auto parents = pixel ? shds.pixelShaderRefs : shds.vertexShaderRefs;

					for (auto& s : parents)
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
				else if (param.type == "cbuffer" && !param.value.empty())
				{
					auto& paramDefault = shader.privateCbuffers[param.value];
					paramDefault.reserve(param.children.size());

					for (auto& var : param.children)
					{
						if (var.type.starts_with("float"))
						{
							std::vector<float> defaultData;
							defaultData.reserve(var.params.size());

							for (auto& v : var.params)
							{
								defaultData.push_back(std::stof(v));
							}

							paramDefault.push_back({ var.value, defaultData });
						}
					}
				}
			}

			if (pixel)
				shds.pixelShaderRefs[obj.value] = shader;
			else
				shds.vertexShaderRefs[obj.value] = shader;
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
			AaLogger::log(entry.path().generic_string());
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

	return shaders;
}
