#pragma once

#include "Directx.h"
#include <vector>

bool SaveTextureToMemory(
	ID3D12CommandQueue* pCommandQ,
	ID3D12Resource* pSource,
	std::vector<UCHAR>& data,
	D3D12_RESOURCE_STATES beforeState,
	D3D12_RESOURCE_STATES afterState);

void SaveBMP(std::vector<UCHAR>& imageData, UINT width, UINT height, const char* filename);