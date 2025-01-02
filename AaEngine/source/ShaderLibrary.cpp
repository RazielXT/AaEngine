#include "ShaderLibrary.h"
#include "FileLogger.h"
#include "Directories.h"

ShaderLibrary::ShaderLibrary()
{
}

ShaderLibrary::~ShaderLibrary()
{
}

void ShaderLibrary::loadShaderReferences(std::string directory, bool subDirectories /*= false*/)
{
	auto maps = ShaderFileParser::parseAllShaderFiles(directory, subDirectories);
	
	addShaderReferences(maps);
}

void ShaderLibrary::addShaderReferences(const shaderRefMaps& maps)
{
	for (int type = 0; type < ShaderType_COUNT; type++)
	{
		auto& map = maps.shaderRefs[type];

		for (const auto& [name, ref] : map)
		{
			auto& info = loadedShaders[type][name];
			if (info == nullptr)
			{
				info = std::make_unique<LoadedShader>();
				info->ref = ref;
			}
			else
				FileLogger::logWarning("Duplicate shader ref " + name);
		}
	}
}

const LoadedShader* ShaderLibrary::getShader(const std::string& name, ShaderType type)
{
	auto it = loadedShaders[type].find(name);

	if (it == loadedShaders[type].end())
		return nullptr;

	if (!it->second->blob)
	{
		it->second->blob = compiler.compileShader(it->second->ref, it->second->desc, type);
		it->second->filetime = std::time(nullptr);
	}

	return it->second.get();
}

const LoadedShader* ShaderLibrary::getShader(const std::string& name, ShaderType type, const ShaderRef& ref)
{
	auto& info = loadedShaders[type][name];
	if (info == nullptr)
	{
		info = std::make_unique<LoadedShader>();
		info->ref = ref;
	}

	return getShader(name, type);
}

std::vector<const LoadedShader*> ShaderLibrary::reloadShaders()
{
	std::vector<const LoadedShader*> changed;

	for (int type = 0; type < ShaderType_COUNT; ++type)
	{
		auto& shadersType = loadedShaders[type];

		for (auto& [name, shader] : shadersType)
		{
			struct stat attrib;
			if (shader->blob && stat((SHADER_HLSL_DIRECTORY + shader->ref.file).c_str(), &attrib) == 0 && attrib.st_mtime > shader->filetime)
			{
				shader->desc = {};
				shader->blob = compiler.compileShader(shader->ref, shader->desc, (ShaderType)type);
				shader->filetime = attrib.st_mtime;

				changed.push_back(shader.get());
			}
		}
	}

	return changed;
}
