#pragma once

#include "AaShaderCompiler.h"
#include "DescriptorManager.h"
#include "ShaderConstantBuffers.h"
#include "ShaderResources.h"

struct LoadedShader;

struct SignatureInfo
{
	struct CBuffer
	{
		ShaderReflection::CBuffer& info;
		D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY(-1);
	};
	std::vector<CBuffer> cbuffers;

	struct Texture
	{
		ShaderReflection::Texture& info;
		D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY(-1);
	};
	std::vector<Texture> textures;

	struct Sampler
	{
		ShaderReflection::Sampler& info;
		D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY(-1);
	};
	std::vector<Sampler> samplers;

	struct UAV
	{
		ShaderReflection::UAV& info;
		D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY(-1);
	};
	std::vector<UAV> uavs;

	struct StructuredBuffer
	{
		ShaderReflection::StructuredBuffer& info;
		D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY(-1);
	};
	std::vector<StructuredBuffer> structuredBuffers;
	std::vector<StructuredBuffer> rwStructuredBuffers;

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

	ID3D12RootSignature* createRootSignature(ID3D12Device* device, const wchar_t* name, const std::vector<SamplerInfo>& staticSamplers = {});
	std::shared_ptr<ResourcesInfo> createResourcesData() const;

private:

	D3D12_SHADER_VISIBILITY getVisibility(ShaderType t);
	void addVisibility(D3D12_SHADER_VISIBILITY& v, ShaderType t);
};
