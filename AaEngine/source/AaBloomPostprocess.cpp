#include "AaBloomPostProcess.h"

AaBloomPostProcess::AaBloomPostProcess(AaSceneManager* mSceneMgr)
{
	this->mSceneMgr = mSceneMgr;

	auto windowWidth = mSceneMgr->getRenderSystem()->getWindow()->getWidth();
	auto windowHeight = mSceneMgr->getRenderSystem()->getWindow()->getHeight();

	PpTex* sceneTex = createTexture(windowWidth, windowHeight, DXGI_FORMAT_R16G16B16A16_FLOAT);
	PpTex* lumTex = createTexture(windowWidth / 4.0f, windowHeight / 4.0f, DXGI_FORMAT_R16G16B16A16_FLOAT);
	PpTex* lumTex2 = createTexture(windowWidth / 4.0f, windowHeight / 4.0f, DXGI_FORMAT_R16G16B16A16_FLOAT);

	PpPass p;
	p.renderScene = true;
	p.target = sceneTex;
	createTextureTarget(sceneTex);

	PpPass p2;
	p2.target = lumTex;
	p2.input.push_back(sceneTex);
	createTextureView(sceneTex);
	createTextureTarget(lumTex);
	p2.material = AaMaterialResources::get().getMaterial("PassLuminance");

	PpPass p3;
	p3.target = lumTex2;
	p3.input.push_back(lumTex);
	createTextureView(lumTex);
	createTextureTarget(lumTex2);
	p3.material = AaMaterialResources::get().getMaterial("BlurX");

	PpPass p4;
	p4.target = lumTex;
	p4.input.push_back(lumTex2);
	createTextureView(lumTex2);
	p4.material = AaMaterialResources::get().getMaterial("BlurY");

	PpPass p5;
	p5.targetBackbuffer = true;
	p5.input.push_back(sceneTex);
	p5.input.push_back(lumTex);
	p5.material = AaMaterialResources::get().getMaterial("AddThrough");

	ppPasses.push_back(p);
	ppPasses.push_back(p2);
	ppPasses.push_back(p3);
	ppPasses.push_back(p4);
	ppPasses.push_back(p5);
}

AaBloomPostProcess::~AaBloomPostProcess()
{
	for (auto& ppTexture : ppTextures)
	{
		ppTexture->texture->Release();

		if (ppTexture->view)
			ppTexture->view->Release();
		if (ppTexture->target)
			ppTexture->target->Release();
	}
}

struct VertexType
{
	XMFLOAT3 position;
	XMFLOAT2 uv;
};

void AaBloomPostProcess::render(AaCamera& camera)
{
	ID3D11DeviceContext* d3dContext = mSceneMgr->getRenderSystem()->getContext();

	for (auto& p : ppPasses)
	{
		if (p.targetBackbuffer)
		{
			mSceneMgr->getRenderSystem()->setBackbufferAsRenderTarget(p.renderScene);
		}
		else
		{
			mSceneMgr->getRenderSystem()->setRenderTargets(1, &p.target->target, p.renderScene);

			D3D11_VIEWPORT viewport;
			viewport.Width = static_cast<float>(p.target->width);
			viewport.Height = static_cast<float>(p.target->height);
			viewport.MinDepth = 0.0f;
			viewport.MaxDepth = 1.0f;
			viewport.TopLeftX = 0.0f;
			viewport.TopLeftY = 0.0f;
			d3dContext->RSSetViewports(1, &viewport);
		}

		if (p.renderScene)
		{
			mSceneMgr->getRenderSystem()->clearViews();
			mSceneMgr->renderSceneWithMaterial(camera, AaMaterialResources::get().getMaterial("depthWrite"), true, 1, 10);
			mSceneMgr->renderScene(camera, 0, 10);
		}
		else
		{
			ID3D11ShaderResourceView* views[10];

			for (int i = 0; i < p.input.size(); i++)
				views[i] = p.input.at(i)->view;

			p.material->prepareForRenderingWithCustomPSTextures(views, p.input.size());

			d3dContext->IASetInputLayout(nullptr);
			d3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			d3dContext->Draw(3, 0);

			p.material->clearAfterRendering();
		}
	}
}

PpTex* AaBloomPostProcess::getTexture(int id)
{
	return ppTextures.at(id);
}

PpTex* AaBloomPostProcess::createTexture(uint32_t windowWidth, uint32_t windowHeight, DXGI_FORMAT format)
{
	auto tex = new PpTex();

	tex->width = windowWidth;
	tex->height = windowHeight;
	tex->format = format;

	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.Format = format;
	desc.Height = windowHeight;
	desc.Width = windowWidth;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.SampleDesc.Count = 1;

	HRESULT result = mSceneMgr->getRenderSystem()->getDevice()->CreateTexture2D(&desc, nullptr, &tex->texture);

	ppTextures.push_back(tex);

	return tex;
}

void AaBloomPostProcess::createTextureTarget(PpTex* tex)
{
	if (tex->target == nullptr)
	{
		D3D11_RENDER_TARGET_VIEW_DESC desc;
		desc.Format = tex->format;
		desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipSlice = 0;
		mSceneMgr->getRenderSystem()->getDevice()->CreateRenderTargetView(tex->texture, &desc, &tex->target);
	}
}

void AaBloomPostProcess::createTextureView(PpTex* tex)
{
	if (tex->view == nullptr)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
		memset(&SRVDesc, 0, sizeof(SRVDesc));
		SRVDesc.Format = tex->format;
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		SRVDesc.Texture2D.MipLevels = 1;

		mSceneMgr->getRenderSystem()->getDevice()->CreateShaderResourceView(tex->texture, &SRVDesc, &tex->view);
	}
}
