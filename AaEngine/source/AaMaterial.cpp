#include "AaMaterial.h"
#include "AaEntity.h"
#include "AaLogger.h"
#include "AaShaderBuffersManager.h"
#include "AaMaterialFileParser.h"
#include "AaMaterialResources.h"
#include "AaShaderManager.h"

AaMaterial::AaMaterial(std::string name, AaRenderSystem* mRenderSystem)
{
	this->name = name;
	mRS = mRenderSystem;
	mRenderState = mRenderSystem->getDefaultRenderState();
}

AaMaterial::~AaMaterial()
{
	for (MaterialShader* sh : shaders)
	{
		if (sh)
		{
			delete sh;
		}
	}
}

void AaMaterial::setMaterialConstant(std::string buffer, std::string name, ShaderType targetShaderType, float* value)
{
	if (MaterialShader* sh = shaders[targetShaderType])
	{
		for (auto& b : sh->privateCbuffers)
		{
			if (b.info->name == buffer)
			{
				for (auto& p : b.info->params)
				{
					if (p.name == name)
					{
						memcpy(b.data.data() + (p.position / sizeof(float)), value, sizeof(p.size));
						b.needsUpdate = true;
						break;
					}
				}
				break;
			}
		}
	}
}

void AaMaterial::updateObjectConstants(AaEntity* ent, AaCamera& camera)
{
	//if (usedBuffersFlag & CBUFFER_PER_OBJECT)
		AaShaderBuffersManager::get().updateObjectConstants(ent, camera);
}

std::pair<int, int> AaMaterial::prepareCBufferForRendering(ID3D11Buffer** buffersArray, MaterialShader* sh)
{
	auto& shading = AaShaderBuffersManager::get();

	int buffMax = 0;
	int buffMin = 100;

	//update constant buffer if needed
	for (auto& b : sh->privateCbuffers)
	{
		if (b.needsUpdate)
		{
			mRS->getContext()->UpdateSubresource(b.buffer, 0, nullptr, b.data.data(), 0, 0);
			b.needsUpdate = false;
		}

		buffMin = min(buffMin, b.info->slot);
		buffMax = max(buffMax, b.info->slot);
	}
	for (auto& c : sh->shader->compiled->globalCbuffers)
	{
		buffMin = min(buffMin, c.second);
		buffMax = max(buffMax, c.second);
	}

	for (auto& b : sh->privateCbuffers)
	{
		buffersArray[b.info->slot - buffMin] = b.buffer;
	}
	for (auto& c : sh->shader->compiled->globalCbuffers)
	{
		buffersArray[c.second - buffMin] = shading.getCbuffer(c.first);
	}
	
	int buffCount = 0;
	if (buffMin < 100)
		buffCount = 1 + buffMax - buffMin;

	return { buffMin, buffCount };
}

void AaMaterial::prepareForRendering()
{
	ID3D11DeviceContext* context = mRS->getContext();

	//for all possible shaders
	for (auto type = 0; type < std::size(shaders); type++)
	{
		auto sh = shaders[type];
		if (!sh)
			continue;

		ID3D11Buffer* buffersArray[10]{};
		auto [startSlot, buffCount] = prepareCBufferForRendering(buffersArray, sh);

		if (type == ShaderTypeVertex)
		{
			context->VSSetShader((ID3D11VertexShader*)sh->shader->compiled->shader, nullptr, 0);

			if (buffCount > 0)
				context->VSSetConstantBuffers(startSlot, buffCount, buffersArray);

			if (sh->numTextures > 0)
			{
				context->VSSetShaderResources(0, sh->numTextures, sh->shaderMaps);
				context->VSSetSamplers(0, sh->numTextures, sh->samplerStates);
			}
		}
		else if (type == ShaderTypePixel)
		{
			context->PSSetShader((ID3D11PixelShader*)sh->shader->compiled->shader, nullptr, 0);

			if (buffCount > 0)
				context->PSSetConstantBuffers(startSlot, buffCount, buffersArray);

			if (sh->numTextures > 0)
			{
				context->PSSetShaderResources(0, sh->numTextures, sh->shaderMaps);
				context->PSSetSamplers(0, sh->numTextures, sh->samplerStates);
			}

			if (sh->numUAVs)
			{
				mRS->setUAVs(sh->numUAVs, sh->UAVs);
			}
		}
	}
}

