#include "AaMaterial.h"
#include "AaEntity.h"
#include "AaLogger.h"



AaMaterial::AaMaterial(std::string name,AaRenderSystem* mRenderSystem)
{
	for(int i = 0;i<5;i++)
		shaders[i] = NULL;
	
	this->name=name;
	this->mRS=mRenderSystem;

	this->mRenderState = mRenderSystem->getDefaultRenderState();
}

AaMaterial::~AaMaterial()
{
}

void AaMaterial::updatePerMaterialConstants()
{
	//for all possible shaders
	for(int id=0;id<5;id++)
	{
		if(shaders[id] && shaders[id]->needMatConstBufferUpdate)
		{

			float* mem = shaders[id]->perMaterialConstantsMemory;
			std::map<std::string,ConstantInfo>::iterator varsIterator = shaders[id]->perMatVars.begin();
			std::map<std::string,ConstantInfo>::iterator varsIteratorEnd = shaders[id]->perMatVars.end();	

			//for all materials
			for(;varsIterator !=varsIteratorEnd; varsIterator++)
			{
				ConstantInfo info = varsIterator->second;
				memcpy(&mem[info.position],info.defaultValue,info.size);
			}

			mRS->getContext()->UpdateSubresource( shaders[id]->perMaterialConstantsBuffer, 0, 0, mem, 0, 0 );
		}
	}

	/*
	if(perMatBufferFlagsVS)
	{
		if(perMatBufferFlagsVS & SHADER_CONSTANT_DIFFUSE_COLOUR)
			*perMatConstantsVSPointers.diffuseTerm = diffuseColour;
		if(perMatBufferFlagsVS & SHADER_CONSTANT_SPECULAR_COLOUR)
			*perMatConstantsVSPointers.specularTerm = specularColour;
		if(perMatBufferFlagsVS & SHADER_CONSTANT_GLOW_POWER)
			*perMatConstantsVSPointers.glowTerm = glowPower;

		mRS->getContext()->UpdateSubresource( constantBufferPerMatVS, 0, 0, perMatAllocatedBufferVS, 0, 0 );
		mRS->getContext()->VSSetConstantBuffers( perMatBufferPositionVS, 1, &constantBufferPerMatVS );
	}

	if(perMatBufferFlagsPS)
	{
		if(perMatBufferFlagsPS & SHADER_CONSTANT_DIFFUSE_COLOUR)
			*perMatConstantsPSPointers.diffuseTerm = diffuseColour;
		if(perMatBufferFlagsPS & SHADER_CONSTANT_SPECULAR_COLOUR)
			*perMatConstantsPSPointers.specularTerm = specularColour;
		if(perMatBufferFlagsPS & SHADER_CONSTANT_GLOW_POWER)
			*perMatConstantsPSPointers.glowTerm = glowPower;

		mRS->getContext()->UpdateSubresource( constantBufferPerMatPS, 0, 0, perMatAllocatedBufferPS, 0, 0 );
		mRS->getContext()->VSSetConstantBuffers( perMatBufferPositionPS, 1, &constantBufferPerMatPS );
	}
	*/
}

