#include "AaMaterial.h"
#include "AaEntity.h"
#include "AaLogger.h"


AaMaterial::AaMaterial(std::string name,AaRenderSystem* mRenderSystem)
{
	for(int i = 0;i<5;i++)
		shaders[i] = NULL;
	
	this->name=name;
	this->mRS=mRenderSystem;
	usedVertexBuffersCount = 0;
	mInputLayout = 0;

	this->mRenderState = mRenderSystem->getDefaultRenderState();
}

AaMaterial::~AaMaterial()
{
	for(int i = 0;i<5;i++)
	{
		LoadedShader* sh = shaders[i];
		if( sh!=NULL)
		{
			delete sh;
		}
	}

	if (mInputLayout)
	{
		mInputLayout->Release();
	}
}

void AaMaterial::setMaterialConstant(std::string name, Shader_type targetShaderType, float* value)
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

	LoadedShader* sh = shaders[id];
	if( sh!=NULL)
	{
		std::map<std::string,ConstantInfo>::iterator it = sh->perMatVars.find(name);
		if(it!=sh->perMatVars.end())
		{
			memcpy((*it).second.defaultValue,value,(*it).second.size*4);
			sh->needMatConstBufferUpdate = true;
		}
	}
}

void AaMaterial::prepareForRendering()
{
	ID3D11DeviceContext* context = mRS->getContext();

	//for all possible shaders
	for(int id=0;id<5;id++)
	{
		if(shaders[id])
		{
			LoadedShader* sh = shaders[id];

			//update constant buffer if needed
			if(sh->needMatConstBufferUpdate)
			{
				float* mem = shaders[id]->perMaterialConstantsMemory;
				std::map<std::string,ConstantInfo>::iterator varsIterator = sh->perMatVars.begin();
				std::map<std::string,ConstantInfo>::iterator varsIteratorEnd = sh->perMatVars.end();	

				//for all materials
				for(;varsIterator !=varsIteratorEnd; varsIterator++)
				{
					ConstantInfo info = varsIterator->second;
					memcpy(&mem[info.position],info.defaultValue,info.size);
				}

				context->UpdateSubresource( sh->perMaterialConstantsBuffer, 0, 0, mem, 0, 0 );
			}

			ID3D11Buffer* buffersArray[3] = { mRS->perFrameBuffer,mRS->perObjectBuffer,sh->perMaterialConstantsBuffer};
			int buffStart = (sh->usedBuffersFlag & PER_FRAME) ? 0 : ((sh->usedBuffersFlag & PER_OBJECT) ? 1 : ((sh->usedBuffersFlag & PER_MATERIAL) ? 2 : -1 ));
			int buffCount = (sh->usedBuffersFlag & PER_MATERIAL) ? (3-buffStart) : ((sh->usedBuffersFlag & PER_OBJECT) ? (2-buffStart) : 1);

			if(id==0)
			{
				context->VSSetShader( (ID3D11VertexShader*) sh->shader, 0, 0 );

				if(buffStart>=0)
					context->VSSetConstantBuffers(buffStart, buffCount, &buffersArray[buffStart] );

				if(sh->numTextures>0)
				{
					context->VSSetShaderResources( 0, sh->numTextures, sh->shaderMaps );
					context->VSSetSamplers( 0, sh->numTextures, sh->samplerStates );
				}
			}
			else
			if(id==4)
			{
				context->PSSetShader( (ID3D11PixelShader*) sh->shader, 0, 0 );

				if(buffStart>=0)
					context->PSSetConstantBuffers(buffStart, buffCount, &buffersArray[buffStart] );

				if(sh->numTextures>0)
				{
					mRS->getContext()->PSSetShaderResources( 0, sh->numTextures, sh->shaderMaps );
					mRS->getContext()->PSSetSamplers( 0, sh->numTextures, sh->samplerStates );
				}

				if(sh->numUAVs)
				{
					mRS->setUAVs(1,sh->numUAVs,sh->UAVs);
				}
			}
		}	
	}

	context->IASetInputLayout( mInputLayout );
}

