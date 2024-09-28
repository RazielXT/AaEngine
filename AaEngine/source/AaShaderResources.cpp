#include "AaShaderResources.h"
#include "AaLogger.h"
#include "Directories.h"

static AaShaderResources* instance = nullptr;

AaShaderResources::AaShaderResources(AaRenderSystem* mRS) : compiler(mRS)
{
	if (instance)
		throw std::exception("duplicate AaShaderResources");

	mRenderSystem = mRS;
	instance = this;
}

AaShaderResources::~AaShaderResources()
{
	instance = nullptr;
}

AaShaderResources& AaShaderResources::get()
{
	return *instance;
}

void AaShaderResources::loadShaderReferences(std::string directory, bool subDirectories /*= false*/)
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

LoadedShader* AaShaderResources::getShader(const std::string& name, ShaderType type)
{
	auto it = loadedShaders[type].find(name);

	if (it == loadedShaders[type].end())
		return nullptr;

	if (!it->second->blob)
	{
		it->second->blob = compiler.compileShader(it->second->ref, it->second->desc);
		it->second->filetime = std::time(nullptr);
	}

	return it->second.get();
}

std::vector<LoadedShader*> AaShaderResources::reloadShaders()
{
	std::vector<LoadedShader*> changed;

	for (auto& shadersType : loadedShaders)
	{
		for (auto& [name, shader] : shadersType)
		{
			struct stat attrib;
			if (stat((SHADER_HLSL_DIRECTORY + shader->ref.file).c_str(), &attrib) == 0 && attrib.st_mtime > shader->filetime)
			{
				shader->blob = compiler.compileShader(shader->ref, shader->desc);
				shader->filetime = attrib.st_mtime;

				changed.push_back(shader.get());
			}
		}
	}

	return changed;
}