void AaMaterial::updatePerObjectConstants(AaEntity* ent)
{
	
	if(usedBuffersFlag & PER_OBJECT)
	{
		mRS->perObjectConstantsBuffer.worldPosition = XMFLOAT3(1,0,0);//ent->getPosition();
		XMStoreFloat4x4(&mRS->perObjectConstantsBuffer.worldViewProjectionMatrix,ent->getWorldViewProjectionMatrix());

		mRS->getContext()->UpdateSubresource( mRS->perObjectBuffer, 0, 0, &mRS->perObjectConstantsBuffer, 0, 0 );
	}

	/*if(perObjBufferFlagsVS)
	{
		if(perObjBufferFlagsVS & SHADER_CONSTANT_WORLD_VIEW_PROJECTION)
			*perObjConstantsVSPointers.worldViewProjectionMatrix = ent->getWorldViewProjectionMatrix();
		if(perObjBufferFlagsVS & SHADER_CONSTANT_WORLD_POSITION)
			*perObjConstantsVSPointers.worldPosition = ent->getPosition();

		mRS->getContext()->UpdateSubresource( constantBufferPerObjVS, 0, 0, perObjAllocatedBufferVS, 0, 0 );
		mRS->getContext()->VSSetConstantBuffers( perObjBufferPositionVS, 1, &constantBufferPerObjVS );
	}

	if(perObjBufferFlagsPS)
	{
		if(perObjBufferFlagsPS & SHADER_CONSTANT_WORLD_VIEW_PROJECTION)
			*perObjConstantsPSPointers.worldViewProjectionMatrix = ent->getWorldViewProjectionMatrix();
		if(perObjBufferFlagsPS & SHADER_CONSTANT_WORLD_POSITION)
			*perObjConstantsPSPointers.worldPosition = ent->getPosition();

		mRS->getContext()->UpdateSubresource( constantBufferPerObjPS, 0, 0, perObjAllocatedBufferPS, 0, 0 );
		mRS->getContext()->VSSetConstantBuffers( perObjBufferPositionPS, 1, &constantBufferPerObjPS );
	}*/
}
/*
void AaMaterial::setConstantsPointers(std::vector<std::string>* constants, BUFFER_TYPE bType, SHADER_TYPE sType)
{
	short pos=0;
	
	PerMaterialConstants* pmc;
	PerObjectConstants* poc;
	float* pmb;
	float* pob;
	UCHAR* mflag;
	UCHAR* oflag;

	if(sType==SHADER_TYPE_VERTEX)
	{
		pmc=&perMatConstantsVSPointers;
		poc=&perObjConstantsVSPointers;
		pmb=perMatAllocatedBufferVS;
		pob=perObjAllocatedBufferVS;
		mflag=&perMatBufferFlagsVS;
		oflag=&perObjBufferFlagsVS;
	}
	if(sType==SHADER_TYPE_PIXEL)
	{
		pmc=&perMatConstantsPSPointers;
		poc=&perObjConstantsPSPointers;
		pmb=perMatAllocatedBufferPS;
		pob=perObjAllocatedBufferPS;
		mflag=&perMatBufferFlagsPS;
		oflag=&perObjBufferFlagsPS;
	}

	if(bType==BUFFER_TYPE_PER_MAT)
		for(int i=0;i<constants->size();i++)
		{
			std::string constant=constants->at(i);
			
			if(constant=="diffuse_colour")
			{
				pmc->diffuseTerm=(XMFLOAT3*) &pmb[pos];
				*mflag|=SHADER_CONSTANT_DIFFUSE_COLOUR;
				pos+=3;
				continue;
			}

			if(constant=="specular_colour")
			{
				pmc->specularTerm=(XMFLOAT4*) &pmb[pos];
				*mflag|=SHADER_CONSTANT_SPECULAR_COLOUR;
				pos+=3;
				continue;
			}

			if(constant=="glow_power")
			{
				pmc->glowTerm=(float*) &pmb[pos];
				*mflag|=SHADER_CONSTANT_GLOW_POWER;
				pos++;
				continue;
			}

			AaLogger::getLogger()->writeMessage("ERROR, unknown per_mat constant "+constant);
			return;
		}

	if(bType==BUFFER_TYPE_PER_OBJ)
		for(int i=0;i<constants->size();i++)
		{
			std::string constant=constants->at(i);
			
			if(constant=="world_view_projection_matrix")
			{
				poc->worldViewProjectionMatrix=(XMMATRIX*) &pob[pos];
				*oflag|=SHADER_CONSTANT_WORLD_VIEW_PROJECTION;
				pos+=16;
				continue;
			}

			if(constant=="world_position")
			{
				poc->worldPosition=(XMFLOAT3*) &pob[pos];
				*oflag|=SHADER_CONSTANT_WORLD_POSITION;
				pos+=3;
				continue;
			}


			AaLogger::getLogger()->writeMessage("ERROR, unknown per_obj constant "+constant);
			return;
		}

}*/

