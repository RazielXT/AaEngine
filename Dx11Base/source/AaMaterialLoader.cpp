#include "AaMaterialLoader.h"
#include "AaLogger.h"
#include "AaMaterialFileParser.h"
#include "GlobalDefinitions.h"


AaMaterialLoader::AaMaterialLoader(AaRenderSystem* mRenderSystem)
{
	this->mRenderSystem=mRenderSystem;

	std::string filePosition=TEXTURE_DIRECTORY;
	filePosition.append("Default.png");

	HRESULT d3dResult = D3DX11CreateShaderResourceViewFromFile( mRenderSystem->getDevice(), filePosition.c_str(), 0, 0, &defaultTexture, 0 );

	if( FAILED( d3dResult ) )
	{
		DXTRACE_MSG( "Failed to load the default texture image!" );	
		AaLogger::getLogger()->writeMessage("ERROR Failed to load the default texture image!");
	}
	else
	AaLogger::getLogger()->writeMessage("Loaded texture image Default.png");

	//defaultFiltering
	D3D11_SAMPLER_DESC textureMapDesc;
	ZeroMemory( &textureMapDesc, sizeof( textureMapDesc ) );
	textureMapDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	textureMapDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	textureMapDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	textureMapDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	textureMapDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	textureMapDesc.MaxAnisotropy = 8;
	textureMapDesc.MaxLOD = D3D11_FLOAT32_MAX;
	d3dResult = mRenderSystem->getDevice()->CreateSamplerState( &textureMapDesc, &defaultMapSampler );

	if( FAILED( d3dResult ) )
	{
		DXTRACE_MSG( "Failed to create default sampler state!" );
		AaLogger::getLogger()->writeMessage("ERROR Failed to create default sampler state" );
	}

}

AaMaterialLoader::~AaMaterialLoader()
{
	for (int i = 0; i< createdSamplers.size(); i++)
	{
		createdSamplers.at(i)->Release();
	}

	std::map<std::string,ID3D11ShaderResourceView*>::iterator it = loadedTextures.begin();

	while(it != loadedTextures.end())
	{
		it->second->Release();
		it++;
	}

	std::map<std::string,ID3D11UnorderedAccessView*>::iterator it3 = loadedUAVs.begin();

	while(it3 != loadedUAVs.end())
	{
		it3->second->Release();
		it3++;
	}

	loadedTextures.clear();

	std::map<std::string,shaderRef>::iterator it2 = loadedVertexShaders.begin();
	
	while(it2 != loadedVertexShaders.end())
	{
		((ID3D11VertexShader*)it2->second.shader)->Release();
		it2->second.vsBuffer->Release();
		it2++;
	}

	loadedVertexShaders.clear();

	it2 = loadedPixelShaders.begin();
	
	while(it2 != loadedPixelShaders.end())
	{
		((ID3D11VertexShader*)it2->second.shader)->Release();
		it2++;
	}

	loadedPixelShaders.clear();

	defaultTexture->Release();
	defaultMapSampler->Release();
}

void AaMaterialLoader::addUAV(ID3D11UnorderedAccessView* view, std::string name)
{
	loadedUAVs[name] = view;
}

void AaMaterialLoader::addSRV(ID3D11ShaderResourceView* view, std::string name)
{
	loadedSRVs[name] = view;
}

ID3D11SamplerState* AaMaterialLoader::createSampler(textureInfo info)
{
	ID3D11SamplerState* targetSampler;
	//defaultFiltering
	D3D11_SAMPLER_DESC textureMapDesc;
	ZeroMemory( &textureMapDesc, sizeof( textureMapDesc ) );

	if (info.bordering==TextureBorder_Clamp)
	{
		textureMapDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		textureMapDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		textureMapDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	}
	else
	if (info.bordering==TextureBorder_BorderColor)
	{
		textureMapDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
		textureMapDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
		textureMapDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
		textureMapDesc.BorderColor[0] = info.border_color[0];
		textureMapDesc.BorderColor[1] = info.border_color[1];
		textureMapDesc.BorderColor[2] = info.border_color[2];
		textureMapDesc.BorderColor[3] = info.border_color[3];
	}
	else
	{
		textureMapDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		textureMapDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		textureMapDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	}

	textureMapDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;

	if (info.filter==None)
	{
		textureMapDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	}
	else
	if (info.filter==Bilinear)
	{
		textureMapDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
	}
	else
	{
		textureMapDesc.Filter = D3D11_FILTER_ANISOTROPIC;
		textureMapDesc.MaxAnisotropy = info.maxAnisotropy;
	}

	textureMapDesc.MaxLOD = D3D11_FLOAT32_MAX;
	mRenderSystem->getDevice()->CreateSamplerState( &textureMapDesc, &targetSampler );

	createdSamplers.push_back(targetSampler);

	return targetSampler;
}

