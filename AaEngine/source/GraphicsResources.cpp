#include "GraphicsResources.h"
#include "Directories.h"

GraphicsResources::GraphicsResources(RenderSystem& rs)
	: descriptors(*rs.core.device), shaderBuffers(*rs.core.device), materials(rs, *this), models(*rs.core.device)
{
	descriptors.init(10000);
	descriptors.initializeSamplers(rs.upscale.getMipLodBias());

	shaders.loadShaderReferences(SHADER_DIRECTORY);
	materials.loadMaterials(MATERIAL_DIRECTORY);

	//core
	{
		ModelLoadContext loaderCtx(rs);
		loaderCtx.folder = SCENE_DIRECTORY + "core/";
		loaderCtx.group = ResourceGroup::Core;
		loaderCtx.batch.Begin();

		models.preloadFolder(loaderCtx);

		auto uploadResourcesFinished = loaderCtx.batch.End(rs.core.commandQueue);
		uploadResourcesFinished.wait();
	}
}
