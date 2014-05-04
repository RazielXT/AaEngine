#include "AaVoxelScene.h"

AaVoxelScene::AaVoxelScene(AaSceneManager* mSceneMgr)
{
	this->mSceneMgr = mSceneMgr;

	bounceVoxelTexture = NULL;
	bVoxelSRV = NULL;
	bVoxelUAV = NULL;

	voxelizingLookCamera = mSceneMgr->createCamera("voxelCamera");
}

AaVoxelScene::~AaVoxelScene()
{
	//views should be released by manager

	if (bounceVoxelTexture)
		bounceVoxelTexture->Release();
	if (voxelShadowTexture)
		voxelShadowTexture->Release();
	if (voxelNormalTexture)
		voxelNormalTexture->Release();
}

HRESULT createUAVTexture(UINT size,D3D11_TEXTURE3D_DESC& desc, int mips, AaSceneManager* mSceneMgr, ID3D11Device* mDevice, ID3D11Texture3D** vTexture, ID3D11UnorderedAccessView** vUAV, ID3D11ShaderResourceView** vSRV)
{
	HRESULT result;
	result = mDevice->CreateTexture3D(&desc,0,vTexture);


	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	srv_desc.Format = desc.Format;
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
	srv_desc.Texture3D.MipLevels = mips;
	srv_desc.Texture3D.MostDetailedMip = 0;

	result = mDevice->CreateShaderResourceView(*vTexture,&srv_desc,vSRV);
	mSceneMgr->getRenderSystem()->getContext()->GenerateMips(*vSRV);

	D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
	uav_desc.Format = desc.Format;
	uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
	uav_desc.Texture3D.MipSlice = 0;
	uav_desc.Texture3D.FirstWSlice = 0;
	uav_desc.Texture3D.WSize = size;

	result = mDevice->CreateUnorderedAccessView(*vTexture,&uav_desc,vUAV);
	AaLogger::getLogger()->writeMessage(boost::lexical_cast<std::string>(result));
	return result;
}

void AaVoxelScene::initScene(int size)
{
	ID3D11Device* mDevice = mSceneMgr->getRenderSystem()->getDevice();

	this->size = size;

	D3D11_TEXTURE3D_DESC desc;
	ZeroMemory( &desc, sizeof( desc ) );
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.Depth = desc.Height = desc.Width = size;
	desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
	desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	desc.MipLevels = 0;


	//FINAL
	desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;//DXGI_FORMAT_R8G8B8A8_UNORM;
	createUAVTexture(size, desc, -1, mSceneMgr, mDevice, &finalVoxelTexture, &fVoxelUAV, &fVoxelSRV);

	//NORMAL
	desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	createUAVTexture(size, desc, -1, mSceneMgr, mDevice, &voxelNormalTexture, &voxelNormalUAV, &voxelNormalSRV);

	//PAST BOUNCES
	desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	createUAVTexture(size, desc, -1, mSceneMgr, mDevice, &bounceVoxelTexture, &bVoxelUAV, &bVoxelSRV);

	//SHADOW
	desc.Format = DXGI_FORMAT_R32_FLOAT;
	createUAVTexture(size, desc, -1, mSceneMgr, mDevice, &voxelShadowTexture, &voxelShadowUAV, &voxelShadowSRV);

	//CAUSTICS	
	ZeroMemory( &desc, sizeof( desc ) );
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.Depth = desc.Height = desc.Width = size;
	desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	desc.MipLevels = 1;
	desc.MiscFlags = 0;
	desc.Format = DXGI_FORMAT_R32_FLOAT;
	createUAVTexture(size, desc, 1, mSceneMgr, mDevice, &causticsVoxelTexture, &voxelCausticUAV, &voxelCausticSRV);



	//CS test

	ID3DBlob* csBuffer = 0;
	ID3DBlob* errorBuffer = 0;

	DWORD shaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
	shaderFlags |= D3DCOMPILE_DEBUG;
#endif

	std::string str,strTotal;
	std::ifstream in;
	in.open("../data/gfx/mipVoxels.cs");
	getline(in,str);

	while ( in ) 
	{
		strTotal += str+"\n";
		getline(in,str);
	}

	HRESULT result = D3DCompile(strTotal.c_str(),strTotal.length(),"mipVoxels.cs",0,0,"CSMain" , "cs_5_0" ,	shaderFlags, 0, &csBuffer, &errorBuffer );

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
		csBuffer->GetBufferSize( ), 0, &csMipVoxels);
	csBuffer->Release();

	//mSceneMgr->getMaterialLoader()->addTextureResource(cVoxelSRV,"voxelScene");
	//mSceneMgr->getMaterialLoader()->addUAV(cVoxelUAV,"voxelScene");

	mSceneMgr->getMaterialLoader()->addTextureResource(bVoxelSRV,"previousBounces");
	mSceneMgr->getMaterialLoader()->addUAV(bVoxelUAV,"previousBounces");

	mSceneMgr->getMaterialLoader()->addTextureResource(fVoxelSRV,"fVoxelScene");
	mSceneMgr->getMaterialLoader()->addUAV(fVoxelUAV,"fVoxelScene");
	mSceneMgr->getMaterialLoader()->addTextureResource(voxelShadowSRV,"voxelShadowScene");
	mSceneMgr->getMaterialLoader()->addUAV(voxelShadowUAV,"voxelShadowScene");
	mSceneMgr->getMaterialLoader()->addTextureResource(voxelCausticSRV,"voxelCausticScene");
	mSceneMgr->getMaterialLoader()->addUAV(voxelCausticUAV,"voxelCausticScene");
	mSceneMgr->getMaterialLoader()->addTextureResource(voxelNormalSRV,"voxelNormalScene");
	mSceneMgr->getMaterialLoader()->addUAV(voxelNormalUAV,"voxelNormalScene");
}

