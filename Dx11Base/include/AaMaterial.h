#ifndef _AA_MATERIAL_
#define _AA_MATERIAL_

#include <string>
#include <vector>
#include <map>
#include "AaRenderSystem.h"
#include "ShaderConstsPtr.h"

#define PER_FRAME 1
#define PER_MATERIAL 2
#define PER_OBJECT 4

/*
#define VERTEX_SHADER 1
#define GEOMETRY_SHADER 2
#define PIXEL_SHADER 4
*/

class AaEntity;

enum Shader_type
{
	Shader_type_vertex,Shader_type_hull,Shader_type_domain,Shader_type_geometry,Shader_type_pixel
};

struct ConstantInfo
{
	short position;
	char size;
	float defaultValue[16];
};

struct shaderRef
{
	std::string file;
	std::string entry;
	std::string profile;

	UCHAR usedBuffersFlag;
	short perMatConstBufferSize;
	std::map<std::string,ConstantInfo> perMatVars;

	void* shader;
	ID3DBlob* vsBuffer;
};

struct LoadedShader
{
	LoadedShader(shaderRef* ref)
	{
		perMaterialConstantsBufferSize = ref->perMatConstBufferSize;
		perMatVars.insert(ref->perMatVars.begin(),ref->perMatVars.end());
		shader = ref->shader;
		vsBuffer = ref->vsBuffer;

		numTextures = 0;
		numUAVs = 0;
		perMaterialConstantsMemory = NULL;
		needMatConstBufferUpdate = false;
		usedBuffersFlag = ref->usedBuffersFlag;
	}

	~LoadedShader()
	{
		if(perMaterialConstantsMemory!=NULL)
		{
			delete[] perMaterialConstantsMemory;
			perMaterialConstantsBuffer->Release();
 		}
	}


	void* shader;
	ID3DBlob* vsBuffer;

	int numTextures;
	ID3D11ShaderResourceView* shaderMaps[10];
	ID3D11SamplerState* samplerStates[10];

	int numUAVs;
	ID3D11UnorderedAccessView* UAVs[5];

	UCHAR usedBuffersFlag;

	short perMaterialConstantsBufferSize;
	std::map<std::string,ConstantInfo> perMatVars;
	ID3D11Buffer* perMaterialConstantsBuffer;
	float* perMaterialConstantsMemory;
	bool needMatConstBufferUpdate;

};

class AaMaterial
{

public:

	AaMaterial(std::string name,AaRenderSystem* mRenderSystem);
	~AaMaterial();

	AaRenderSystem* getRenderSystem() { return mRS; }
	int addTexture(ID3D11ShaderResourceView* textureMap, ID3D11SamplerState* textureMapSampler, Shader_type targetShader);
	int addTexture(ID3D11ShaderResourceView* textureMap, Shader_type targetShader);
	int addUAV(ID3D11UnorderedAccessView* uav);

	void setShaderFromReference(shaderRef* shaderRef, Shader_type targetShader);

	void setMaterialConstant(std::string name, Shader_type shader_target, float* value);
	void updateObjectConstants(AaEntity* ent);
	void prepareForRendering();
	void prepareForRenderingWithCustomPSTextures(ID3D11ShaderResourceView** textures, int count , int start = 0);
	void clearAfterRendering();

	ID3D11InputLayout* mInputLayout; 
	USHORT usedVertexBuffersCount;
	RenderState* mRenderState;
	LoadedShader* shaders[5];


private:

	std::string name;
	AaRenderSystem* mRS;
	UCHAR usedBuffersFlag;
	USHORT clear_flag;

};

#endif