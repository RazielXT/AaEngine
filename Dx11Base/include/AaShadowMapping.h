#ifndef _AA_SHADOWING_
#define _AA_SHADOWING_

#include <vector>
#include "AaSceneManager.h"
#include "AaCamera.h"

class AaShadowMapping
{
public:
	AaShadowMapping(AaSceneManager* mSM);
	~AaShadowMapping();

	void renderShadowMaps();
	void renderCaustics();

private:
	
	void updateShadowCamera();

	AaCamera* mShadowCamera;
	AaCamera* viewer;
	ID3D11DepthStencilView* dsView;
	ID3D11RenderTargetView* rtShadowMapRTViews[4];
	ID3D11ShaderResourceView* rtShadowMapSRViews[4];
	UINT shadowMapsCount;
	AaSceneManager* mSceneMgr;
	float* width, *height;
	float* shadowmapRanges;


	ID3D11UnorderedAccessView* voxelCausticUAV;
};

#endif