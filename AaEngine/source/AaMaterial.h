#pragma once

#include <string>
#include <vector>
#include <map>
#include "AaRenderSystem.h"
#include <functional>
#include "AaShaderManager.h"

class AaEntity;
class AaCamera;

struct MaterialShader
{
	MaterialShader(LoadedShader* ref, const std::map<std::string, std::vector<float>>& defaultValues, ID3D11Device*);
	~MaterialShader();

	int numTextures = 0;
	ID3D11ShaderResourceView* shaderMaps[10]{};
	ID3D11SamplerState* samplerStates[10]{};
	std::vector<std::string> shaderMapNames;

	int numUAVs = 0;
	ID3D11UnorderedAccessView* UAVs[5]{};
	std::vector<std::string> UAVsNames;

	struct MaterialCbuffer
	{
		CbufferInfo* info{};
		ID3D11Buffer* buffer{};
		std::vector<float> data;
		bool needsUpdate = false;
	};
	std::vector<MaterialCbuffer> privateCbuffers;

	LoadedShader* shader;
};

struct materialInfo;
class AaMaterialResources;

class AaMaterial
{
public:

	AaMaterial(std::string name,AaRenderSystem* mRenderSystem);
	~AaMaterial();

	AaRenderSystem* getRenderSystem() { return mRS; }
	int addTexture(ID3D11ShaderResourceView* textureMap, ID3D11SamplerState* textureMapSampler, const std::string& name, ShaderType targetShader);
	int addUAV(ID3D11UnorderedAccessView* uav, const std::string& name, ShaderType targetShader = ShaderType::ShaderTypePixel);

	void updateTexture(const std::string& name, ID3D11ShaderResourceView* textureMap, ID3D11SamplerState* textureMapSampler = nullptr);
	void updateUAV(const std::string& name, ID3D11UnorderedAccessView* uav);

	void setShader(LoadedShader* shader, ShaderType targetShader, const std::map<std::string, std::vector<float>>& defaultValues);
	LoadedShader* getShader(ShaderType targetShader);

	void setMaterialConstant(std::string buffer, std::string name, ShaderType shader_target, float* value);
	void updateObjectConstants(AaEntity* ent, AaCamera& camera);
	void prepareForRendering();
	void prepareForRenderingWithCustomPSTextures(ID3D11ShaderResourceView** textures, int count , int start = 0);
	void clearAfterRendering();
	void setPSTextures(ID3D11ShaderResourceView** textures,ID3D11SamplerState** samplers, int count , int start = 0);
	void clearPSTextures(int count = 10, int start = 0);

	RenderState* mRenderState{};
	MaterialShader* shaders[5]{};

	const std::string& getName() const;

private:

	std::pair<int, int> prepareCBufferForRendering(ID3D11Buffer** buffers, MaterialShader* shader);

	std::string name;
	AaRenderSystem* mRS;
	USHORT clear_flag{};
};
