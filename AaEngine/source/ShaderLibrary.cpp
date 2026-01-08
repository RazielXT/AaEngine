#include "ShaderLibrary.h"
#include "FileLogger.h"
#include "Directories.h"

ShaderLibrary::ShaderLibrary(const ShaderDefines& d) : defines(d)
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
	for (int type = 0; type < (int)ShaderType::COUNT; type++)
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

		auto& customizations = maps.shaderCustomizations[type];

		for (const auto& [name, info] : customizations)
		{
			auto& original = loadedShaders[type][info.sourceName];
			if (original)
			{
				auto& customizedShader = loadedShaders[type][name];
				customizedShader = std::make_unique<LoadedShader>();
				customizedShader->ref = original->ref;

				for (auto& d : info.customization.defines)
				{
					customizedShader->ref.defines.push_back(d);
				}
			}
			else
				FileLogger::logWarning("Invalid customization shader ref " + info.sourceName);
		}
	}
}

const LoadedShader* ShaderLibrary::getShader(const std::string& name, ShaderType type)
{
	auto it = loadedShaders[(int)type].find(name);

	if (it == loadedShaders[(int)type].end())
		return nullptr;

	if (!it->second->blob)
	{
		it->second->blob = compiler.compileShader(it->second->ref, it->second->desc, type, defines);
		it->second->filetime = std::time(nullptr);
	}

	return it->second.get();
}

const LoadedShader* ShaderLibrary::getShader(const std::string& name, ShaderType type, const ShaderRef& ref)
{
	auto& info = loadedShaders[(int)type][name];
	if (info == nullptr)
	{
		info = std::make_unique<LoadedShader>();
		info->ref = ref;
	}

	return getShader(name, type);
}

std::vector<const LoadedShader*> ShaderLibrary::reloadShadersChangedFiles()
{
	std::vector<const LoadedShader*> changed;

	for (int type = 0; type < (int)ShaderType::COUNT; ++type)
	{
		auto& shadersType = loadedShaders[type];

		for (auto& [name, shader] : shadersType)
		{
			if (!shader->blob)
				continue;

			bool fileChanged = [&]()
				{
					struct stat attrib;

					if (stat((SHADER_HLSL_DIRECTORY + shader->ref.file).c_str(), &attrib) == 0 && attrib.st_mtime > shader->filetime)
						return true;

					for (auto& i : shader->desc.compiledIncludes)
					{
						if (stat(i.c_str(), &attrib) == 0 && attrib.st_mtime > shader->filetime)
							return true;
					}

					return false;
				}();

			if (fileChanged)
			{
				if (auto blob = compiler.compileShader(shader->ref, shader->desc, (ShaderType)type, defines))
				{
					shader->blob = blob;
					shader->filetime = std::time(nullptr);

					changed.push_back(shader.get());
				}
			}
		}
	}

	return changed;
}

std::vector<const LoadedShader*> ShaderLibrary::reloadShadersWithDefine(const std::string& define)
{
	std::vector<const LoadedShader*> changed;

	for (int type = 0; type < (int)ShaderType::COUNT; ++type)
	{
		auto& shadersType = loadedShaders[type];

		for (auto& [name, shader] : shadersType)
		{
			if (!shader->blob)
				continue;

			if (shader->desc.allDefines.contains(define))
			{
				shader->blob = compiler.compileShader(shader->ref, shader->desc, (ShaderType)type, defines);
				shader->filetime = std::time(nullptr);

				changed.push_back(shader.get());
			}
		}
	}

	return changed;
}

std::set<std::string> ShaderLibrary::getKnownDefines() const
{
	std::set<std::string> usedDefines;

	for (auto& shaders : loadedShaders)
	{
		for (auto& [name, shader] : shaders)
		{
			usedDefines.insert(shader->desc.allDefines.cbegin(), shader->desc.allDefines.cend());
		}
	}

	return usedDefines;
}
