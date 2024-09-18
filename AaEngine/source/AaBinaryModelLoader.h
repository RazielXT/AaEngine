#pragma once

#include "AaModel.h"
#include "AaModelSerialization.h"

class AaRenderSystem;

namespace AaBinaryModelLoader
{
	AaModel* load(std::string filename, AaRenderSystem* rs);

	AaModel* load(ModelInfo& info, AaRenderSystem* rs);
};
