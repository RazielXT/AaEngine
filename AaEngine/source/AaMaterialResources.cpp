#include "AaMaterialResources.h"
#include "AaLogger.h"
#include "AaMaterialFileParser.h"
#include "Directories.h"

static AaMaterialResources* instance = nullptr;

AaMaterialResources& AaMaterialResources::get()
{
	return *instance;
}

AaMaterialResources::AaMaterialResources(AaRenderSystem* mRenderSystem)
{
	if (instance)
		throw std::exception("Duplicate AaMaterialResources");

	instance = this;

	this->mRenderSystem = mRenderSystem;
	resourcesMgr.init(mRenderSystem->device, 1000);
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

	for (const MaterialRef& info : knownMaterials)
	{
		if (info.name == name && !info.abstract)
		{
			auto instance = materialBaseMap[info.base.empty() ? name : info.base]->CreateMaterialInstance(info);
			auto ptr = instance.get();
			materialMap[info.name] = std::move(instance);

			return ptr;
		}
	}

	return nullptr;
}

void AaMaterialResources::loadMaterials(std::string directory, bool subDirectories)
{
	AaMaterialFileParser::parseAllMaterialFiles(knownMaterials, directory, subDirectories);

	for (const MaterialRef& info : knownMaterials)
	{
		if (info.base.empty())
		{
			if (materialBaseMap.contains(info.name))
			{
				AaLogger::logWarning("Skip loading duplicate base material " + info.name);
				continue;
			}

			materialBaseMap[info.name] = std::make_unique<MaterialBase>(mRenderSystem, resourcesMgr, info);
		}
	}
}

void AaMaterialResources::PrepareShaderResourceView(RenderTargetTexture& rtt)
{
	resourcesMgr.createShaderResourceView(mRenderSystem->device, rtt);
}

void AaMaterialResources::PrepareDepthShaderResourceView(RenderDepthTargetTexture& rtt)
{
	resourcesMgr.createDepthShaderResourceView(mRenderSystem->device, rtt);
}

void AaMaterialResources::ReloadShaders()
{
	auto shadersChanged = AaShaderResources::get().reloadShaders();

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