std::map<std::string,AaMaterial*> AaMaterialLoader::loadMaterials(std::string directory, bool subDirectories)
{
	std::vector<materialInfo> matInfos=fileParser.parseAllMaterialFiles(directory,subDirectories);  
	std::map<std::string,AaMaterial*> loadedMaterials;
	shaderRef vs;
	shaderRef ps;

	for(int i=0;i<matInfos.size();i++)
	{
		materialInfo curMat=matInfos.at(i);

		std::map<std::string,AaMaterial*>::iterator itM = loadedMaterials.find(curMat.name);
	
		if(itM!=loadedMaterials.end())
		{
			AaLogger::getLogger()->writeMessage("WARNING attempted creation of material with existing name "+curMat.name);
			continue;
		}

		//create material
		AaMaterial* mat=new AaMaterial(curMat.name,mRenderSystem);
		mat->mRenderState = mRenderSystem->getRenderState(curMat.renderStateDesc);

		std::map<std::string,shaderRef>::iterator it;

		//set shader programs loaded from shd file
		if(!curMat.vs_ref.empty())
		{	
			it=loadedVertexShaders.find(curMat.vs_ref);
			if(it==loadedVertexShaders.end())
			{
				AaLogger::getLogger()->writeMessage("ERROR, vertex program "+curMat.vs_ref+" for material "+curMat.name+" not found!");
				continue;
			}
			else
				vs=it->second;

			mat->setShaderFromReference(&vs,Shader_type_vertex);

			//v shader textures
			for(int j=0;j<curMat.vstextures.size();j++)
			{
				ID3D11SamplerState* cSampler = defaultMapSampler;
				if(!curMat.pstextures.at(j).defaultSampler)
				{
					cSampler = createSampler(curMat.pstextures.at(j));
				}

				mat->addTexture(getTextureResource(curMat.vstextures.at(j).file),defaultMapSampler,Shader_type_vertex);
			}
		}

		if(!curMat.gs_ref.empty())
		{
			/*it=loadedPixelShaders.find(curMat.ps_ref);
			if(it==loadedPixelShaders.end())
			{
				AaLogger::getLogger()->writeMessage("ERROR, pixel program "+curMat.vs_ref+" for material "+curMat.name+" not found!");
				continue;
			}
			else
				ps=it->second;*/
		}

		if(!curMat.ps_ref.empty())
		{
			it=loadedPixelShaders.find(curMat.ps_ref);
			if(it==loadedPixelShaders.end())
			{
				AaLogger::getLogger()->writeMessage("ERROR, pixel program "+curMat.vs_ref+" for material "+curMat.name+" not found!");
				continue;
			}
			else
				ps=it->second;

			mat->setShaderFromReference(&ps,Shader_type_pixel);

			//pixel shader textures
			for(int j=0;j<curMat.pstextures.size();j++)
			{
				ID3D11SamplerState* cSampler = defaultMapSampler;
				if(!curMat.pstextures.at(j).defaultSampler)
				{
					cSampler = createSampler(curMat.pstextures.at(j));
				}

				mat->addTexture(getTextureResource(curMat.pstextures.at(j).file),cSampler ,Shader_type_pixel);
			}

			//pixel shader uav
			for(int j=0;j<curMat.psuavs.size();j++)
			{	
				mat->addUAV(loadedUAVs.find(curMat.psuavs.at(j))->second);
			}
		}


		std::map<std::string,std::vector<float>>::iterator valuesIt = curMat.defaultParams.begin();
		//for all unique default values in params
		for(;valuesIt !=curMat.defaultParams.end(); valuesIt++)
		{
			for(int i=0;i<5;i++)
			{
				if(mat->shaders[i] && mat->shaders[i]->perMatVars.find(valuesIt->first)!=mat->shaders[i]->perMatVars.end())
				{
					ConstantInfo info = mat->shaders[i]->perMatVars.find(valuesIt->first)->second;
					if(info.size!=valuesIt->second.size()*4)
					AaLogger::getLogger()->writeMessage("WARNING in material "+curMat.name +", default parameter "+valuesIt->first+" should have "+boost::lexical_cast<std::string,int>((int)info.size/4.0f)+ " values!");
					else
					{
						for(int i2=0;i2<valuesIt->second.size();i2++)
							info.defaultValue[i2] = valuesIt->second.at(i2);
						mat->shaders[i]->perMatVars[valuesIt->first] = info;
					}

				}
			}
		}

		//add
		loadedMaterials.insert(std::pair<std::string,AaMaterial*>(curMat.name,mat));
	}

	return loadedMaterials;
}