void AaMaterial::setPSTextures(ID3D11ShaderResourceView** textures, ID3D11SamplerState** samplers, int count, int start)
{
	MaterialShader* ps = shaders[ShaderTypePixel];

	if (ps)
	{
		ID3D11DeviceContext* context = mRS->getContext();

		context->PSSetShaderResources(start, count, textures);
		context->PSSetSamplers(start, count, samplers);
	}
}

void AaMaterial::clearPSTextures(int count, int start)
{
	ID3D11ShaderResourceView* srNulls[10] = {};
	ID3D11SamplerState* ssNulls[10] = {};

	MaterialShader* sh = shaders[ShaderTypePixel];

	if (sh)
	{
		ID3D11DeviceContext* context = mRS->getContext();
		context->VSSetShaderResources(start, count, srNulls);
		context->VSSetSamplers(start, count, ssNulls);
	}
}

const std::string& AaMaterial::getName() const
{
	return name;
}

void AaMaterial::prepareForRenderingWithCustomPSTextures(ID3D11ShaderResourceView** textures, int count, int start)
{
	ID3D11DeviceContext* context = mRS->getContext();

	//for all possible shaders
	for (auto type = 0; type < std::size(shaders); type++)
	{
		auto sh = shaders[type];
		if (!sh)
			continue;

		ID3D11Buffer* buffersArray[10];
		auto [startSlot, buffCount] = prepareCBufferForRendering(buffersArray, sh);

		if (type == ShaderTypeVertex)
		{
			context->VSSetShader((ID3D11VertexShader*)sh->shader->compiled->shader, nullptr, 0);

			if (buffCount > 0)
				context->VSSetConstantBuffers(startSlot, buffCount, buffersArray);

			if (sh->numTextures > 0)
			{
				context->VSSetShaderResources(0, sh->numTextures, sh->shaderMaps);
				context->VSSetSamplers(0, sh->numTextures, sh->samplerStates);
			}
		}
		else if (type == ShaderTypePixel)
		{
			context->PSSetShader((ID3D11PixelShader*)sh->shader->compiled->shader, nullptr, 0);

			if (buffCount > 0)
				context->PSSetConstantBuffers(startSlot, buffCount, buffersArray);

			if (sh->numTextures > 0)
			{
				if (count < sh->numTextures)
				{
					if (start > 0)
						context->PSSetShaderResources(0, sh->numTextures - count, sh->shaderMaps);
					else
						context->PSSetShaderResources(count, sh->numTextures - count, sh->shaderMaps);
				}
				else
				{
					context->PSSetShaderResources(start, count, textures);
				}

				context->PSSetSamplers(0, sh->numTextures, sh->samplerStates);

			}

			if (sh->numUAVs)
			{
				mRS->setUAVs(sh->numUAVs, sh->UAVs);
			}
		}
	}
}

void AaMaterial::clearAfterRendering()
{
	ID3D11DeviceContext* context = mRS->getContext();
	ID3D11ShaderResourceView* srNulls[10] = {};
	ID3D11SamplerState* ssNulls[10] = {};

	for (auto type = 0; type < std::size(shaders); type++)
	{
		auto sh = shaders[type];
		if (!sh)
			continue;

		if (type == ShaderTypeVertex)
		{
			context->VSSetShader(nullptr, nullptr, 0);
			context->VSSetConstantBuffers(0, 0, nullptr);

			if (sh->numTextures)
			{
				context->VSSetShaderResources(0, sh->numTextures, srNulls);
				context->VSSetSamplers(0, sh->numTextures, ssNulls);
			}

		}
		else if (type == ShaderTypePixel)
		{
			context->PSSetShader(nullptr, nullptr, 0);
			context->PSSetConstantBuffers(0, 0, nullptr);

			if (sh->numTextures)
			{
				context->PSSetShaderResources(0, sh->numTextures, srNulls);
				context->PSSetSamplers(0, sh->numTextures, ssNulls);
			}

			if (sh->numUAVs)
			{
				mRS->removeUAVs();
			}
		}
	}
}

