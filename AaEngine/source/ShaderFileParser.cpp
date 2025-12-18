#include "ShaderFileParser.h"
#include "FileLogger.h"
#include <filesystem>
#include "ConfigParser.h"

using ParsedObjects = std::map<std::string, const Config::Object*>[(int)ShaderType::COUNT];

void ShaderFileParser::ParseShaderParams(ShaderRef& shader, const Config::Object& obj)
{
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
			ParseShaderDefines(shader, param);
		}
	}
}

void ShaderFileParser::ParseShaderDefines(ShaderRef& shader, const Config::Object& param)
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

static void ParseShaderParams(ShaderRef& shader, ShaderType type, const Config::Object& obj, const ParsedObjects& previous)
{
	if (obj.params.size() > 1 && obj.params[0] == ":")
	{
		for (size_t i = 1; i < obj.params.size(); i++)
		{
			auto parentName = obj.params[i];
			if (!parentName.empty())
			{
				for (auto& [name, parent] : previous[(int)type])
				{
					if (name == parentName)
						ParseShaderParams(shader, type, *parent, previous);
				}
			}
		}
	}

	ShaderFileParser::ParseShaderParams(shader, obj);
}

ShaderType ShaderFileParser::ParseShaderType(const std::string& t)
{
	ShaderType type = ShaderType::None;
	if (t == "vertex_shader")
		type = ShaderType::Vertex;
	else if (t == "pixel_shader")
		type = ShaderType::Pixel;
	else if (t == "compute_shader")
		type = ShaderType::Compute;

	return type;
}

void parseShaderFile(shaderRefMaps& shds, const std::string& file)
{
	auto objects = Config::Parse(file);

	ParsedObjects parsed;
	for (auto& obj : objects)
	{
		auto type = ShaderFileParser::ParseShaderType(obj.type);
		
		if (type != ShaderType::None)
		{
			ShaderRef shader;
			ParseShaderParams(shader, type, obj, parsed);

			shds.shaderRefs[(int)type][obj.value] = shader;
			parsed[(int)type][obj.value] = &obj;
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
			FileLogger::log("Parsing " + entry.path().generic_string());
			parseShaderFile(shaders, entry.path().generic_string());
		}
	}
}

shaderRefMaps ShaderFileParser::parseAllShaderFiles(std::string directory, bool subFolders)
{
	shaderRefMaps shaders;

	if (!std::filesystem::exists(directory))
		return shaders;

	parseAllShaderFiles(shaders, directory, subFolders);

	size_t c = 0;
	for (const auto& s : shaders.shaderRefs)
		c += s.size();

	FileLogger::log("Total shaders found: " + std::to_string(c));

	return shaders;
}