void AaMaterial::prepareForRendering()
{
	for(int i = 0; i<5;i++)
	{	
		if(shaders[i]==NULL) continue;

		ID3D11DeviceContext* context = mRS->getContext();
		LoadedShader* sh = shaders[i];

		ID3D11Buffer* buffersArray[3] = { mRS->perFrameBuffer,mRS->perObjectBuffer,sh->perMaterialConstantsBuffer};
		int buffStart = (sh->usedBuffersFlag & PER_FRAME) ? 0 : ((sh->usedBuffersFlag & PER_OBJECT) ? 1 : ((sh->usedBuffersFlag & PER_MATERIAL) ? 2 : -1 ));
		int buffCount = (sh->usedBuffersFlag & PER_MATERIAL) ? (3-buffStart) : ((sh->usedBuffersFlag & PER_OBJECT) ? (2-buffStart) : 1);

		if(i==0)
		{
			context->VSSetShader( (ID3D11VertexShader*) sh->shader, 0, 0 );

			if(buffStart>0)
				context->VSSetConstantBuffers(buffStart, buffCount, &buffersArray[buffStart] );

			if(sh->numTextures>0)
			{
				context->VSSetShaderResources( 0, sh->numTextures, sh->shaderMaps );
				context->VSSetSamplers( 0, sh->numTextures, sh->samplerStates );
			}
		}
		else
		if(i==4)
		{
			context->PSSetShader( (ID3D11PixelShader*) sh->shader, 0, 0 );

			if(buffStart>0)
				context->PSSetConstantBuffers(buffStart, buffCount, &buffersArray[buffStart] );

			if(sh->numTextures>0)
			{
				mRS->getContext()->PSSetShaderResources( 0, sh->numTextures, sh->shaderMaps );
				mRS->getContext()->PSSetSamplers( 0, sh->numTextures, sh->samplerStates );
			}
		}
	}
}

void AaMaterial::setShaderFromReference(shaderRef* shaderRef, Shader_type targetShaderType)
{

	int id;
	switch (targetShaderType)
	{
		case Shader_type_vertex: id = 0; break;
		case Shader_type_hull: id = 1; break;
		case Shader_type_domain: id = 2; break;
		case Shader_type_geometry: id = 3; break;
		case Shader_type_pixel: id = 4; break;
		default: return;
	}

	//deallocate if previous shader is bound
	if(shaders[id]!=NULL)
		delete shaders[id];

	//set vs shader
	shaders[id] = new LoadedShader(shaderRef);
	usedBuffersFlag |= shaderRef->usedBuffersFlag;

	//allocate buffers for cpu/gpu variables and set pointers
	if(shaderRef->perMatConstBufferSize>0)
	{
		shaders[id]->perMaterialConstantsMemory = new float[shaderRef->perMatConstBufferSize]; 
		shaders[id]->needMatConstBufferUpdate = true;

		D3D11_BUFFER_DESC constDesc;
		ZeroMemory( &constDesc, sizeof( constDesc ) );
		constDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		constDesc.ByteWidth = sizeof(float)*shaderRef->perMatConstBufferSize;
		constDesc.Usage = D3D11_USAGE_DEFAULT;
		HRESULT d3dResult = mRS->getDevice()->CreateBuffer( &constDesc, 0, &shaders[id]->perMaterialConstantsBuffer);
	}

}

int AaMaterial::addTexture(ID3D11ShaderResourceView* textureMap, Shader_type targetShader)
{
	ID3D11SamplerState* textureMapSampler;
	D3D11_SAMPLER_DESC textureMapDesc;
	ZeroMemory( &textureMapDesc, sizeof( textureMapDesc ) );
	textureMapDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	textureMapDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	textureMapDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	textureMapDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	textureMapDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	textureMapDesc.MaxAnisotropy = 8;
	textureMapDesc.MaxLOD = D3D11_FLOAT32_MAX;
	HRESULT d3dResult = mRS->getDevice()->CreateSamplerState( &textureMapDesc, &textureMapSampler );

	if( FAILED( d3dResult ) )
	{
		DXTRACE_MSG( "Failed to create color map sampler state!" );
		AaLogger::getLogger()->writeMessage("ERROR Failed to create color map sampler state" );
		return -1;
	}

	int r = addTexture(textureMap, textureMapSampler,targetShader);

	if(r==-1) textureMapSampler->Release();

	return r;
}


int AaMaterial::addTexture(ID3D11ShaderResourceView* textureMap, ID3D11SamplerState* textureMapSampler, Shader_type targetShaderType)
{
	int target;
	switch (targetShaderType)
	{
		case Shader_type_vertex: target = 0; break;
		case Shader_type_geometry: target = 3; break;
		case Shader_type_pixel: target = 4; break;
			default: return -1; break;
	}

	if(target<0 || shaders[target] == NULL || shaders[target]->numTextures==10)
		return -1;

	LoadedShader* targetShader = shaders[target];

	targetShader->shaderMaps[targetShader->numTextures]=textureMap;
	targetShader->samplerStates[targetShader->numTextures]=textureMapSampler;
	targetShader->numTextures++;

	return targetShader->numTextures-1;
}
