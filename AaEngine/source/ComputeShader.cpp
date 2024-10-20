#include "ComputeShader.h"
#include "AaShaderLibrary.h"
#include "ShaderSignature.h"

void ComputeShader::init(ID3D12Device* device, const std::string& name)
{
	auto shader = AaShaderLibrary::get().getShader(name, ShaderTypeCompute);

	if (!shader)
		return;

	SignatureInfo info;
	info.add(shader, ShaderTypeCompute);
	info.finish();

	auto signature = info.createRootSignature(device);
}
