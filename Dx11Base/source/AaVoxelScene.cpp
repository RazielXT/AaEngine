#include "AaVoxelScene.h"

AaVoxelScene::AaVoxelScene(AaSceneManager* mSceneMgr)
{
	mRenderSystem = mSceneMgr->getRenderSystem();
	mMaterialLoader = mSceneMgr->getMaterialLoader();

	voxelTexture = NULL;
	voxelSRV = NULL;
	voxelUAV = NULL;
}

AaVoxelScene::~AaVoxelScene()
{
	//views should be released by manager

	if (voxelTexture)
		voxelTexture->Release();
}

void AaVoxelScene::initScene(int size)
{
	float* initial = new float[size*size*size*4];
	memset(initial,0,size*size*size*4*4);

	for (int i=0;i<size;i++)
	for (int i2=0;i2<size*size;i2++)
	{
		initial[i2*4 + i*size*size*4] = 1;
		initial[i2*4 + i*size*size*4 + 1] = 0;
		initial[i2*4 + i*size*size*4 + 2] = 0;
		initial[i2*4 + i*size*size*4 + 3] = 0;
	}
	
	HRESULT result;
	D3D11_SUBRESOURCE_DATA data;
	data.pSysMem = initial;
	data.SysMemPitch = size*4;
	data.SysMemSlicePitch = size*size*4;

	D3D11_TEXTURE3D_DESC desc;
	ZeroMemory( &desc, sizeof( desc ) );
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	desc.Depth = desc.Height = desc.Width = size;
	//desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
	desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	desc.MipLevels = 1;

	result = mRenderSystem->getDevice()->CreateTexture3D(&desc,&data,&voxelTexture);

	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	srv_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
	srv_desc.Texture3D.MipLevels = 1;
	srv_desc.Texture3D.MostDetailedMip = 0;

	result = mRenderSystem->getDevice()->CreateShaderResourceView(voxelTexture,&srv_desc,&voxelSRV);

	D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
	uav_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
	uav_desc.Texture3D.MipSlice = 0;
	uav_desc.Texture3D.FirstWSlice = 0;
	uav_desc.Texture3D.WSize = size;

	result = mRenderSystem->getDevice()->CreateUnorderedAccessView(voxelTexture,&uav_desc,&voxelUAV);

	mMaterialLoader->addTextureResource(voxelSRV,"voxelScene");
	mMaterialLoader->addUAV(voxelUAV,"voxelScene");
}