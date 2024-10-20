#pragma once

#include <SimpleMath.h>
#include "AaModel.h"
#include "ModelParseOptions.h"

namespace BinaryModelLoader
{
	AaModel* load(std::string filename, ModelParseOptions);
};
