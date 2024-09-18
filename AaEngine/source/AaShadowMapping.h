#pragma once

#include <vector>
#include "AaSceneManager.h"
#include "AaCamera.h"

class AaShadowMapping
{
public:
	AaShadowMapping(AaSceneManager* mSM);
	~AaShadowMapping();

	void updateShadowCamera(XMFLOAT3 sunDir);

	void renderShadowMaps();

	AaCamera shadowCamera;

private:

	ID3D11DepthStencilView* dsView{};
	ID3D11RenderTargetView* rtShadowMapRTViews[4]{};
	ID3D11ShaderResourceView* rtShadowMapSRViews[4]{};
	UINT shadowMapsCount{};
	AaSceneManager* mSceneMgr{};
	float* width{}, *height{};
	float* shadowmapRanges{};

	ID3D11UnorderedAccessView* voxelCausticUAV{};
};
