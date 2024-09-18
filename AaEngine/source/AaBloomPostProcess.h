#pragma once

#include "AaSceneManager.h"

struct PpTex
{
	ID3D11Texture2D* texture{};
	ID3D11ShaderResourceView* view{};
	ID3D11RenderTargetView* target{};
	DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UINT;

	uint32_t width{}, height{};
};

struct PpPass
{
	bool targetBackbuffer{};
	bool renderScene{};

	std::vector<PpTex*> input;
	PpTex* target{};
	AaMaterial* material{};
};

class AaBloomPostProcess
{
public:

	AaBloomPostProcess(AaSceneManager* mSceneMgr);
	~AaBloomPostProcess();

	void render(AaCamera& camera);
	PpTex* getTexture(int id);

private: 

	PpTex* createTexture(uint32_t windowWidth, uint32_t windowHeight,DXGI_FORMAT format);
	void createTextureTarget(PpTex* tex);
	void createTextureView(PpTex* tex);

	AaSceneManager* mSceneMgr{};

	std::vector<PpTex*> ppTextures;
	std::vector<PpPass> ppPasses;
};