void AaMaterialLoader::loadShaderReferences(std::string directory, bool subDirectories)
{
	shaderRefMaps maps=fileParser.parseAllShaderFiles(directory,subDirectories);  

	loadedVertexShaders.insert(maps.vertexShaderRefs.begin(),maps.vertexShaderRefs.end());
	loadedPixelShaders.insert(maps.pixelShaderRefs.begin(),maps.pixelShaderRefs.end());

	compileShaderReferences();
}

bool AaMaterialLoader::compileVertexShaderRef(shaderRef* reference)
{
	ID3DBlob* errorBuffer = 0;
	HRESULT result;

	
	DWORD shaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;
	#if defined( DEBUG ) || defined( _DEBUG )
	shaderFlags |= D3DCOMPILE_DEBUG;
	#endif

	#ifdef USE_D3DCOMPILE

	std::string str,strTotal;
	std::ifstream in;
	in.open(SHADER_DIRECTORY+reference->file);
	getline(in,str);

	while ( in ) 
	{
		strTotal += str+"\n";
		getline(in,str);
	}

	result = D3DCompile(strTotal.c_str(),strTotal.length(),reference->file.c_str(),0,0,reference->entry.c_str() , reference->profile.c_str() ,	shaderFlags, 0, &reference->vsBuffer, &errorBuffer );
	
	#else
		result = D3DX11CompileFromFile( filename.c_str(), 0, 0, functionName.c_str() , shaderProfile.c_str() ,	shaderFlags, 0, 0, &vsBuffer, &errorBuffer, 0 );
	#endif


	if( FAILED( result ) )
	{
		if( errorBuffer != 0 )
		{
		OutputDebugStringA(( char* )errorBuffer->GetBufferPointer( ));
		std::string errorMessage=( char* )errorBuffer->GetBufferPointer( );
		AaLogger::getLogger()->writeMessage("ERROR "+errorMessage);
		errorBuffer->Release();
		}
		return 1;
	}

	if( errorBuffer != 0 )
	errorBuffer->Release( );
	result = mRenderSystem->getDevice()->CreateVertexShader( reference->vsBuffer->GetBufferPointer( ),
	reference->vsBuffer->GetBufferSize( ), 0, ((ID3D11VertexShader**)&reference->shader) );

	if( FAILED( result ) )
	{
		if( reference->vsBuffer )
		reference->vsBuffer->Release( );
		return 1;
	}

	return 0;
}

