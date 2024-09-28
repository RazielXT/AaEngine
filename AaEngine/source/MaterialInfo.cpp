#include "MaterialInfo.h"
#include "AaShaderResources.h"

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
		hasVertexInput = !shader->desc.inputLayout.empty();

	if (type == ShaderTypePixel)
		textureTargets = shader->desc.outputTargets;

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
}
