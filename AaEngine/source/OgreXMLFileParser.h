#pragma once

#include "AaModel.h"

namespace OgreXMLFileParser
{
	AaModel* load(std::string filename, AaRenderSystem* rs, bool optimize = true, bool saveBinary = false);
};