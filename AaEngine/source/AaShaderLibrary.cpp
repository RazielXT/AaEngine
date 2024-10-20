#include "AaShaderLibrary.h"
#include "AaLogger.h"
#include "Directories.h"

static AaShaderLibrary* instance = nullptr;

AaShaderLibrary::AaShaderLibrary(AaRenderSystem* mRS)
{
	if (instance)
		throw std::exception("duplicate AaShaderResources");

	mRenderSystem = mRS;
	instance = this;
}

AaShaderLibrary::~AaShaderLibrary()
{
	instance = nullptr;
}

AaShaderLibrary& AaShaderLibrary::get()
{
	return *instance;
}

void AaShaderLibrary::loadShaderReferences(std::string directory, bool subDirectories /*= false*/)
{
	shaderRefMaps maps = AaShaderFileParser::parseAllShaderFiles(directory, subDirectories);
	
	for (int type = 0; type < ShaderType_COUNT; type++)
	{
		auto& map = maps.shaderRefs[type];

		for (const auto& it : map)
		{
			auto& info = loadedShaders[type][it.first];
			if (info == nullptr)
			{
				info = std::make_unique<LoadedShader>();
				info->ref = it.second;
			}
			else
				AaLogger::logWarning("Duplicate shader ref " + it.first);
		}
	}
}

LoadedShader* AaShaderLibrary::getShader(const std::string& name, ShaderType type)
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

std::vector<LoadedShader*> AaShaderLibrary::reloadShaders()
{
	std::vector<LoadedShader*> changed;

	for (int type = 0; type < ShaderType_COUNT; ++type)
	{
		auto& shadersType = loadedShaders[type];

		for (auto& [name, shader] : shadersType)
		{
			struct stat attrib;
			if (stat((SHADER_HLSL_DIRECTORY + shader->ref.file).c_str(), &attrib) == 0 && attrib.st_mtime > shader->filetime)
			{
				shader->blob = compiler.compileShader(shader->ref, shader->desc, (ShaderType)type);
				shader->filetime = attrib.st_mtime;

				changed.push_back(shader.get());
			}
		}
	}

	return changed;
}