void AaVoxelScene::unifyVoxels()
{
		UINT row_count = size/(float)mipmaps_count; 
		ID3D11DeviceContext* mContext = mSceneMgr->getRenderSystem()->getContext();

		// Some service variables
		ID3D11UnorderedAccessView* ppUAViewNULL[1] = { NULL };
		ID3D11ShaderResourceView* ppSRVNULL[2] = { NULL, NULL };

		// We now set up the shader and run it
		mContext->CSSetShader( csMipVoxels, NULL, 0 );
		ID3D11ShaderResourceView* views[2] = {bVoxelSRV,voxelShadowSRV};
		mContext->CSSetShaderResources( 0, 2, views );
		mContext->CSSetUnorderedAccessViews( 0, 1, &fVoxelUAV, NULL );

		mContext->Dispatch( row_count,row_count,row_count);

		mContext->CSSetShader( NULL, NULL, 0 );
		mContext->CSSetUnorderedAccessViews( 0, 1, ppUAViewNULL, NULL );
		mContext->CSSetShaderResources( 0, 2, ppSRVNULL );
}

void AaVoxelScene::customMipmapping()
{
	UINT row_count = size/8.0f; 
	ID3D11DeviceContext* mContext = mSceneMgr->getRenderSystem()->getContext();

	// Some service variables
	ID3D11UnorderedAccessView* ppUAViewNULL[3] = { NULL, NULL, NULL };
	ID3D11ShaderResourceView* ppSRVNULL[1] = { NULL };

	// We now set up the shader and run it
	mContext->CSSetShader( csMipVoxels, NULL, 0 );

	mContext->CSSetShaderResources( 0, 1, &voxelNormalSRV );
	mContext->CSSetUnorderedAccessViews( 0, 3, fVoxelUAVmip, NULL );
	mContext->Dispatch( row_count,row_count,row_count);

	mContext->CSSetShader( NULL, NULL, 0 );
	mContext->CSSetUnorderedAccessViews( 0, 3, ppUAViewNULL, NULL );
	mContext->CSSetShaderResources( 0, 1, ppSRVNULL );
}

float AaVoxelScene::stepnow = 0;

