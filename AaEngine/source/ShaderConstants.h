#pragma once

#include "ShaderConstantBuffers.h"
#include <vector>

enum class CBufferType
{
	Other,
	Root,
	Instancing,
	COUNT
};

class ShaderBuffersInfo
{
public:

	struct 
	{
		CbufferView instancing;
	}
	cbuffers;

	std::vector<std::vector<float>> data;
};