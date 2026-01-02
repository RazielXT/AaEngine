#include "ShaderSignature.h"
#include "ShaderLibrary.h"
#include "FileLogger.h"
#include "GraphicsResources.h"

D3D12_SHADER_VISIBILITY SignatureInfo::getVisibility(ShaderType t)
{
	if (t == ShaderType::Vertex)
		return D3D12_SHADER_VISIBILITY_VERTEX;
	if (t == ShaderType::Pixel)
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

void SignatureInfo::add(const LoadedShader& shader, ShaderType type)
{
	if (type == ShaderType::Vertex)
	{
		hasVertexInput = !shader.desc.inputLayout.empty();

		if (hasVertexInput)
			flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;
	}

	if (type == ShaderType::Pixel)
	{
		textureTargets = shader.desc.outputTargets;
		flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
	}

	for (auto& b : shader.desc.cbuffers)
	{
		CBuffer* buffer = nullptr;
		for (auto& cb : cbuffers)
		{
			if (cb.info.Name == b.Name)
			{
				buffer = &cb;
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

	for (auto& b : shader.desc.structuredBuffers)
	{
		StructuredBuffer* buffer = nullptr;
		for (auto& sb : structuredBuffers)
		{
			if (sb.info.Name == b.Name)
			{
				buffer = &sb;
				break;
			}
		}
		if (!buffer)
		{
			structuredBuffers.emplace_back(b);
			buffer = &structuredBuffers.back();
		}
		addVisibility(buffer->visibility, type);
	}

	for (auto& b : shader.desc.rwStructuredBuffers)
	{
		StructuredBuffer* buffer = nullptr;
		for (auto& sb : rwStructuredBuffers)
		{
			if (sb.info.Name == b.Name)
			{
				buffer = &sb;
				break;
			}
		}
		if (!buffer)
		{
			rwStructuredBuffers.emplace_back(b);
			buffer = &rwStructuredBuffers.back();
		}
		addVisibility(buffer->visibility, type);
	}

	for (auto& t : shader.desc.textures)
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

	for (auto& s : shader.desc.samplers)
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

	for (auto& uav : shader.desc.uavs)
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

	bindlessTextures |= shader.desc.bindlessTextures;
}

void SignatureInfo::setTexturesVolatile()
{
	for (auto& t : textures)
	{
		t.staticWhileExecute = false;
	}
}

void SignatureInfo::finish()
{
	UINT rootIndex = 0;

	if (!cbuffers.empty())
	{
		// +1 for potential move to root constants
		const auto rootParamsSizeMax = (64 - (!bindlessTextures && !textures.empty()) - cbuffers.size() - structuredBuffers.size() - rwStructuredBuffers.size() + 1) * sizeof(DWORD);

		for (auto& b : cbuffers)
		{
			if (b.info.Size <= rootParamsSizeMax && (!rootBuffer || b.info.Name == "$Globals"))
				rootBuffer = &b;
			else if (b.info.Name == "$Globals")
				__debugbreak();

			for (auto& p : b.info.Params)
			{
				if (bindlessTextures)
					break;

				if (p.Name.starts_with("TexId"))
					bindlessTextures = true;

				if (p.Name.starts_with("ResId"))
					bindlessResources = true;
			}
		}
	}

	if (bindlessTextures || bindlessResources)
		flags |= D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED | D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED;
}

ID3D12RootSignature* SignatureInfo::createRootSignature(ID3D12Device& device, const wchar_t* name, const std::vector<SamplerInfo>& staticSamplers)
{
	std::vector<CD3DX12_ROOT_PARAMETER1> params;
	params.resize(cbuffers.size() + textures.size() + uavs.size() + structuredBuffers.size() + rwStructuredBuffers.size());
	auto param = params.data();

	for (auto& buffer : cbuffers)
	{
		if (&buffer == rootBuffer)
			param->InitAsConstants(buffer.info.Size / sizeof(DWORD), buffer.info.Slot, buffer.info.Space, buffer.visibility);
		else
			param->InitAsConstantBufferView(buffer.info.Slot, buffer.info.Space, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE, buffer.visibility);

		param++;
	}

	for (auto& buffer : structuredBuffers)
	{
		param->InitAsShaderResourceView(buffer.info.Slot, buffer.info.Space, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE, buffer.visibility);

		param++;
	}

	for (auto& buffer : rwStructuredBuffers)
	{
		param->InitAsUnorderedAccessView(buffer.info.Slot, buffer.info.Space, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, buffer.visibility);

		param++;
	}

	std::vector<CD3DX12_DESCRIPTOR_RANGE1> srvRanges;
	srvRanges.resize(textures.size());
	for (size_t i = 0; i < textures.size(); i++)
	{
		auto& tex = textures[i];
		auto& range = srvRanges[i];

		range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, tex.info.Slot, tex.info.Space, tex.staticWhileExecute ? D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE : D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
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
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.MipLODBias = 0.0f;
		sampler.MaxAnisotropy = 8;
		sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		sampler.MinLOD = 0.0f;
		sampler.MaxLOD = D3D12_FLOAT32_MAX;
		sampler.ShaderRegister = samplers[i].info.Slot;
		sampler.RegisterSpace = 0;
		sampler.ShaderVisibility = samplers[i].visibility;

		if (samplers[i].info.Name == "DepthSampler" || samplers[i].info.Name == "PointSampler")
		{
			sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
			sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
		}
		else if (samplers[i].info.Name == "DepthClampSampler")
		{
			sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
			sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		}
		else if (samplers[i].info.Name == "ShadowSampler")
		{
			sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
			sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
		}
		else if (samplers[i].info.Name == "ShadowCmpSampler")
		{
			sampler.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
			sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
			sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS;
		}
		else if (samplers[i].info.Name == "LinearSampler")
		{
			sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		}
		else if (samplers[i].info.Name == "LinearWrapSampler")
		{
			sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		}
		else if (samplers[i].info.Name == "LinearBorderSampler")
		{
			sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
		}
		else if (samplers[i].info.Name == "VoxelSampler")
		{
			sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		}
		else if (i < staticSamplers.size())
		{
			auto& refSampler = staticSamplers[i];
			sampler.Filter = refSampler.filter;
			sampler.AddressU = sampler.AddressV = sampler.AddressW = refSampler.bordering;
			sampler.MaxAnisotropy = refSampler.maxAnisotropy;
			sampler.BorderColor = refSampler.borderColor;
		}
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
		FileLogger::logError((const char*)error->GetBufferPointer());
		return nullptr;
	}

	ID3D12RootSignature* rootSignature{};
	device.CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	rootSignature->SetName(name);

	return rootSignature;
}

std::shared_ptr<ResourcesInfo> SignatureInfo::createResourcesData(GraphicsResources& graphicsResources) const
{
	auto resources = std::make_shared<ResourcesInfo>();
	UINT rootIndex = 0;
	std::vector<UINT> bindlessTextures;

	for (auto& cb : cbuffers)
	{
		if (&cb == rootBuffer)
		{
			resources->rootBuffer.defaultData.resize(cb.info.Size / sizeof(float));
			resources->rootBuffer.rootIndex = rootIndex++;

			for (const auto& p : cb.info.Params)
			{
				ResourcesInfo::AutoParam type = ResourcesInfo::AutoParam::None;

				if (p.Name.starts_with("TexId"))
				{
					type = ResourcesInfo::AutoParam::TEXID;
					bindlessTextures.push_back((UINT)resources->resourceAutoParams.size());
				}
				if (type != ResourcesInfo::AutoParam::None)
				{
					resources->resourceAutoParams.emplace_back(type, (UINT)(p.StartOffset / sizeof(float)));
					continue;
				}

				type = ResourcesInfo::AutoParam::None;

				if (p.Name == "WorldMatrix")
					type = ResourcesInfo::AutoParam::WORLD_MATRIX;
				else if (p.Name == "InvWorldMatrix")
					type = ResourcesInfo::AutoParam::INV_WORLD_MATRIX;
				else if (p.Name == "PreviousWorldMatrix")
					type = ResourcesInfo::AutoParam::PREV_WORLD_MATRIX;
				else if (p.Name == "WorldPosition")
					type = ResourcesInfo::AutoParam::WORLD_POSITION;

				if (type != ResourcesInfo::AutoParam::None)
				{
					resources->objectAutoParams.emplace_back(type, (UINT)(p.StartOffset / sizeof(float)));
					continue;
				}

				type = ResourcesInfo::AutoParam::None;

				if (p.Name == "ViewProjectionMatrix")
					type = ResourcesInfo::AutoParam::VP_MATRIX;
				else if (p.Name == "ViewMatrix")
					type = ResourcesInfo::AutoParam::VIEW_MATRIX;
				else if (p.Name == "WorldViewProjectionMatrix")
					type = ResourcesInfo::AutoParam::WVP_MATRIX;
				else if (p.Name == "ProjectionMatrix")
					type = ResourcesInfo::AutoParam::PROJ_MATRIX;
				else if (p.Name == "InvViewProjectionMatrix")
					type = ResourcesInfo::AutoParam::INV_VP_MATRIX;
				else if (p.Name == "InvViewMatrix")
					type = ResourcesInfo::AutoParam::INV_VIEW_MATRIX;
				else if (p.Name == "InvProjectionMatrix")
					type = ResourcesInfo::AutoParam::INV_PROJ_MATRIX;
				else if (p.Name == "ShadowMatrix")
					type = ResourcesInfo::AutoParam::SHADOW_MATRIX;
				else if (p.Name == "ShadowMapSize")
					type = ResourcesInfo::AutoParam::SHADOW_MAP_SIZE;
				else if (p.Name == "ShadowMapSizeInv")
					type = ResourcesInfo::AutoParam::SHADOW_MAP_SIZE_INV;
				else if (p.Name == "Time")
					type = ResourcesInfo::AutoParam::TIME;
				else if (p.Name == "DeltaTime")
					type = ResourcesInfo::AutoParam::DELTA_TIME;
				else if (p.Name == "ViewportSizeInverse")
					type = ResourcesInfo::AutoParam::VIEWPORT_SIZE_INV;
				else if (p.Name == "ViewportSize")
					type = ResourcesInfo::AutoParam::VIEWPORT_SIZE;
				else if (p.Name == "SunDirection")
					type = ResourcesInfo::AutoParam::SUN_DIRECTION;
				else if (p.Name == "SunColor")
					type = ResourcesInfo::AutoParam::SUN_COLOR;
				else if (p.Name == "CameraPosition")
					type = ResourcesInfo::AutoParam::CAMERA_POSITION;
				else if (p.Name == "ZMagic")
					type = ResourcesInfo::AutoParam::Z_MAGIC;

				if (type != ResourcesInfo::AutoParam::None)
					resources->frameAutoParams.emplace_back(type, (UINT)(p.StartOffset / sizeof(float)));
			}
		}
		else
		{
			ResourcesInfo::GpuBuffer r;
			r.globalCBuffer = graphicsResources.shaderBuffers.GetCbufferResource(cb.info.Name);
			r.type = GpuBufferType::Global;
			r.rootIndex = rootIndex++;

			resources->buffers.push_back(r);
		}
	}

	for (auto& b : structuredBuffers)
	{
		ResourcesInfo::GpuBuffer r;

		if (b.info.Name == "InstancingBuffer")
			r.type = GpuBufferType::Instancing;
		else if (b.info.Name == "GeometryBuffer")
			r.type = GpuBufferType::Geometry;

		r.rootIndex = rootIndex++;
		resources->buffers.push_back(r);
	}

	resources->textures.resize(textures.size() + bindlessTextures.size());
	for (size_t i = 0; i < textures.size(); i++)
	{
		auto& t = resources->textures[i];
		t.rootIndex = rootIndex++;
	}
	for (size_t i = 0; i < bindlessTextures.size(); i++)
	{
		auto& t = resources->textures[i + textures.size()];
		t.autoParamIdx = bindlessTextures[i];
	}

	resources->boundTexturesCount = (UINT)textures.size();

	resources->uavs.resize(uavs.size() + rwStructuredBuffers.size());
	for (size_t i = 0; i < uavs.size(); i++)
	{
		auto& uav = resources->uavs[i];
		uav.rootIndex = rootIndex++;
	}
	for (size_t i = 0; i < rwStructuredBuffers.size(); i++)
	{
		auto& uav = resources->uavs[uavs.size() + i];
		uav.rootIndex = rootIndex++;
	}

	return resources;
}
