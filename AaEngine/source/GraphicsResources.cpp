#include "GraphicsResources.h"
#include "Directories.h"

GraphicsResources::GraphicsResources(RenderSystem& rs)
	: descriptors(*rs.core.device), shaderBuffers(*rs.core.device), materials(rs, *this), models(rs), shaderDefines(*this), shaders(shaderDefines)
{
	descriptors.init(10000);
	descriptors.initializeSamplers(rs.upscale.getMipLodBias());

	shaders.loadShaderReferences(SHADER_DIRECTORY);
	materials.loadMaterials(MATERIAL_DIRECTORY);
}
