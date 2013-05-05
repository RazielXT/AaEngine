#include "AaVoxelScene.h"

AaVoxelScene::AaVoxelScene(AaSceneManager* mSceneMgr)
{
	this->mSceneMgr = mSceneMgr;

	voxelTexture = NULL;
	voxelSRV = NULL;
	voxelUAV = NULL;

	voxelizingLookCamera = mSceneMgr->createCamera("voxelCamera");
}

AaVoxelScene::~AaVoxelScene()
{
	//views should be released by manager

	if (voxelTexture)
		voxelTexture->Release();
	if (voxelShadowTexture)
		voxelShadowTexture->Release();
	if (voxelNormalTexture)
		voxelNormalTexture->Release();
}

void AaVoxelScene::initScene(int size)
{
	ID3D11Device* mDevice = mSceneMgr->getRenderSystem()->getDevice();

	float* initial = new float[size*size*size*4*2];
	memset(initial,0,size*size*size*4*4*2);
	this->size = size;

	/*for (int i=0;i<size;i++)
	for (int i2=0;i2<size*size;i2++)
	{
		initial[i2*3 + i*size*size*3] = 1;
		initial[i2*3 + i*size*size*3 + 1] = 1;
		initial[i2*3 + i*size*size*3 + 2] = 1;
	}*/
	
	HRESULT result;
	D3D11_SUBRESOURCE_DATA data;
	data.pSysMem = initial;

	D3D11_TEXTURE3D_DESC desc;
	ZeroMemory( &desc, sizeof( desc ) );
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.Depth = desc.Height = desc.Width = size;
	desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
	desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	desc.MipLevels = 4;

	
	//FINAL
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	int formatSize = 1;

	data.SysMemPitch = size*formatSize;
	data.SysMemSlicePitch = size*size*formatSize;

	D3D11_SUBRESOURCE_DATA data2;
	data2.pSysMem = initial;
	data2.SysMemPitch = size*formatSize/2.0f;
	data2.SysMemSlicePitch = size*size*formatSize/4.0f;
	D3D11_SUBRESOURCE_DATA data3;
	data3.pSysMem = initial;
	data3.SysMemPitch = size*formatSize/4.0f;
	data3.SysMemSlicePitch = size*size*formatSize/16.0f;
	D3D11_SUBRESOURCE_DATA data4;
	data4.pSysMem = initial;
	data4.SysMemPitch = size*formatSize/16.0f;
	data4.SysMemSlicePitch = size*size*formatSize/64.0f;
	D3D11_SUBRESOURCE_DATA datas[4] = {data,data2,data3,data4};
	result = mDevice->CreateTexture3D(&desc,datas,&fVoxelTexture);

	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	srv_desc.Format = desc.Format;
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
	srv_desc.Texture3D.MipLevels = 4;
	srv_desc.Texture3D.MostDetailedMip = 0;
	
	result = mDevice->CreateShaderResourceView(fVoxelTexture,&srv_desc,&fVoxelSRV);
	mSceneMgr->getRenderSystem()->getContext()->GenerateMips(fVoxelSRV);

	D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
	uav_desc.Format = desc.Format;
	uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
	uav_desc.Texture3D.MipSlice = 0;
	uav_desc.Texture3D.FirstWSlice = 0;
	uav_desc.Texture3D.WSize = size;

	result = mDevice->CreateUnorderedAccessView(fVoxelTexture,&uav_desc,&fVoxelUAV);

	//NORMAL
	/*desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	data.SysMemPitch = size*4;
	data.SysMemSlicePitch = size*size*4;*/
	result = mDevice->CreateTexture3D(&desc,datas,&voxelNormalTexture);
	desc.MipLevels = 1;

	srv_desc.Format = desc.Format;
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
	srv_desc.Texture3D.MipLevels = 4;
	srv_desc.Texture3D.MostDetailedMip = 0;

	result = mDevice->CreateShaderResourceView(voxelNormalTexture,&srv_desc,&voxelNormalSRV);

	uav_desc.Format = desc.Format;
	uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
	uav_desc.Texture3D.MipSlice = 0;
	uav_desc.Texture3D.FirstWSlice = 0;
	uav_desc.Texture3D.WSize = size;

	result = mDevice->CreateUnorderedAccessView(voxelNormalTexture,&uav_desc,&voxelNormalUAV);


	//COLOR
	desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	data.SysMemPitch = size;
	data.SysMemSlicePitch = size*size;
	result = mDevice->CreateTexture3D(&desc,&data,&voxelTexture);

	srv_desc.Format = desc.Format;
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
	srv_desc.Texture3D.MipLevels = 1;
	srv_desc.Texture3D.MostDetailedMip = 0;

	result = mDevice->CreateShaderResourceView(voxelTexture,&srv_desc,&voxelSRV);

	uav_desc.Format = desc.Format;
	uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
	uav_desc.Texture3D.MipSlice = 0;
	uav_desc.Texture3D.FirstWSlice = 0;
	uav_desc.Texture3D.WSize = size;

	result = mDevice->CreateUnorderedAccessView(voxelTexture,&uav_desc,&voxelUAV);

	//SHADOW
	desc.Format = DXGI_FORMAT_R32_FLOAT;
	data.SysMemPitch = size;
	data.SysMemSlicePitch = size*size;
	result = mDevice->CreateTexture3D(&desc,&data,&voxelShadowTexture);

	srv_desc.Format = desc.Format;
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
	srv_desc.Texture3D.MipLevels = 1;
	srv_desc.Texture3D.MostDetailedMip = 0;

	result = mDevice->CreateShaderResourceView(voxelShadowTexture,&srv_desc,&voxelShadowSRV);

	uav_desc.Format = desc.Format;
	uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
	uav_desc.Texture3D.MipSlice = 0;
	uav_desc.Texture3D.FirstWSlice = 0;
	uav_desc.Texture3D.WSize = size;

	result = mDevice->CreateUnorderedAccessView(voxelShadowTexture,&uav_desc,&voxelShadowUAV);

	// We load and compile the shader. If we fail, we bail out here.
	ID3DBlob* csBuffer = 0;
	ID3DBlob* errorBuffer = 0;

	DWORD shaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
	shaderFlags |= D3DCOMPILE_DEBUG;
#endif

	std::string str,strTotal;
	std::ifstream in;
	in.open("../data/gfx/unifyVoxels.cs");
	getline(in,str);

	while ( in ) 
	{
		strTotal += str+"\n";
		getline(in,str);
	}

	result = D3DCompile(strTotal.c_str(),strTotal.length(),"unifyVoxels.cs",0,0,"CSMain" , "cs_5_0" ,	shaderFlags, 0, &csBuffer, &errorBuffer );

	if( FAILED( result ) )
	{
		if( errorBuffer != 0 )
		{
			OutputDebugStringA(( char* )errorBuffer->GetBufferPointer( ));
			std::string errorMessage=( char* )errorBuffer->GetBufferPointer( );
			AaLogger::getLogger()->writeMessage("ERROR "+errorMessage);
			errorBuffer->Release();
		}
	}

	if( errorBuffer != 0 )
		errorBuffer->Release();

	result = mDevice->CreateComputeShader( csBuffer->GetBufferPointer( ),
		csBuffer->GetBufferSize( ), 0, &csMixVoxels);
	csBuffer->Release();

	mSceneMgr->getMaterialLoader()->addTextureResource(voxelSRV,"voxelScene");
	mSceneMgr->getMaterialLoader()->addUAV(voxelUAV,"voxelScene");
	mSceneMgr->getMaterialLoader()->addTextureResource(fVoxelSRV,"fVoxelScene");
	mSceneMgr->getMaterialLoader()->addUAV(fVoxelUAV,"fVoxelScene");
	mSceneMgr->getMaterialLoader()->addTextureResource(voxelShadowSRV,"voxelShadowScene");
	mSceneMgr->getMaterialLoader()->addUAV(voxelShadowUAV,"voxelShadowScene");
	mSceneMgr->getMaterialLoader()->addTextureResource(voxelNormalSRV,"voxelNormalScene");
	mSceneMgr->getMaterialLoader()->addUAV(voxelNormalUAV,"voxelNormalScene");
}

