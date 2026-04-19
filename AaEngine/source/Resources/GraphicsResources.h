#pragma once

#include "Resources/DescriptorManager.h"
#include "Resources/Shader/ShaderDataBuffers.h"
#include "Resources/Shader/ShaderLibrary.h"
#include "Resources/Shader/ShaderDefines.h"
#include "Resources/Material/MaterialResources.h"
#include "Resources/Model/ModelResources.h"

class RenderSystem;

struct GraphicsResources
{
	GraphicsResources(RenderSystem& rs);

	DescriptorManager descriptors;
	ShaderDataBuffers shaderBuffers;
	ShaderDefines shaderDefines;
	ShaderLibrary shaders;
	TextureResources textures;
	MaterialResources materials;
	ModelResources models;
};
