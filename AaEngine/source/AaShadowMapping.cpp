#include "AaShadowMapping.h"

AaShadowMapping::AaShadowMapping(AaSceneManager* mSM)
{
	mSceneMgr = mSM;
	shadowMapsCount = 1;

	width = new float[shadowMapsCount];
	height = new float[shadowMapsCount];

	width[0] = 1024;
	height[0] = 1024;

	shadowmapRanges = new float[shadowMapsCount];
	shadowmapRanges[0] = 100;

	ID3D11Device* d3dDevice_ = mSceneMgr->getRenderSystem()->getDevice();

	ID3D11Texture2D* depthTexture_;
	//DEPTH / STENCIL
	D3D11_TEXTURE2D_DESC depthTexDesc;
	ZeroMemory(&depthTexDesc, sizeof(depthTexDesc));
	depthTexDesc.Width = width[0];
	depthTexDesc.Height = height[0];
	depthTexDesc.MipLevels = 1;
	depthTexDesc.ArraySize = 1;
	depthTexDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthTexDesc.SampleDesc.Count = 1;
	depthTexDesc.SampleDesc.Quality = 0;
	depthTexDesc.Usage = D3D11_USAGE_DEFAULT;
	depthTexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthTexDesc.CPUAccessFlags = 0;
	depthTexDesc.MiscFlags = 0;
	d3dDevice_->CreateTexture2D(&depthTexDesc, nullptr, &depthTexture_);

	// Create the depth stencil view
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	ZeroMemory(&descDSV, sizeof(descDSV));
	descDSV.Format = depthTexDesc.Format;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;
	d3dDevice_->CreateDepthStencilView(depthTexture_, &descDSV, &dsView);
	depthTexture_->Release();

	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.Format = DXGI_FORMAT_R32G32_FLOAT;
	desc.Height = height[0];
	desc.Width = width[0];
	desc.ArraySize = 1;
	desc.SampleDesc.Count = 1;
	//desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	desc.MipLevels = 1;

	ID3D11Texture2D* smTexture_{};
	HRESULT result = d3dDevice_->CreateTexture2D(&desc, nullptr, &smTexture_);

	result = d3dDevice_->CreateRenderTargetView(smTexture_, nullptr, &rtShadowMapRTViews[0]);

	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
	memset(&SRVDesc, 0, sizeof(SRVDesc));
	SRVDesc.Format = desc.Format;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MipLevels = 1;

	result = d3dDevice_->CreateShaderResourceView(smTexture_, &SRVDesc, &rtShadowMapSRViews[0]);

	AaMaterialResources::get().addTextureResource(rtShadowMapSRViews[0], "shadowmap1");

	smTexture_->Release();
}

AaShadowMapping::~AaShadowMapping()
{
	dsView->Release();
	rtShadowMapRTViews[0]->Release();
	//rtShadowMapSRViews[0]->Release();

	delete[] width;
	delete[] height;
}

void AaShadowMapping::updateShadowCamera(XMFLOAT3 sunDir)
{
	//XMFLOAT3 vpos = viewer->getPosition();
	//float worldUnitPerTexel = 200.0f/512.0f;
	//vpos.x = floor(vpos.x/worldUnitPerTexel)*worldUnitPerTexel;
	//vpos.y = floor(vpos.y/worldUnitPerTexel)*worldUnitPerTexel;
	//vpos.z = floor(vpos.z/worldUnitPerTexel)*worldUnitPerTexel;
	shadowCamera.setPosition(XMFLOAT3(sunDir.x * -250 + 200, sunDir.y * -250, sunDir.z * -250 - 200));
	shadowCamera.lookTo(sunDir);
	shadowCamera.setOrthograhicCamera(400, 400, 1, 500);
	//mShadowCamera->setOrthograhicCamera(vpos.x-100,vpos.x+100,vpos.z-100,vpos.z+100,1,1000);
	//mShadowCamera->setPerspectiveCamera(80,1,1,2000);
}

void AaShadowMapping::renderShadowMaps()
{
	ID3D11DeviceContext* d3dDeviceContext = mSceneMgr->getRenderSystem()->getContext();
	int iCurrentCascade = 0;

	float clColor[4] = { 1,1,1,1 };
	d3dDeviceContext->ClearRenderTargetView(rtShadowMapRTViews[iCurrentCascade], clColor);
	d3dDeviceContext->ClearDepthStencilView(dsView, D3D11_CLEAR_DEPTH, 1.0f, 0);

	mSceneMgr->getRenderSystem()->setRenderTargets(1, &rtShadowMapRTViews[iCurrentCascade], dsView);

	D3D11_VIEWPORT viewport;
	viewport.Width = static_cast<float>(width[iCurrentCascade]);
	viewport.Height = static_cast<float>(height[iCurrentCascade]);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	d3dDeviceContext->RSSetViewports(1, &viewport);

	mSceneMgr->renderSceneWithMaterial(shadowCamera, AaMaterialResources::get().getMaterial("depthWrite"), true, 0, 5);
	mSceneMgr->renderSceneWithMaterial(shadowCamera, AaMaterialResources::get().getMaterial("depthWriteAndVoxel"), true, 0, 5);
}
