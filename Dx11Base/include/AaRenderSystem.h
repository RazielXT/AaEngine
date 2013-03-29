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
#include <map>

struct RenderState
{
	ID3D11RasterizerState* rasterizerState;
	ID3D11BlendState* alphaBlendState_;
	ID3D11DepthStencilState* dsState;
};

struct RS_DESC
{
	RS_DESC()
	{
		alpha_blend = 0;
		depth_check = 1;
		depth_write = 1;
		culling = 1;
	}

	UCHAR alpha_blend;
	UCHAR depth_check;
	UCHAR depth_write;
	UCHAR culling;
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
	RenderState* getRenderState(RS_DESC desc);

	ID3D11Buffer* perFrameBuffer;
	ID3D11Buffer* perObjectBuffer;
	PerFrameConstants perFrameConstantsBuffer;
	PerObjectConstants perObjectConstantsBuffer;

	void setBackbufferAsRenderTarget();
	void setRenderTargets(UINT numCount, ID3D11RenderTargetView** views);
	void setUAVs(UINT startSlot, UINT num, ID3D11UnorderedAccessView** views);
	void removeUAVs();

private:

	ID3D11RasterizerState* rasterizerStates[2];
	ID3D11BlendState* alphaBlendStates[2];
	ID3D11DepthStencilState* dsStates[4];

	ID3D11RenderTargetView** currentRTs;
	UINT currentRTcount;

	AaWindow* mWindow;
	float clearColor[4];
	ID3D11Texture2D* depthTexture_;
	ID3D11Device* d3dDevice_;
	ID3D11DeviceContext* d3dContext_;
	RenderState* defaultRS;

	std::map<RS_DESC,RenderState*> mRenderStates;

};

#endif