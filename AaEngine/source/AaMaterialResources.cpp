#include "AaMaterialResources.h"
#include "AaLogger.h"
#include "AaMaterialFileParser.h"
#include "Directories.h"

static AaMaterialResources* instance = nullptr;

AaMaterialResources& AaMaterialResources::get()
{
	return *instance;
}

AaMaterialResources::AaMaterialResources(RenderSystem* rs)
{
	if (instance)
		throw std::exception("Duplicate AaMaterialResources");

	instance = this;

	renderSystem = rs;
}

AaMaterialResources::~AaMaterialResources()
{
	instance = nullptr;
}

MaterialInstance* AaMaterialResources::getMaterial(std::string name)
{
	auto it = materialMap.find(name);

	if (it != materialMap.end())
		return it->second.get();

	ResourceUploadBatch resourceUpload(renderSystem->device);
	resourceUpload.Begin();

	auto m = loadMaterial(name, resourceUpload);

	auto uploadResourcesFinished = resourceUpload.End(renderSystem->commandQueue);
	uploadResourcesFinished.wait();

	return m;
}

MaterialInstance* AaMaterialResources::getMaterial(std::string name, ResourceUploadBatch& batch)
{
	auto it = materialMap.find(name);

	if (it != materialMap.end())
		return it->second.get();

	return loadMaterial(name, batch);
}

MaterialInstance* AaMaterialResources::loadMaterial(std::string name, ResourceUploadBatch& batch)
{
	for (const MaterialRef& info : knownMaterials)
	{
		if (info.name == name && !info.abstract)
		{
			auto instance = materialBaseMap[info.base.empty() ? name : info.base]->CreateMaterialInstance(info, batch);
			auto ptr = instance.get();
			materialMap[info.name] = std::move(instance);

			return ptr;
		}
	}

	return nullptr;
}

void AaMaterialResources::loadMaterials(std::string directory, bool subDirectories)
{
	shaderRefMaps shaders;
	AaMaterialFileParser::parseAllMaterialFiles(knownMaterials, shaders, directory, subDirectories);
	AaShaderLibrary::get().addShaderReferences(shaders);

	for (const MaterialRef& info : knownMaterials)
	{
		auto& base = materialBaseMap[info.base];
		if (!base)
		{
			base = std::make_unique<MaterialBase>(renderSystem, DescriptorManager::get(), info);
		}
	}
}

void AaMaterialResources::ReloadShaders()
{
	auto shadersChanged = AaShaderLibrary::get().reloadShaders();

	for (auto& [name, base] : materialBaseMap)
	{
		for (auto& s : shadersChanged)
		{
			if (base->ContainsShader(s))
			{
				base->ReloadPipeline();
				break;
			}
		}
	}
}
