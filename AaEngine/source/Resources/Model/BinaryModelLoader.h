#pragma once

#include <SimpleMath.h>
#include "Resources/Model/VertexBufferModel.h"
#include "Resources/Model/ModelParseOptions.h"

namespace BinaryModelLoader
{
	VertexBufferModel* load(std::string filename, ModelParseOptions);
};
