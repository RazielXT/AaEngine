#include "AaBloomPostProcess.h"

AaBloomPostProcess::AaBloomPostProcess(AaSceneManager* mSceneMgr)
{
	this->mSceneMgr = mSceneMgr;

	float windowWidth = mSceneMgr->getRenderSystem()->getWindow()->getWidth(); 
	float windowHeight = mSceneMgr->getRenderSystem()->getWindow()->getHeight(); 

	vbo = createScreenVBO();

	PpTex* sceneTex = createTexture(windowWidth,windowHeight,DXGI_FORMAT_R16G16B16A16_FLOAT);
	PpTex* lumTex = createTexture(windowWidth/4.0f,windowHeight/4.0f,DXGI_FORMAT_R16G16B16A16_FLOAT);
	PpTex* lumTex2 = createTexture(windowWidth/4.0f,windowHeight/4.0f,DXGI_FORMAT_R16G16B16A16_FLOAT);

	PpPass p;
	p.renderScene = true;
	p.target = sceneTex;
	createTextureTarget(sceneTex);
	
	PpPass p2;
	p2.target = lumTex;
	p2.input.push_back(sceneTex);
	createTextureView(sceneTex);
	createTextureTarget(lumTex);
	p2.material = mSceneMgr->getMaterial("PassLuminance");

	PpPass p3;
	p3.target = lumTex2;
	p3.input.push_back(lumTex);
	createTextureView(lumTex);
	createTextureTarget(lumTex2);
	p3.material = mSceneMgr->getMaterial("BlurX");

	PpPass p4;
	p4.target = lumTex;
	p4.input.push_back(lumTex2);
	createTextureView(lumTex2);
	p4.material = mSceneMgr->getMaterial("BlurY");

	PpPass p5;
	p5.targetBackbuffer = true;
	p5.input.push_back(sceneTex);
	p5.input.push_back(lumTex);
	p5.material = mSceneMgr->getMaterial("AddThrough");

	ppPasses.push_back(p);
	ppPasses.push_back(p2);
	ppPasses.push_back(p3);
	ppPasses.push_back(p4);
	ppPasses.push_back(p5);
}

AaBloomPostProcess::~AaBloomPostProcess()
{
	for(int i=0;i<ppTextures.size();i++)
	{
		ppTextures.at(i)->texture->Release();

		if(ppTextures.at(i)->view)
			ppTextures.at(i)->view->Release();
		if(ppTextures.at(i)->target)
			ppTextures.at(i)->target->Release();
	}

	vbo->Release();
}

void AaBloomPostProcess::render()
{
	ID3D11DeviceContext* d3dContext = mSceneMgr->getRenderSystem()->getContext();

	for(int i=0;i<ppPasses.size();i++)
	{
		PpPass p = ppPasses.at(i);

		if (p.targetBackbuffer)
		{
			mSceneMgr->getRenderSystem()->setBackbufferAsRenderTarget(p.renderScene);
		} 
		else
		{
			mSceneMgr->getRenderSystem()->setRenderTargets(1,&p.target->target,p.renderScene);

			D3D11_VIEWPORT viewport;
			viewport.Width = static_cast<float>(p.target->width);
			viewport.Height = static_cast<float>(p.target->height);
			viewport.MinDepth = 0.0f;
			viewport.MaxDepth = 1.0f;
			viewport.TopLeftX = 0.0f;
			viewport.TopLeftY = 0.0f;
			d3dContext->RSSetViewports( 1, &viewport );
		}

		if(p.renderScene)
			mSceneMgr->getRenderSystem()->clearViews();

		if(ppPasses.at(i).renderScene)
		{
			mSceneMgr->renderScene();
		}
		else
		{
			ID3D11ShaderResourceView* views[10];

			for(int i = 0; i<p.input.size();i++)
				views[i] = p.input.at(i)->view;

			p.material->prepareForRenderingWithCustomPSTextures(views,p.input.size());

			unsigned int strides = sizeof( VertexPos );
			unsigned int offsets = 0;

			d3dContext->IASetVertexBuffers( 0, 1, &vbo, &strides, &offsets );
			d3dContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
			d3dContext->Draw( 6, 0 );

			p.material->clearAfterRendering();
		}
	}
}

