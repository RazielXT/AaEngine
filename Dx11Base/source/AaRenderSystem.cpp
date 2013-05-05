#include "AaRenderSystem.h"
#include "ShaderConstsPtr.h"
#include <assert.h>
bool operator<(const RS_DESC& lhs, const RS_DESC& rhs)
{
	USHORT c1 = lhs.alpha_blend + lhs.depth_check*2 + lhs.depth_write*4 + lhs.culling*8;
	USHORT c2 = rhs.alpha_blend + rhs.depth_check*2 + rhs.depth_write*4 + rhs.culling*8;

	return c1<c2;
}

AaRenderSystem::AaRenderSystem(AaWindow* mWindow)
{
	this->mWindow = mWindow;
	HWND hwnd = mWindow->getHwnd();

	clearColor[0]=0.55f;
	clearColor[1]=0.75f;
	clearColor[2]=0.92f;
	clearColor[3]=1.0f;

	RECT dimensions;
	GetClientRect( hwnd, &dimensions );
	unsigned int width = dimensions.right - dimensions.left;
	unsigned int height = dimensions.bottom - dimensions.top;

	D3D_DRIVER_TYPE driverTypes[] =
	{
	D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP, D3D_DRIVER_TYPE_SOFTWARE
	};
	unsigned int totalDriverTypes = ARRAYSIZE( driverTypes );
	D3D_FEATURE_LEVEL featureLevels[] =
	{
	D3D_FEATURE_LEVEL_11_0,
	D3D_FEATURE_LEVEL_10_1,
	D3D_FEATURE_LEVEL_10_0
	};
	unsigned int totalFeatureLevels = ARRAYSIZE( featureLevels );


	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	ZeroMemory( &swapChainDesc, sizeof( swapChainDesc ) );
	swapChainDesc.BufferCount = 1;
	swapChainDesc.BufferDesc.Width = width;
	swapChainDesc.BufferDesc.Height = height;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.OutputWindow = hwnd;
	swapChainDesc.Windowed = true;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;


	D3D_DRIVER_TYPE driverType_;
	D3D_FEATURE_LEVEL featureLevel_;

	unsigned int creationFlags = 0;

	#ifdef _DEBUG
		creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
	#endif
		HRESULT result;
		unsigned int driver = 0;
		for( driver = 0; driver < totalDriverTypes; ++driver )
		{
			result = D3D11CreateDeviceAndSwapChain( 0, driverTypes[driver], 0,
			creationFlags, featureLevels, totalFeatureLevels,
			D3D11_SDK_VERSION, &swapChainDesc, &swapChain_,
			&d3dDevice_, &featureLevel_, &d3dContext_ );

			if( SUCCEEDED( result ) )
			{
			driverType_ = driverTypes[driver];
			break;
			}
		}

		//assert(SUCCEEDED( result ));
		//DXTRACE_MSG( "Failed to create the Direct3D device!" );

	ID3D11Texture2D* backBufferTexture;
	HRESULT result2 = swapChain_->GetBuffer( 0, __uuidof( ID3D11Texture2D ), ( LPVOID* )&backBufferTexture );
	
	/*if( FAILED( result ) )
	{
	DXTRACE_MSG( "Failed to get the swap chain back buffer!" );
	return false;
	}*/
	result = d3dDevice_->CreateRenderTargetView( backBufferTexture, 0, &backBufferTarget_ );
	if( backBufferTexture )
	backBufferTexture->Release( );
	
	/*if( FAILED( result ) )
	{
	DXTRACE_MSG( "Failed to create the render target view!" );
	return false;
	}*/

	//DEPTH / STENCIL
	D3D11_TEXTURE2D_DESC depthTexDesc;
	ZeroMemory( &depthTexDesc, sizeof( depthTexDesc ) );
	depthTexDesc.Width = width;
	depthTexDesc.Height = height;
	depthTexDesc.MipLevels = 1;
	depthTexDesc.ArraySize = 1;
	depthTexDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthTexDesc.SampleDesc.Count = 1;
	depthTexDesc.SampleDesc.Quality = 0;
	depthTexDesc.Usage = D3D11_USAGE_DEFAULT;
	depthTexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthTexDesc.CPUAccessFlags = 0;
	depthTexDesc.MiscFlags = 0;
	result = d3dDevice_->CreateTexture2D( &depthTexDesc, NULL, &depthTexture_ );

	if( FAILED( result ) )
	{
		DXTRACE_MSG( "Failed to create the depth texture!" );
	}
	// Create the depth stencil view
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	ZeroMemory( &descDSV, sizeof( descDSV ) );
	descDSV.Format = depthTexDesc.Format;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;
	result = d3dDevice_->CreateDepthStencilView( depthTexture_, &descDSV,&depthStencilView_ );
	depthTexture_->Release();
	currentDSView_ = depthStencilView_;

	if( FAILED( result ) )
	{
		DXTRACE_MSG( "Failed to create the depth stencil target view!" );
	}
			
	D3D11_VIEWPORT viewport;
	viewport.Width = static_cast<float>(width);
	viewport.Height = static_cast<float>(height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	d3dContext_->RSSetViewports( 1, &viewport );
	
	D3D11_RASTERIZER_DESC rasterDesc;
	rasterDesc.AntialiasedLineEnable = false;
	rasterDesc.CullMode = D3D11_CULL_BACK;
	rasterDesc.DepthBias = 0;
	rasterDesc.DepthBiasClamp = 0.0f;
	rasterDesc.DepthClipEnable = true;
	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.FrontCounterClockwise = false;
	rasterDesc.MultisampleEnable = false;
	rasterDesc.SlopeScaledDepthBias = 0.0f;
	rasterDesc.ScissorEnable=false;
	d3dDevice_->CreateRasterizerState(&rasterDesc,&rasterizerStates[1]);
	rasterDesc.CullMode = D3D11_CULL_NONE;
	d3dDevice_->CreateRasterizerState(&rasterDesc,&rasterizerStates[0]);

	D3D11_BLEND_DESC blendStateDescription;
	ZeroMemory( &blendStateDescription, sizeof( blendStateDescription ) );
	blendStateDescription.AlphaToCoverageEnable = FALSE;
	blendStateDescription.IndependentBlendEnable = FALSE;
	blendStateDescription.RenderTarget[0].BlendEnable = FALSE;
	blendStateDescription.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	blendStateDescription.RenderTarget[0].DestBlend	= D3D11_BLEND_ZERO;
	blendStateDescription.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendStateDescription.RenderTarget[0].SrcBlendAlpha	= D3D11_BLEND_ONE;
	blendStateDescription.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendStateDescription.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendStateDescription.RenderTarget[0].RenderTargetWriteMask	= D3D11_COLOR_WRITE_ENABLE_ALL;
	d3dDevice_->CreateBlendState( &blendStateDescription, &alphaBlendStates[0] );

	ZeroMemory( &blendStateDescription, sizeof( blendStateDescription ) );
	blendStateDescription.RenderTarget[0].BlendEnable = TRUE;
	blendStateDescription.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendStateDescription.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendStateDescription.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendStateDescription.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendStateDescription.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendStateDescription.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendStateDescription.RenderTarget[0].RenderTargetWriteMask = 0x0f;
	d3dDevice_->CreateBlendState( &blendStateDescription, &alphaBlendStates[1] );

	D3D11_DEPTH_STENCIL_DESC depthDisabledStencilDesc;
	depthDisabledStencilDesc.DepthEnable = false;
	depthDisabledStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthDisabledStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
	depthDisabledStencilDesc.StencilEnable = false;
	d3dDevice_->CreateDepthStencilState(&depthDisabledStencilDesc, &dsStates[0] );
	depthDisabledStencilDesc.DepthEnable = true;
	depthDisabledStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	d3dDevice_->CreateDepthStencilState(&depthDisabledStencilDesc, &dsStates[1] );
	depthDisabledStencilDesc.DepthEnable = false;
	depthDisabledStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	d3dDevice_->CreateDepthStencilState(&depthDisabledStencilDesc, &dsStates[2] );
	depthDisabledStencilDesc.DepthEnable = true;
	depthDisabledStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	d3dDevice_->CreateDepthStencilState(&depthDisabledStencilDesc, &dsStates[3] );

	RS_DESC desc;
	desc.alpha_blend = 0;
	desc.culling = 1;
	desc.depth_check = 1;
	desc.depth_write = 1;
	defaultRS = getRenderState(desc);

	D3D11_BUFFER_DESC constDesc;
	ZeroMemory( &constDesc, sizeof( constDesc ) );
	constDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	constDesc.ByteWidth = sizeof(PerFrameConstants);
	constDesc.Usage = D3D11_USAGE_DEFAULT;
	HRESULT d3dResult = d3dDevice_->CreateBuffer( &constDesc, 0, &perFrameBuffer);

	ZeroMemory( &constDesc, sizeof( constDesc ) );
	constDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	constDesc.ByteWidth = sizeof(PerObjectConstants);
	constDesc.Usage = D3D11_USAGE_DEFAULT;
	d3dResult = d3dDevice_->CreateBuffer( &constDesc, 0, &perObjectBuffer);

//	perFrameConstantsBuffer.time = 0;

	setBackbufferAsRenderTarget();
	clearViews();
	swapChain_->Present(0,0);
}

AaRenderSystem::~AaRenderSystem()
{
	perObjectBuffer->Release();
	perFrameBuffer->Release();

	std::map<RS_DESC,RenderState*>::iterator it = mRenderStates.begin();
	while(it!=mRenderStates.end())
	{
		delete it->second;
		it++;
	}

	alphaBlendStates[0]->Release();
	alphaBlendStates[1]->Release();
	dsStates[0]->Release();
	dsStates[1]->Release();
	dsStates[2]->Release();
	dsStates[3]->Release();
	rasterizerStates[0]->Release();
	rasterizerStates[1]->Release();

	if( d3dContext_ ) d3dContext_->ClearState( );

	if( backBufferTarget_ ) backBufferTarget_->Release( );
	if( depthStencilView_ ) depthStencilView_->Release( );
	if( swapChain_ ) swapChain_->Release( );
	if( d3dContext_ ) d3dContext_->Release( );
	if( d3dDevice_ ) d3dDevice_->Release( );
	d3dDevice_->Release( );
}

void AaRenderSystem::clearViews()
{
	d3dContext_->ClearRenderTargetView( *currentRTs, clearColor );
	d3dContext_->ClearDepthStencilView( currentDSView_, D3D11_CLEAR_DEPTH, 1.0f, 0 );
}

RenderState* AaRenderSystem::getDefaultRenderState()
{
	return defaultRS;
}

RenderState* AaRenderSystem::getRenderState(RS_DESC desc)
{
	std::map<RS_DESC,RenderState*>::iterator it = mRenderStates.find(desc);

	if(it!=mRenderStates.end())
		return it->second;
	else
	{
		RenderState* newRS = new RenderState();
		newRS->alphaBlendState_ = alphaBlendStates[desc.alpha_blend];
		newRS->rasterizerState = rasterizerStates[desc.culling];
		newRS->dsState = dsStates[desc.depth_check + desc.depth_write*2];

		mRenderStates[desc] = newRS;

		return newRS;
	}
}

void AaRenderSystem::setBackbufferAsRenderTarget(bool useDepthBuffer)
{
	D3D11_VIEWPORT viewport;
	viewport.Width = static_cast<float>(mWindow->getWidth());
	viewport.Height = static_cast<float>(mWindow->getHeight());
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	d3dContext_->RSSetViewports( 1, &viewport );

	currentRTcount = 1;
	currentRTs = &backBufferTarget_;
	currentDSView_ = depthStencilView_;

	if(useDepthBuffer)
		d3dContext_->OMSetRenderTargets( currentRTcount, currentRTs, depthStencilView_ );
	else
		d3dContext_->OMSetRenderTargets( currentRTcount, currentRTs, 0 );
}

void AaRenderSystem::setRenderTargets(UINT numCount, ID3D11RenderTargetView** views, bool useDepthBuffer)
{
	currentRTcount = numCount;
	currentRTs = views;

	if(useDepthBuffer)
		currentDSView_ =depthStencilView_;
	else
		currentDSView_ = 0;

	d3dContext_->OMSetRenderTargets( currentRTcount, currentRTs, currentDSView_ );
}

void AaRenderSystem::setRenderTargets(UINT numCount, ID3D11RenderTargetView** views, ID3D11DepthStencilView* dsView)
{
	currentRTcount = numCount;
	currentRTs = views;
	currentDSView_ = dsView;

	d3dContext_->OMSetRenderTargets( currentRTcount, currentRTs, currentDSView_ );
}

void AaRenderSystem::setUAVs(UINT num, ID3D11UnorderedAccessView** views)
{
	UINT counter[5] = {0,0,0,0,0};
	d3dContext_->OMSetRenderTargetsAndUnorderedAccessViews( currentRTcount, currentRTs, currentDSView_, currentRTcount, num, views, counter);
}

void AaRenderSystem::removeUAVs()
{
	d3dContext_->OMSetRenderTargets( currentRTcount, currentRTs, currentDSView_ );
}