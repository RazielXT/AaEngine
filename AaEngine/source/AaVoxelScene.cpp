#include "AaVoxelScene.h"
#include "Directories.h"
#include "AaLogger.h"
#include "Math.h"

AaVoxelScene::AaVoxelScene(AaSceneManager* mSceneMgr)
{
	this->mSceneMgr = mSceneMgr;

	bounceVoxelTexture = nullptr;
	bounceVoxelSRV = nullptr;
	bounceVoxelUAV = nullptr;
}

AaVoxelScene::~AaVoxelScene()
{
	//views should be released by manager

	if (finalVoxelTexture)
		finalVoxelTexture->Release();
	if (bounceVoxelTexture)
		bounceVoxelTexture->Release();
	if (voxelShadowTexture)
		voxelShadowTexture->Release();

	if (csFadeVoxels)
		csFadeVoxels->Release();
}

HRESULT createUAVTexture(UINT size, D3D11_TEXTURE3D_DESC& desc, int mips, AaSceneManager* mSceneMgr, ID3D11Device* mDevice, ID3D11Texture3D** vTexture, ID3D11UnorderedAccessView** vUAV, ID3D11ShaderResourceView** vSRV)
{
	HRESULT result = mDevice->CreateTexture3D(&desc, nullptr, vTexture);

	if (FAILED(result) || !*vTexture)
	{
		AaLogger::logErrorD3D("failed to create resource view", result);
		return result;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	srv_desc.Format = desc.Format;
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
	srv_desc.Texture3D.MipLevels = mips;
	srv_desc.Texture3D.MostDetailedMip = 0;

	result = mDevice->CreateShaderResourceView(*vTexture, &srv_desc, vSRV);

	if (FAILED(result) || !*vSRV)
	{
		AaLogger::logErrorD3D("failed to create resource view", result);
		return result;
	}

	if (mips > 1)
		mSceneMgr->getRenderSystem()->getContext()->GenerateMips(*vSRV);

	D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
	uav_desc.Format = desc.Format;
	uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
	uav_desc.Texture3D.MipSlice = 0;
	uav_desc.Texture3D.FirstWSlice = 0;
	uav_desc.Texture3D.WSize = size;

	result = mDevice->CreateUnorderedAccessView(*vTexture, &uav_desc, vUAV);

	if (FAILED(result))
	{
		AaLogger::logErrorD3D("failed to create UAV texture", result);
		return result;
	}

	return result;
}

void AaVoxelScene::initScene(int size)
{
	ID3D11Device* mDevice = mSceneMgr->getRenderSystem()->getDevice();

	this->size = size;

	D3D11_TEXTURE3D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.Depth = desc.Height = desc.Width = size;
	desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
	desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	desc.MipLevels = 0;

	//FINAL
	desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	createUAVTexture(size, desc, -1, mSceneMgr, mDevice, &finalVoxelTexture, &finalVoxelUAV, &finalVoxelSRV);

	//PAST BOUNCES
	desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	createUAVTexture(size, desc, -1, mSceneMgr, mDevice, &bounceVoxelTexture, &bounceVoxelUAV, &bounceVoxelSRV);

	//SHADOW
	desc.Format = DXGI_FORMAT_R32_FLOAT;
	createUAVTexture(size, desc, -1, mSceneMgr, mDevice, &voxelShadowTexture, &voxelShadowUAV, &voxelShadowSRV);

	UINT shaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( _DEBUG )
	shaderFlags |= D3DCOMPILE_DEBUG;
#endif

	std::ifstream csFile(SHADER_HLSL_DIRECTORY + "csFadeVoxels.hlsl");
	std::string csFileStr((std::istreambuf_iterator<char>(csFile)), std::istreambuf_iterator<char>());

	ID3DBlob* csBuffer = nullptr;
	ID3DBlob* errorBuffer = nullptr;
	HRESULT result = D3DCompile(csFileStr.c_str(), csFileStr.length(), "csFadeVoxels.hlsl", nullptr, nullptr, "CSMain", "cs_5_0", shaderFlags, 0, &csBuffer, &errorBuffer);

	if (errorBuffer)
	{
		if (FAILED(result))
		{
			OutputDebugStringA((char*)errorBuffer->GetBufferPointer());
			AaLogger::log((char*)errorBuffer->GetBufferPointer());
		}

		errorBuffer->Release();
	}

	result = mDevice->CreateComputeShader(csBuffer->GetBufferPointer(),	csBuffer->GetBufferSize(), nullptr, &csFadeVoxels);
	csBuffer->Release();

	AaMaterialResources::get().addTextureResource(bounceVoxelSRV, "previousBounces");
	AaMaterialResources::get().addUAV(bounceVoxelUAV, "previousBounces");

	AaMaterialResources::get().addTextureResource(finalVoxelSRV, "finalVoxelScene");
	AaMaterialResources::get().addUAV(finalVoxelUAV, "finalVoxelScene");
	AaMaterialResources::get().addTextureResource(voxelShadowSRV, "voxelShadowScene");
	AaMaterialResources::get().addUAV(voxelShadowUAV, "voxelShadowScene");
}

void AaVoxelScene::fadeVoxels()
{
	UINT row_count = size / 8.0f; //[numthreads(8, 8, 8)]
	ID3D11DeviceContext* context = mSceneMgr->getRenderSystem()->getContext();

	// We now set up the shader and run it
	context->CSSetShader(csFadeVoxels, nullptr, 0);
	context->CSSetShaderResources(0, 1, &finalVoxelSRV);
	context->CSSetUnorderedAccessViews(0, 1, &bounceVoxelUAV, nullptr);

	context->Dispatch(row_count, row_count, row_count);

	// Unassign
	ID3D11UnorderedAccessView* ppUAViewNULL[1] = {};
	ID3D11ShaderResourceView* ppSRVNULL[1] = {};
	context->CSSetShader(nullptr, nullptr, 0);
	context->CSSetShaderResources(0, 1, ppSRVNULL);
	context->CSSetUnorderedAccessViews(0, 1, ppUAViewNULL, nullptr);
}

bool giOneStep = false;
bool giKeepStepping = false;

void AaVoxelScene::voxelizeScene(XMFLOAT3 orthoHalfSize, XMFLOAT3 offset)
{
	//clearing
	float black[4] = { 0 };
	mSceneMgr->getRenderSystem()->getContext()->ClearUnorderedAccessViewFloat(finalVoxelUAV, black);

	D3D11_VIEWPORT viewport;
	viewport.Width = static_cast<float>(size);
	viewport.Height = static_cast<float>(size);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	mSceneMgr->getRenderSystem()->getContext()->RSSetViewports(1, &viewport);
	mSceneMgr->getRenderSystem()->setRenderTargets(0, nullptr, false);

	XMFLOAT3 corner(offset.x - orthoHalfSize.x, offset.y - orthoHalfSize.y, offset.z - orthoHalfSize.z);
	float sceneToVoxel = size / (2 * orthoHalfSize.x);

	struct VoxelSizeInfo
	{
		Vector2 middleCone = { 1, 0.9 };
		Vector2 sideCone = { 2, 0.8 };
		Vector4 radius = { 2, 0, 0 ,0 };
		Vector3 voxelOffset;
		float voxelSize;
	}
	cbuffer;
	cbuffer.voxelSize = sceneToVoxel;
	cbuffer.voxelOffset = corner;
	AaShaderBuffersManager::get().setCbuffer("SceneVoxelInfo", (float*)&cbuffer, sizeof(cbuffer));

	voxelizingLookCamera.setOrthograhicCamera(orthoHalfSize.x * 2, orthoHalfSize.y * 2, 1, 1 + orthoHalfSize.z * 2 + 200);

	AaMaterial* voxMat = AaMaterialResources::get().getMaterial("VoxelizationMat");
	AaMaterial* lvoxMat = AaMaterialResources::get().getMaterial("LightVoxelizationMat");
	//AaMaterial* shMat = AaMaterialResources::get().getMaterial("depthWriteAndVoxel");

	//from all 3 axes
	voxelizingLookCamera.setPosition(XMFLOAT3(offset.x, offset.y, offset.z - orthoHalfSize.z - 1));
	voxelizingLookCamera.lookTo(XMFLOAT3(0, 0, 1));
	mSceneMgr->renderSceneWithMaterial(voxelizingLookCamera, voxMat, true, 1, 5);
	mSceneMgr->renderSceneWithMaterial(voxelizingLookCamera, lvoxMat, true, 0, 0);

	voxelizingLookCamera.setPosition(XMFLOAT3(offset.x - orthoHalfSize.x - 1, offset.y, offset.z));
	voxelizingLookCamera.lookTo(XMFLOAT3(1, 0, 0));
	mSceneMgr->renderSceneWithMaterial(voxelizingLookCamera, voxMat, true, 1, 5);
	mSceneMgr->renderSceneWithMaterial(voxelizingLookCamera, lvoxMat, true, 0, 0);

	voxelizingLookCamera.setPosition(XMFLOAT3(offset.x, offset.y - orthoHalfSize.y - 1, offset.z));
	voxelizingLookCamera.pitch(3.14 / 2.0f);
	mSceneMgr->renderSceneWithMaterial(voxelizingLookCamera, voxMat, true, 1, 5);
	mSceneMgr->renderSceneWithMaterial(voxelizingLookCamera, lvoxMat, true, 0, 0);
}

void AaVoxelScene::endFrame(XMFLOAT3 orthoHalfSize, XMFLOAT3 offset)
{
	//multibounce
	if (giOneStep || giKeepStepping)
	{
		//mSceneMgr->getRenderSystem()->getContext()->CopyResource(bounceVoxelTexture, finalVoxelTexture);
		fadeVoxels();
		mSceneMgr->getRenderSystem()->getContext()->GenerateMips(bounceVoxelSRV);
		giOneStep = false;
	}

	mSceneMgr->getRenderSystem()->getContext()->GenerateMips(finalVoxelSRV);

	float values[4] = { 0,0,0,0 };
	mSceneMgr->getRenderSystem()->getContext()->ClearUnorderedAccessViewFloat(voxelShadowUAV, values);
}