PpTex* AaBloomPostProcess::getTexture(int id)
{
	return ppTextures.at(id);
}

PpTex* AaBloomPostProcess::createTexture(float windowWidth, float windowHeight,DXGI_FORMAT format)
{
	PpTex* tex = new PpTex();

	tex->width = windowWidth;
	tex->height = windowHeight;
	tex->format = format;

	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory( &desc, sizeof( desc ) );
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.Format = format;
	desc.Height = windowHeight;
	desc.Width = windowWidth;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.SampleDesc.Count = 1;

	HRESULT result = mSceneMgr->getRenderSystem()->getDevice()->CreateTexture2D(&desc,0,&tex->texture);

	ppTextures.push_back(tex);

	return tex;
}

void AaBloomPostProcess::createTextureTarget(PpTex* tex)
{
	if(tex->target==NULL)
	{
		D3D11_RENDER_TARGET_VIEW_DESC desc;
		desc.Format = tex->format;
		desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipSlice = 0;
		mSceneMgr->getRenderSystem()->getDevice()->CreateRenderTargetView( tex->texture, &desc, &tex->target );
	}
}

void AaBloomPostProcess::createTextureView(PpTex* tex)
{
	if(tex->view==NULL)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
		memset( &SRVDesc, 0, sizeof( SRVDesc ) );
		SRVDesc.Format = tex->format;
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		SRVDesc.Texture2D.MipLevels = 1;

		mSceneMgr->getRenderSystem()->getDevice()->CreateShaderResourceView( tex->texture, &SRVDesc, &tex->view );
	}
}


struct VertexType
{
	XMFLOAT3 position;
	XMFLOAT2 uv;
};

ID3D11Buffer* AaBloomPostProcess::createScreenVBO()
{
	float left, right, top, bottom;
	// Calculate the screen coordinates of the left side of the window.
	left = -1;
	right = 1;
	top = 1;
	bottom = -1;

	// Create the vertex array.
	VertexType* vertices = new VertexType[6];

	vertices[0].position = XMFLOAT3(left, top, 0.0f);  // Top left.
	vertices[0].uv = XMFLOAT2(0.0f, 0.0f);

	vertices[1].position = XMFLOAT3(right, bottom, 0.0f);  // Bottom right.
	vertices[1].uv = XMFLOAT2(1.0f, 1.0f);

	vertices[2].position = XMFLOAT3(left, bottom, 0.0f);  // Bottom left.
	vertices[2].uv = XMFLOAT2(0.0f, 1.0f);

	// Second triangle.
	vertices[3].position = XMFLOAT3(left, top, 0.0f);  // Top left.
	vertices[3].uv = XMFLOAT2(0.0f, 0.0f);

	vertices[4].position = XMFLOAT3(right, top, 0.0f);  // Top right.
	vertices[4].uv = XMFLOAT2(1.0f, 0.0f);

	vertices[5].position = XMFLOAT3(right, bottom, 0.0f);  // Bottom right.
	vertices[5].uv = XMFLOAT2(1.0f, 1.0f);


	D3D11_BUFFER_DESC vertexBufferDesc;
	// Set up the description of the vertex buffer.
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.ByteWidth = sizeof(VertexType) * 6;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;
	vertexBufferDesc.MiscFlags = 0;
	vertexBufferDesc.StructureByteStride = 0;

	// Give the subresource structure a pointer to the vertex data.
	D3D11_SUBRESOURCE_DATA vertexData;
	vertexData.pSysMem = vertices;
	vertexData.SysMemPitch = 0;
	vertexData.SysMemSlicePitch = 0;

	// Now finally create the vertex buffer.
	ID3D11Buffer* vbo;
	HRESULT result = mSceneMgr->getRenderSystem()->getDevice()->CreateBuffer(&vertexBufferDesc, &vertexData, &vbo);

	delete [] vertices;

	if(FAILED(result))
	{
		return NULL;
	}

	return vbo;
}