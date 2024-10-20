#pragma once

#include "AaShaderCompiler.h"
#include "ResourcesManager.h"
#include "ShaderConstantBuffers.h"

struct LoadedShader;

struct SignatureInfo
{
	struct CBuffer
	{
		CBufferInfo& info;
		D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY(-1);
	};
	std::vector<CBuffer> cbuffers;

	struct Texture
	{
		TextureInfo& info;
		D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY(-1);
	};
	std::vector<Texture> textures;

	struct Sampler
	{
		SamplerInfo& info;
		D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY(-1);
	};
	std::vector<Sampler> samplers;

	struct UAV
	{
		UAVInfo& info;
		D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY(-1);
	};
	std::vector<UAV> uavs;

	CBuffer* rootBuffer = nullptr;
	UINT textureTargets{};
	bool hasVertexInput = false;
	bool bindlessTextures = false;

	D3D12_ROOT_SIGNATURE_FLAGS flags =
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS;

	void add(LoadedShader* shader, ShaderType type);
	void finish();

	ID3D12RootSignature* createRootSignature(ID3D12Device* device);

private:

	D3D12_SHADER_VISIBILITY getVisibility(ShaderType t);
	void addVisibility(D3D12_SHADER_VISIBILITY& v, ShaderType t);
};
