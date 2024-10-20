#include "ShaderSignature.h"
#include "AaShaderLibrary.h"
#include "AaLogger.h"

D3D12_SHADER_VISIBILITY SignatureInfo::getVisibility(ShaderType t)
{
	if (t == ShaderTypeVertex)
		return D3D12_SHADER_VISIBILITY_VERTEX;
	if (t == ShaderTypePixel)
		return D3D12_SHADER_VISIBILITY_PIXEL;

	return D3D12_SHADER_VISIBILITY_ALL;
}

void SignatureInfo::addVisibility(D3D12_SHADER_VISIBILITY& v, ShaderType t)
{
	if (v == D3D12_SHADER_VISIBILITY(-1))
		v = getVisibility(t);
	else
		v = D3D12_SHADER_VISIBILITY_ALL;
}

void SignatureInfo::add(LoadedShader* shader, ShaderType type)
{
	if (type == ShaderTypeVertex)
	{
		hasVertexInput = !shader->desc.inputLayout.empty();

		if (hasVertexInput)
			flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;
	}

	if (type == ShaderTypePixel)
	{
		textureTargets = shader->desc.outputTargets;
		flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
	}

	for (auto& b : shader->desc.cbuffers)
	{
		CBuffer* buffer = nullptr;
		for (auto& sb : cbuffers)
		{
			if (sb.info.Name == b.Name)
			{
				buffer = &sb;
				break;
			}
		}
		if (!buffer)
		{
			cbuffers.emplace_back(b);
			buffer = &cbuffers.back();
		}
		addVisibility(buffer->visibility, type);
	}

	for (auto& t : shader->desc.textures)
	{
		Texture* texture = nullptr;
		for (auto& tb : textures)
		{
			if (tb.info.Name == t.Name)
			{
				texture = &tb;
				break;
			}
		}
		if (!texture)
		{
			textures.emplace_back(t);
			texture = &textures.back();
		}
		addVisibility(texture->visibility, type);
	}

	for (auto& s : shader->desc.samplers)
	{
		Sampler* sampler = nullptr;
		for (auto& sb : samplers)
		{
			if (sb.info.Name == s.Name)
			{
				sampler = &sb;
				break;
			}
		}
		if (!sampler)
		{
			samplers.emplace_back(s);
			sampler = &samplers.back();
		}
		addVisibility(sampler->visibility, type);
	}

	for (auto& uav : shader->desc.uavs)
	{
		UAV* found = nullptr;
		for (auto& u : uavs)
		{
			if (u.info.Name == uav.Name)
			{
				found = &u;
				break;
			}
		}
		if (!found)
		{
			uavs.emplace_back(uav);
			found = &uavs.back();
		}
		addVisibility(found->visibility, type);
	}

	bindlessTextures |= shader->desc.bindlessTextures;
}

void SignatureInfo::finish()
{
	UINT rootIndex = 0;

	if (!cbuffers.empty())
	{
		// +1 for potential move to root constants
		const auto rootParamsSizeMax = (64 - hasVertexInput - textures.size() - cbuffers.size() + 1) * sizeof(DWORD);

		for (auto& b : cbuffers)
		{
			if (b.info.Size < rootParamsSizeMax && (!rootBuffer || b.info.Name == "$Globals"))
				rootBuffer = &b;

			for (auto& p : b.info.Params)
			{
				if (bindlessTextures)
					break;

				if (p.Name.starts_with("TexId"))
					bindlessTextures = true;
			}
		}
	}

	if (bindlessTextures)
		flags |= D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;
}

ID3D12RootSignature* SignatureInfo::createRootSignature(ID3D12Device* device)
{
	std::vector<CD3DX12_ROOT_PARAMETER1> params;
	params.resize(cbuffers.size() + textures.size() + uavs.size());
	auto param = params.data();

	for (auto& buffer : cbuffers)
	{
		if (&buffer == rootBuffer)
			param->InitAsConstants(buffer.info.Size / sizeof(DWORD), buffer.info.Slot, buffer.info.Space, buffer.visibility);
		else
			param->InitAsConstantBufferView(buffer.info.Slot, buffer.info.Space, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE, buffer.visibility);

		param++;
	}

	std::vector<CD3DX12_DESCRIPTOR_RANGE1> srvRanges;
	srvRanges.resize(textures.size());
	for (size_t i = 0; i < textures.size(); i++)
	{
		auto& tex = textures[i];
		auto& range = srvRanges[i];

		range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, tex.info.Slot, tex.info.Space, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE);
		param->InitAsDescriptorTable(1, &range, tex.visibility);

		param++;
	}

	std::vector<CD3DX12_DESCRIPTOR_RANGE1> srvRanges2;
	srvRanges2.resize(uavs.size());
	for (size_t i = 0; i < uavs.size(); i++)
	{
		auto& uav = uavs[i];
		auto& range = srvRanges2[i];

		range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, uav.info.Slot, uav.info.Space);
		param->InitAsDescriptorTable(1, &range, uav.visibility);

		param++;
	}

	std::vector<D3D12_STATIC_SAMPLER_DESC> samplersDesc;
	samplersDesc.resize(samplers.size());
	for (size_t i = 0; i < samplers.size(); i++)
	{
		auto& sampler = samplersDesc[i];
		sampler.Filter = D3D12_FILTER_ANISOTROPIC;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.MipLODBias = 0.0f;
		sampler.MaxAnisotropy = 8;
		sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
		sampler.MinLOD = 0.0f;
		sampler.MaxLOD = D3D12_FLOAT32_MAX;
		sampler.ShaderRegister = 0;
		sampler.RegisterSpace = 0;
		sampler.ShaderVisibility = samplers[i].visibility;

		// 		if (i < ref.resources.samplers.size())
		// 		{
		// 			auto& refSampler = ref.resources.samplers[i];
		// 			sampler.Filter = refSampler.filter;
		// 			sampler.AddressU = sampler.AddressV = sampler.AddressW = refSampler.bordering;
		// 			sampler.MaxAnisotropy = refSampler.maxAnisotropy;
		// 			sampler.BorderColor = refSampler.borderColor;
		// 		}
	}

	// create the root signature
	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init_1_1(params.size(), params.data(), samplersDesc.size(), samplersDesc.empty() ? nullptr : samplersDesc.data(), flags);

	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;
	D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &error);
	if (error && error->GetBufferSize() > 0)
	{
		OutputDebugStringA((const char*)error->GetBufferPointer());
		AaLogger::logError((const char*)error->GetBufferPointer());
		return nullptr;
	}

	ID3D12RootSignature* rootSignature{};
	device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));

	return rootSignature;
}
