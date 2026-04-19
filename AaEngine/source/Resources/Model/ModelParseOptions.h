#pragma once

#include "ResourceUploadBatch.h"

struct ModelParseOptions
{
	ResourceUploadBatch* batch;
	ID3D12Device* device;
};
