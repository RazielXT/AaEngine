#pragma once

#include "MaterialInfo.h"

enum class CBufferType
{
	Other,
	Root,
	Instancing,
	COUNT
};

class MaterialConstantBuffers
{
public:

	struct 
	{
		CbufferView instancing;
	}
	cbuffers;

	std::vector<std::vector<float>> data;
};