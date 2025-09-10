#include "MaterialResources.h"
#include "FileLogger.h"
#include "MaterialFileParser.h"
#include "GraphicsResources.h"
#include "Directories.h"
#include "ComputeShader.h"

MaterialResources::MaterialResources(RenderSystem& rs, GraphicsResources& r) : renderSystem(rs), resources(r)
{
}

MaterialResources::~MaterialResources()
{
}

MaterialInstance* MaterialResources::getMaterial(std::string name)
{
	auto it = materialMap.find(name);

	if (it != materialMap.end())
		return it->second.get();

	ResourceUploadBatch resourceUpload(renderSystem.core.device);
	resourceUpload.Begin();

	auto m = loadMaterial(name, resourceUpload);

	auto uploadResourcesFinished = resourceUpload.End(renderSystem.core.commandQueue);
	uploadResourcesFinished.wait();

	return m;
}

MaterialInstance* MaterialResources::getMaterial(std::string name, ResourceUploadBatch& batch)
{
	auto it = materialMap.find(name);

	if (it != materialMap.end())
		return it->second.get();

	return loadMaterial(name, batch);
}

MaterialInstance* MaterialResources::loadMaterial(std::string name, ResourceUploadBatch& batch)
{
	for (const MaterialRef& info : knownMaterials)
	{
		if (info.name == name && !info.abstract)
		{
			auto instance = materialBaseMap[info.base.empty() ? name : info.base]->CreateMaterialInstance(info, resources, batch);
			auto ptr = instance.get();
			materialMap[info.name] = std::move(instance);

			return ptr;
		}
	}

	return nullptr;
}

void MaterialResources::loadMaterials(std::string directory, bool subDirectories)
{
	shaderRefMaps shaders;
	MaterialFileParser::parseAllMaterialFiles(knownMaterials, shaders, directory, subDirectories);
	resources.shaders.addShaderReferences(shaders);

	for (size_t i = 0; i < knownMaterials.size(); i++)
	{
		MaterialRef& info = knownMaterials[i];

		if (!info.abstract && info.pipeline.fill == D3D12_FILL_MODE_SOLID && !info.techniqueMaterial[int(MaterialTechnique::Wireframe)])
		{
			auto wMat = info;
			wMat.pipeline.fill = D3D12_FILL_MODE_WIREFRAME;
			wMat.pipeline.depthBias = 1;
			wMat.pipeline.slopeScaledDepthBias = 1;

			wMat.base = wMat.name = info.name + "_WIRE";
			info.techniqueMaterial[int(MaterialTechnique::Wireframe)] = wMat.name;

			knownMaterials.push_back(wMat);
		}
	}

	for (const MaterialRef& info : knownMaterials)
	{
		auto& base = materialBaseMap[info.base];
		if (!base)
		{
			base = std::make_unique<MaterialBase>(*renderSystem.core.device, info);
		}
	}
}

void MaterialResources::ReloadShaders()
{
	renderSystem.core.WaitForAllFrames();

	auto shadersChanged = resources.shaders.reloadShaders();

	for (auto& [name, base] : materialBaseMap)
	{
		for (auto& s : shadersChanged)
		{
			if (base->ContainsShader(s))
			{
				base->ReloadPipeline(resources.shaders);
				break;
			}
		}
	}

	ComputeShaderLibrary::Reload(*renderSystem.core.device, shadersChanged);
}
