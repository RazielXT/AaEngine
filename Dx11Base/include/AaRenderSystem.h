#ifndef _AA_RENDER_SYSTEM_
#define _AA_RENDER_SYSTEM_

#include <windows.h>
#include<d3d11.h>
#include<d3dx11.h>
#include<D3Dcompiler.h>
#include<DxErr.h>
#include<xnamath.h>
#include "ShaderConstsPtr.h"
#include "AaWindow.h"

struct RenderState
{
	ID3D11RasterizerState* rasterizerState;
	ID3D11BlendState* alphaBlendState_;
	ID3D11DepthStencilState* dsState;
};

class AaRenderSystem
{

public:

	AaRenderSystem(AaWindow* mWindow);
	~AaRenderSystem();

	ID3D11Device* getDevice() {return d3dDevice_;}
	ID3D11DeviceContext* getContext() { return d3dContext_;}
	void clearViews();

	IDXGISwapChain* swapChain_;
	ID3D11RenderTargetView* backBufferTarget_;
	ID3D11DepthStencilView* depthStencilView_;
	AaWindow* getWindow() { return mWindow; }
	RenderState* getDefaultRenderState();

	ID3D11Buffer* perFrameBuffer;
	ID3D11Buffer* perObjectBuffer;
	PerFrameConstants perFrameConstantsBuffer;
	PerObjectConstants perObjectConstantsBuffer;

private:

	AaWindow* mWindow;
	float clearColor[4];
	ID3D11Texture2D* depthTexture_;
	ID3D11Device* d3dDevice_;
	ID3D11DeviceContext* d3dContext_;
	RenderState* defaultRS;

};

#endif