MaterialShader::MaterialShader(LoadedShader* shader, const std::map<std::string, std::vector<float>>& defaultValues, ID3D11Device* device)
{
	this->shader = shader;
	privateCbuffers.resize(shader->compiled->privateCbuffers.size());

	for (size_t i = 0; i < privateCbuffers.size(); i++)
	{
		auto& bufferLayout = shader->compiled->privateCbuffers[i];
		auto& buffer = privateCbuffers[i];
		buffer.info = &bufferLayout;

		if (bufferLayout.size)
		{
			buffer.data.resize(bufferLayout.size / sizeof(float));
			for (auto& p : bufferLayout.params)
			{
				// defaults in material file
				auto it = defaultValues.find(p.name);
				if (it != defaultValues.end())
				{
					memcpy(buffer.data.data() + p.position / sizeof(float), it->second.data(), min(it->second.size() * sizeof(float), p.size));
				}
				// defaults in shader file
				else for (auto& def : shader->ref.privateCbuffers[bufferLayout.name])
				{
					if (def.name == p.name)
						memcpy(buffer.data.data() + p.position / sizeof(float), def.data.data(), min(def.data.size() * sizeof(float), p.size));
				}
			}

			D3D11_SUBRESOURCE_DATA initData;
			initData.pSysMem = buffer.data.data();
			initData.SysMemPitch = 0;
			initData.SysMemSlicePitch = 0;

			D3D11_BUFFER_DESC constDesc;
			ZeroMemory(&constDesc, sizeof(constDesc));
			constDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			constDesc.ByteWidth = (bufferLayout.size + 15) & ~15; //aligned to 16
			constDesc.Usage = D3D11_USAGE_DEFAULT;
			HRESULT d3dResult = device->CreateBuffer(&constDesc, &initData, &buffer.buffer);

			if (FAILED(d3dResult))
				AaLogger::logErrorD3D("Failed to create private cbuffer " + buffer.info->name, d3dResult);
		}
	}
}

MaterialShader::~MaterialShader()
{
	for (auto& m : privateCbuffers)
		m.buffer->Release();
}

void AaMaterial::setShader(LoadedShader* shader, ShaderType targetShaderType, const std::map<std::string, std::vector<float>>& defaultValues)
{
	if (shaders[targetShaderType])
		delete shaders[targetShaderType];

	shaders[targetShaderType] = new MaterialShader(shader, defaultValues, mRS->getDevice());
}

LoadedShader* AaMaterial::getShader(ShaderType targetShader)
{
	return shaders[targetShader]->shader;
}

int AaMaterial::addUAV(ID3D11UnorderedAccessView* uav, const std::string& name, ShaderType targetShaderType)
{
	MaterialShader* pShader = shaders[targetShaderType];

	if (pShader == nullptr || pShader->numUAVs == 5)
		return -1;

	pShader->UAVs[pShader->numUAVs] = uav;
	pShader->numUAVs++;
	pShader->UAVsNames.push_back(name);

	return pShader->numTextures - 1;
}

int AaMaterial::addTexture(ID3D11ShaderResourceView* textureMap, ID3D11SamplerState* textureMapSampler, const std::string& name, ShaderType targetShaderType)
{
	MaterialShader* targetShader = shaders[targetShaderType];

	targetShader->shaderMaps[targetShader->numTextures] = textureMap;
	targetShader->samplerStates[targetShader->numTextures] = textureMapSampler;
	targetShader->shaderMapNames.push_back(name);
	targetShader->numTextures++;

	return targetShader->numTextures - 1;
}

void AaMaterial::updateTexture(const std::string& name, ID3D11ShaderResourceView* textureMap, ID3D11SamplerState* textureMapSampler)
{
	for (auto s : shaders)
	{
		if (s)
		{
			for (size_t i = 0; i < s->shaderMapNames.size(); i++)
			{
				if (s->shaderMapNames[i] == name)
				{
					s->shaderMaps[i] = textureMap;
					if (textureMapSampler)
						s->samplerStates[i] = textureMapSampler;
				}
			}
		}
	}
}

void AaMaterial::updateUAV(const std::string& name, ID3D11UnorderedAccessView* uav)
{
	for (auto s : shaders)
	{
		if (s)
		{
			for (size_t i = 0; i < s->UAVsNames.size(); i++)
			{
				if (s->UAVsNames[i] == name)
				{
					s->UAVs[i] = uav;
				}
			}
		}
	}
}
