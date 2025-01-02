#pragma once

#include <SimpleMath.h>
#include "VertexBufferModel.h"
#include "ModelParseOptions.h"

namespace BinaryModelLoader
{
	VertexBufferModel* load(std::string filename, ModelParseOptions);
};
