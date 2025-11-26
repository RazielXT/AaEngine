#include "ComputeShader.h"
#include "ShaderLibrary.h"
#include "ShaderSignature.h"
#include "StringUtils.h"

ComputeShaderLibrary csLibrary;

ComputeShader::ComputeShader()
{
	csLibrary.shaders.push_back(this);
}

ComputeShader::~ComputeShader()
{
	if (signature)
		signature->Release();

	for (auto it = csLibrary.shaders.begin(); it != csLibrary.shaders.end(); it++)
	{
		if (*it == this)
		{
			csLibrary.shaders.erase(it);
			break;
		}
	}
}

void ComputeShader::init(ID3D12Device& device, const std::string& name, ShaderLibrary& shaders)
{
	return init(device, *shaders.getShader(name, ShaderType::ShaderTypeCompute));
}

void ComputeShader::init(ID3D12Device& device, const LoadedShader& shader)
{
	csShader = &shader;

	SignatureInfo info;
	info.add(shader, ShaderTypeCompute);
	info.finish();

	if (volatileTextures)
		info.setTexturesVolatile();

	signature = info.createRootSignature(device, as_wstring(shader.ref.file + ' ' + shader.ref.entry).c_str());

	D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
	computePsoDesc.pRootSignature = signature;
	computePsoDesc.CS = { shader.blob->GetBufferPointer(), shader.blob->GetBufferSize() };

	device.CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&pipelineState));
}

void ComputeShader::reload(ID3D12Device& device)
{
	init(device, *csShader);
}

void ComputeShaderLibrary::Reload(ID3D12Device& device, const std::vector<const LoadedShader*>& changed)
{
	for (auto cs : csLibrary.shaders)
	{
		if (std::find(changed.begin(), changed.end(), cs->csShader) != changed.end())
		{
			cs->reload(device);
		}
	}
}
