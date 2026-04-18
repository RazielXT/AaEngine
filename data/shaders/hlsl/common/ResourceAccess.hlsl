#pragma once

// Descriptor-heap texture access helpers.
// Use typed variants to make call sites explicit and avoid per-file wrappers.
Texture2D<float4> GetTexture2D(uint index)
{
	return ResourceDescriptorHeap[index];
}

Texture2D<float3> GetTexture2D3f(uint index)
{
	return ResourceDescriptorHeap[index];
}

Texture2D<float2> GetTexture2D2f(uint index)
{
	return ResourceDescriptorHeap[index];
}

Texture2D<float> GetTexture2D1f(uint index)
{
	return ResourceDescriptorHeap[index];
}

Texture2DArray<float4> GetTexture2DArray(uint index)
{
	return ResourceDescriptorHeap[index];
}

Texture2DArray<float> GetTexture2DArray1f(uint index)
{
	return ResourceDescriptorHeap[index];
}

TextureCube<float4> GetTextureCube(uint index)
{
	return ResourceDescriptorHeap[index];
}

Texture3D<float4> GetTexture3D(uint index)
{
	return ResourceDescriptorHeap[index];
}

Texture3D<float> GetTexture3D1f(uint index)
{
	return ResourceDescriptorHeap[index];
}

RWTexture2D<float4> GetRWTexture2D(uint index)
{
	return ResourceDescriptorHeap[index];
}

RWTexture2D<float2> GetRWTexture2D2f(uint index)
{
	return ResourceDescriptorHeap[index];
}

RWTexture2D<float> GetRWTexture2D1f(uint index)
{
	return ResourceDescriptorHeap[index];
}

RWTexture2DArray<float4> GetRWTexture2DArray(uint index)
{
	return ResourceDescriptorHeap[index];
}

RWTexture2DArray<float> GetRWTexture2DArray1f(uint index)
{
	return ResourceDescriptorHeap[index];
}

RWTexture3D<float4> GetRWTexture3D(uint index)
{
	return ResourceDescriptorHeap[index];
}

RWTexture3D<float> GetRWTexture3D1f(uint index)
{
	return ResourceDescriptorHeap[index];
}

// Dynamic samplers for material/object rendering.
// Post-process passes should keep using explicit static samplers bound in root signatures.
SamplerState GetDynamicMaterialSampler(uint index)
{
	return SamplerDescriptorHeap[index];
}

SamplerState GetDynamicMaterialSamplerLinear()
{
	return SamplerDescriptorHeap[0];
}
