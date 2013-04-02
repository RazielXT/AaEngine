#ifndef _AA_BPP_
#define _AA_BPP_

#include "AaSceneManager.h"

struct PpTex
{
	PpTex()
	{
		texture=NULL;
		view=NULL;
		target=NULL;
		format=DXGI_FORMAT_R8G8B8A8_UINT;
	}

	ID3D11Texture2D* texture;
	ID3D11ShaderResourceView* view;
	ID3D11RenderTargetView* target;
	DXGI_FORMAT format;

	float width, height;
};

struct PpPass
{
	PpPass()
	{
		targetBackbuffer=false;
		renderScene=false;
		target=NULL;
	}

	bool targetBackbuffer;
	bool renderScene;

	std::vector<PpTex*> input;
	PpTex* target;
	AaMaterial* material;
};

class AaBloomPostProcess
{

public:

	AaBloomPostProcess(AaSceneManager* mSceneMgr);
	~AaBloomPostProcess();

	void render();
	PpTex* getTexture(int id);

private: 

	PpTex* createTexture(float windowWidth, float windowHeight,DXGI_FORMAT format);
	void createTextureTarget(PpTex* tex);
	void createTextureView(PpTex* tex);

	ID3D11Buffer* createScreenVBO();

	//source objects
/*	ID3D11RenderTargetView* renderSceneTarget;

	ID3D11ShaderResourceView* renderSceneView;
	ID3D11RenderTargetView* sceneLuminosityTarget;

	ID3D11ShaderResourceView* sceneLuminosityView;
	ID3D11RenderTargetView* sceneLuminosityBlurXTarget;

	ID3D11ShaderResourceView* sceneLuminosityBlurXView;
	ID3D11RenderTargetView* sceneLuminosityBlurYTarget;

	ID3D11ShaderResourceView* sceneLuminosityBlurYTarget;
	//target backbuffer

	ID3D11Texture2D* sceneTex;
	ID3D11Texture2D* lum1;
	ID3D11Texture2D* lum2;*/

	AaSceneManager* mSceneMgr;

	ID3D11Buffer* vbo;
	std::vector<PpTex*> ppTextures;
	std::vector<PpPass> ppPasses;
};

#endif