void AaVoxelScene::unifyVoxels()
{
		ID3D11DeviceContext* mContext = mSceneMgr->getRenderSystem()->getContext();

		// Some service variables
		ID3D11UnorderedAccessView* ppUAViewNULL[1] = { NULL };
		ID3D11ShaderResourceView* ppSRVNULL[2] = { NULL, NULL };

		// We now set up the shader and run it
		mContext->CSSetShader( csMixVoxels, NULL, 0 );
		ID3D11ShaderResourceView* views[2] = {voxelSRV,voxelShadowSRV};
		mContext->CSSetShaderResources( 0, 2, views );
		mContext->CSSetUnorderedAccessViews( 0, 1, &fVoxelUAV, NULL );

		mContext->Dispatch( size, size, size);

		mContext->CSSetShader( NULL, NULL, 0 );
		mContext->CSSetUnorderedAccessViews( 0, 1, ppUAViewNULL, NULL );
		mContext->CSSetShaderResources( 0, 2, ppSRVNULL );

		mSceneMgr->getRenderSystem()->getContext()->GenerateMips(fVoxelSRV);
		mSceneMgr->getRenderSystem()->getContext()->GenerateMips(voxelNormalSRV);
}

void AaVoxelScene::voxelizeScene(XMFLOAT3 orthoHalfSize, XMFLOAT3 offset)
{
	voxelizingLookCamera->setPosition(XMFLOAT3(offset.x,offset.y,offset.z-orthoHalfSize.z-1));
	voxelizingLookCamera->lookAt(offset);
	voxelizingLookCamera->setOrthograhicCamera(orthoHalfSize.x*2,orthoHalfSize.y*2,1,1+orthoHalfSize.z*2+200);

	mSceneMgr->setCurrentCamera(voxelizingLookCamera);
	
	D3D11_VIEWPORT viewport;
	viewport.Width = static_cast<float>(size);
	viewport.Height = static_cast<float>(size);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	mSceneMgr->getRenderSystem()->getContext()->RSSetViewports( 1, &viewport );
	
	mSceneMgr->getRenderSystem()->setRenderTargets(0,0,false);


	AaMaterial* voxMat = mSceneMgr->getMaterial("VoxelizationMat");
	AaMaterial* shMat = mSceneMgr->getMaterial("depthWriteAndVoxel");

	XMFLOAT3 corner(offset.x-orthoHalfSize.x,offset.y-orthoHalfSize.y,offset.z-orthoHalfSize.z);
	voxMat->setMaterialConstant("sceneCorner",Shader_type_pixel,&corner.x);
	shMat->setMaterialConstant("sceneCorner",Shader_type_pixel,&corner.x);

	float sceneToVoxel = size/(2*orthoHalfSize.x);
	voxMat->setMaterialConstant("voxelSize",Shader_type_pixel,&sceneToVoxel);
	shMat->setMaterialConstant("voxelSize",Shader_type_pixel,&sceneToVoxel);

	mSceneMgr->renderSceneWithMaterial(voxMat);

	voxelizingLookCamera->setPosition(XMFLOAT3(offset.x-orthoHalfSize.x-1,offset.y,offset.z));
	voxelizingLookCamera->lookAt(offset);
	mSceneMgr->renderSceneWithMaterial(voxMat);

	voxelizingLookCamera->setPosition(XMFLOAT3(offset.x,offset.y-orthoHalfSize.y*2-1,offset.z));
	voxelizingLookCamera->pitch(3.14/2.0f);
	mSceneMgr->renderSceneWithMaterial(voxMat);

	mSceneMgr->getRenderSystem()->getContext()->GenerateMips(fVoxelSRV);
	mSceneMgr->getRenderSystem()->getContext()->GenerateMips(voxelShadowSRV);

	float values[4] = {0,0,0,0};
	mSceneMgr->getRenderSystem()->getContext()->ClearUnorderedAccessViewFloat(voxelShadowUAV,values);
}