void AaVoxelScene::voxelizeScene(XMFLOAT3 orthoHalfSize, XMFLOAT3 offset)
{

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
	AaMaterial* lvoxMat = mSceneMgr->getMaterial("LightVoxelizationMat");
	AaMaterial* voxMat2 = mSceneMgr->getMaterial("PartVoxelizationMat");
	AaMaterial* shMat = mSceneMgr->getMaterial("depthWriteAndVoxel");
	AaMaterial* caMat = mSceneMgr->getMaterial("depthWriteAndCaustic");

	XMFLOAT3 corner(offset.x-orthoHalfSize.x,offset.y-orthoHalfSize.y,offset.z-orthoHalfSize.z);
	voxMat->setMaterialConstant("sceneCorner",Shader_type_pixel,&corner.x);
	lvoxMat->setMaterialConstant("sceneCorner",Shader_type_pixel,&corner.x);
	voxMat2->setMaterialConstant("sceneCorner",Shader_type_pixel,&corner.x);
	shMat->setMaterialConstant("sceneCorner",Shader_type_pixel,&corner.x);
	caMat->setMaterialConstant("sceneCorner",Shader_type_pixel,&corner.x);

	float sceneToVoxel = size/(2*orthoHalfSize.x);
	voxMat->setMaterialConstant("voxelSize",Shader_type_pixel,&sceneToVoxel);
	lvoxMat->setMaterialConstant("voxelSize",Shader_type_pixel,&sceneToVoxel);
	voxMat2->setMaterialConstant("voxelSize",Shader_type_pixel,&sceneToVoxel);
	shMat->setMaterialConstant("voxelSize",Shader_type_pixel,&sceneToVoxel);
	caMat->setMaterialConstant("voxelSize",Shader_type_pixel,&sceneToVoxel);
	

	voxelizingLookCamera->setOrthograhicCamera(orthoHalfSize.x*2,orthoHalfSize.y*2,1,1+orthoHalfSize.z*2+200);
	mSceneMgr->setCurrentCamera(voxelizingLookCamera);	

	voxelizingLookCamera->setPosition(XMFLOAT3(offset.x,offset.y,offset.z-orthoHalfSize.z-1));
	voxelizingLookCamera->lookAt(offset);	
	mSceneMgr->renderSceneWithMaterial(voxMat,true,1,5);
	mSceneMgr->renderSceneWithMaterial(lvoxMat,true,0,0);

	voxelizingLookCamera->setPosition(XMFLOAT3(offset.x-orthoHalfSize.x-1,offset.y,offset.z));
	voxelizingLookCamera->lookAt(offset);
	mSceneMgr->renderSceneWithMaterial(voxMat,true,1,5);
	mSceneMgr->renderSceneWithMaterial(lvoxMat,true,0,0);

	voxelizingLookCamera->setPosition(XMFLOAT3(offset.x,offset.y-orthoHalfSize.y*2-1,offset.z));
	voxelizingLookCamera->pitch(3.14/2.0f);
	mSceneMgr->renderSceneWithMaterial(voxMat,true,1,5);
	mSceneMgr->renderSceneWithMaterial(lvoxMat,true,0,0);

	//customMipmapping();
	//mSceneMgr->getRenderSystem()->getContext()->GenerateMips(fVoxelSRV);
	//mSceneMgr->getRenderSystem()->getContext()->GenerateMips(voxelShadowSRV);

}

void AaVoxelScene::endFrame(XMFLOAT3 orthoHalfSize, XMFLOAT3 offset)
{
	//multibounce

	if(stepnow>0)
	{
		mSceneMgr->getRenderSystem()->getContext()->CopyResource(bounceVoxelTexture,finalVoxelTexture);
		mSceneMgr->getRenderSystem()->getContext()->GenerateMips(bVoxelSRV);
		//stepnow = 0;
	}
	

	//voxelize queue end (caustics)

	D3D11_VIEWPORT viewport;
	viewport.Width = static_cast<float>(size);
	viewport.Height = static_cast<float>(size);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	mSceneMgr->getRenderSystem()->getContext()->RSSetViewports( 1, &viewport );
	mSceneMgr->getRenderSystem()->setRenderTargets(0,0,false);

	AaMaterial* voxMat = mSceneMgr->getMaterial("PartVoxelizationMat");

	mSceneMgr->setCurrentCamera(voxelizingLookCamera);	

	voxelizingLookCamera->setPosition(XMFLOAT3(offset.x,offset.y,offset.z-orthoHalfSize.z-1));
	voxelizingLookCamera->lookAt(offset);	
	mSceneMgr->renderSceneWithMaterial(voxMat,true,6,7);

	voxelizingLookCamera->setPosition(XMFLOAT3(offset.x-orthoHalfSize.x-1,offset.y,offset.z));
	voxelizingLookCamera->lookAt(offset);
	mSceneMgr->renderSceneWithMaterial(voxMat,true,6,7);

	voxelizingLookCamera->setPosition(XMFLOAT3(offset.x,offset.y-orthoHalfSize.y*2-1,offset.z));
	voxelizingLookCamera->pitch(3.14/2.0f);
	mSceneMgr->renderSceneWithMaterial(voxMat,true,6,7);

	//customMipmapping();
	mSceneMgr->getRenderSystem()->getContext()->GenerateMips(fVoxelSRV);
	//mSceneMgr->getRenderSystem()->getContext()->GenerateMips(voxelShadowSRV);

	float values[4] = {0,0,0,0};
	mSceneMgr->getRenderSystem()->getContext()->ClearUnorderedAccessViewFloat(voxelShadowUAV,values);
}