bool AaMaterialLoader::compilePixelShaderRef(shaderRef* reference)
{
	ID3DBlob* psBuffer = 0;
	ID3DBlob* errorBuffer = 0;
	HRESULT result;

	DWORD shaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;
	#if defined( DEBUG ) || defined( _DEBUG )
		shaderFlags |= D3DCOMPILE_DEBUG;
	#endif

	#ifdef USE_D3DCOMPILE

	std::string str,strTotal;
	std::ifstream in;
	in.open(SHADER_DIRECTORY+reference->file);
	getline(in,str);

	while ( in ) 
	{
		strTotal += str+"\n";
		getline(in,str);
	}

	result = D3DCompile(strTotal.c_str(),strTotal.length(),reference->file.c_str(),0,0,reference->entry.c_str() , reference->profile.c_str() ,	shaderFlags, 0, &psBuffer, &errorBuffer );
	
	#else
		result = D3DX11CompileFromFile( filename.c_str(), 0, 0, functionName.c_str() , shaderProfile.c_str() ,	shaderFlags, 0, 0, &psBuffer, &errorBuffer, 0 );
	#endif

	if( FAILED( result ) )
	{
		if( errorBuffer != 0 )
		{
		OutputDebugStringA(( char* )errorBuffer->GetBufferPointer( ));
		std::string errorMessage=( char* )errorBuffer->GetBufferPointer( );
		AaLogger::getLogger()->writeMessage("ERROR "+errorMessage);
		errorBuffer->Release();
		}
	return 1;
	}

	if( errorBuffer != 0 )
	errorBuffer->Release();

	result = mRenderSystem->getDevice()->CreatePixelShader( psBuffer->GetBufferPointer( ),
		psBuffer->GetBufferSize( ), 0, ((ID3D11PixelShader**)&reference->shader) );
	psBuffer->Release();

	if( FAILED( result ) )
	{
		return 1;
	}

	return 0;
}


void AaMaterialLoader::compileShaderReferences()
{

	std::map<std::string,shaderRef>::iterator it=loadedVertexShaders.begin();

	for(;it!=loadedVertexShaders.end();it++)
	{
		compileVertexShaderRef(&(it->second));
	}

	std::map<std::string,shaderRef>::iterator it2=loadedPixelShaders.begin();

	for(;it2!=loadedPixelShaders.end();it2++)
	{
		compilePixelShaderRef(&(it2->second));
	}
}

void AaMaterialLoader::addTextureResource(ID3D11ShaderResourceView* view, std::string name)
{
	loadedTextures[name] = view;
}

ID3D11ShaderResourceView* AaMaterialLoader::getTextureResource(std::string file)
{
	std::map<std::string,ID3D11ShaderResourceView*>::iterator it = loadedTextures.find(file);
	
	if(it==loadedTextures.end())
	{
		ID3D11ShaderResourceView* textureMap;

		std::string filePosition=TEXTURE_DIRECTORY;
		filePosition.append(file);

		//D3DX11_IMAGE_LOAD_INFO info;
		
		ID3D11Texture2D* texture;
		HRESULT d3dResult = D3DX11CreateTextureFromFile(mRenderSystem->getDevice(), filePosition.c_str(), 0, 0, (ID3D11Resource**)&texture, 0 );
		//D3DX11CreateShaderResourceViewFromFile( mRenderSystem->getD3DDevice(), filePosition.c_str(), 0, 0, &textureMap, 0 );

		if( FAILED( d3dResult ) )
		{
			AaLogger::getLogger()->writeMessage("ERROR Failed to load the texture image "+file+"!" );
			textureMap = defaultTexture;
		}
		else
		{

			D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
			memset( &SRVDesc, 0, sizeof( SRVDesc ) );
			D3D11_TEXTURE2D_DESC desc;
			texture->GetDesc(&desc);
			SRVDesc.Format = desc.Format;
			SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			SRVDesc.Texture2D.MipLevels = -1;

			d3dResult = mRenderSystem->getDevice()->CreateShaderResourceView( texture, &SRVDesc, &textureMap );

			texture->Release();

			if( FAILED( d3dResult ) )
			{
				AaLogger::getLogger()->writeMessage("ERROR Failed to create SRV for texture image "+file+"!" );
				textureMap = defaultTexture;
			}
			else
			{
				loadedTextures[file]=textureMap;
				AaLogger::getLogger()->writeMessage("Loaded texture image "+file);
			}
		}

		return textureMap;
	}

	return it->second;
}