void AaMaterial::updateObjectConstants(AaEntity* ent)
{
	if(usedBuffersFlag & PER_OBJECT)
	{
		mRS->perObjectConstantsBuffer.worldPosition = ent->getPosition();
		XMStoreFloat4x4(&mRS->perObjectConstantsBuffer.worldViewProjectionMatrix,ent->getWorldViewProjectionMatrix());
		XMStoreFloat4x4(&mRS->perObjectConstantsBuffer.worldMatrix,XMMatrixTranspose(ent->getWorldMatrix()));

		mRS->getContext()->UpdateSubresource( mRS->perObjectBuffer, 0, 0, &mRS->perObjectConstantsBuffer, 0, 0 );
	}
}

void AaMaterial::clearAfterRendering()
{
	ID3D11DeviceContext* context = mRS->getContext();
	ID3D11ShaderResourceView* srNulls[10] = {0};
	ID3D11SamplerState* ssNulls[10] = {0};

	for(int i = 0; i<5;i++)
	{	
		LoadedShader* sh = shaders[i];
		if(sh==NULL) continue;

		if(i==0)
		{
			context->VSSetShader(0, 0, 0 );
			context->VSSetConstantBuffers(0,0,0);

			if(sh->numTextures)
			{
				context->VSSetShaderResources( 0, sh->numTextures, srNulls );
				context->VSSetSamplers( 0, sh->numTextures, ssNulls );
			}

		}
		else
		if(i==4)
		{
			context->PSSetShader(0, 0, 0 );
			context->PSSetConstantBuffers(0,0,0);

			if(sh->numTextures)
			{
				context->PSSetShaderResources( 0, sh->numTextures, srNulls );
				context->PSSetSamplers( 0, sh->numTextures, ssNulls );
			}

			if(sh->numUAVs)
			{
				mRS->removeUAVs();
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

	//set shader
	shaders[id] = new LoadedShader(shaderRef);
	usedBuffersFlag |= shaderRef->usedBuffersFlag;

	//create input layout for vs
	if (id==0)
	{
		if (mInputLayout)
		{
			mInputLayout->Release();
		}

		bool basic = true;

		// Reflect shader info
		ID3D11ShaderReflection* pVertexShaderReflection = NULL; 
		D3DReflect( shaderRef->vsBuffer->GetBufferPointer(), shaderRef->vsBuffer->GetBufferSize(), IID_ID3D11ShaderReflection, (void**) &pVertexShaderReflection);
		// Get shader info
		D3D11_SHADER_DESC shaderDesc;
		pVertexShaderReflection->GetDesc( &shaderDesc );

		// Read input layout description from shader info
		for ( UINT i=0; i< shaderDesc.InputParameters; i++ )
		{
			D3D11_SIGNATURE_PARAMETER_DESC paramDesc;       
			pVertexShaderReflection->GetInputParameterDesc(i, &paramDesc );

			if (!strcmp(paramDesc.SemanticName,"NORMAL") || !strcmp(paramDesc.SemanticName,"TANGENT"))
			{
				basic = false;
			}
		}

		//Free allocation shader reflection memory
		pVertexShaderReflection->Release();

		//input do vertex shadera
		D3D11_INPUT_ELEMENT_DESC vLp= { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 };
		D3D11_INPUT_ELEMENT_DESC vLtc = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 };

		D3D11_INPUT_ELEMENT_DESC vLn = { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 };
		D3D11_INPUT_ELEMENT_DESC vLt = { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 };

		D3D11_INPUT_ELEMENT_DESC vertexLayout[4];
		vertexLayout[0] = vLp;
		vertexLayout[1] = vLtc;
		vertexLayout[2] = vLn;
		vertexLayout[3] = vLt;

		if (basic)
		{
			HRESULT result = mRS->getDevice()->CreateInputLayout( vertexLayout,
				2, shaderRef->vsBuffer->GetBufferPointer( ), shaderRef->vsBuffer->GetBufferSize( ), &mInputLayout );
			usedVertexBuffersCount = 1;
		} 
		else
		{
			HRESULT result = mRS->getDevice()->CreateInputLayout( vertexLayout,
				4, shaderRef->vsBuffer->GetBufferPointer( ), shaderRef->vsBuffer->GetBufferSize( ), &mInputLayout );
			usedVertexBuffersCount = 2;
		}
	
	}

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

int AaMaterial::addUAV(ID3D11UnorderedAccessView* uav)
{
	LoadedShader* pShader = shaders[4];

	if(pShader == NULL || pShader->numUAVs==5)
		return -1;
	
	pShader->UAVs[pShader->numUAVs] = uav;
	pShader->numUAVs++;

	return pShader->numTextures-1;
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
