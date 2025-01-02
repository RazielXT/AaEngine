#pragma once

#include "DescriptorManager.h"
#include "ShaderDataBuffers.h"
#include "ShaderLibrary.h"
#include "MaterialResources.h"
#include "ModelResources.h"

class RenderSystem;

struct GraphicsResources
{
	GraphicsResources(RenderSystem& rs);

	DescriptorManager descriptors;
	ShaderDataBuffers shaderBuffers;
	ShaderLibrary shaders;
	TextureResources textures;
	MaterialResources materials;
	ModelResources models;
};
