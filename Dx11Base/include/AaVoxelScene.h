#ifndef _AA_VOXELSCENE_
#define _AA_VOXELSCENE_

#include "AaSceneManager.h"

class AaVoxelScene
{

public:

	AaVoxelScene(AaSceneManager* mSceneMgr);
	~AaVoxelScene();

	void initScene(int size);
	void voxelizeScene(XMFLOAT3 orthoSize, XMFLOAT3 offset);
	void unifyVoxels();

	ID3D11Texture3D* fVoxelTexture;
	ID3D11UnorderedAccessView* fVoxelUAV;
	ID3D11ShaderResourceView* fVoxelSRV;

	ID3D11Texture3D* voxelTexture;
	ID3D11UnorderedAccessView* voxelUAV;
	ID3D11ShaderResourceView* voxelSRV;

	ID3D11Texture3D* voxelShadowTexture;
	ID3D11UnorderedAccessView* voxelShadowUAV;
	ID3D11ShaderResourceView* voxelShadowSRV;

	ID3D11Texture3D* voxelNormalTexture;
	ID3D11UnorderedAccessView* voxelNormalUAV;
	ID3D11ShaderResourceView* voxelNormalSRV;

	float size;
	AaSceneManager* mSceneMgr;
	AaMaterialLoader* mMaterialLoader;
	AaCamera* voxelizingLookCamera;
	ID3D11ComputeShader* csMixVoxels;
};


#endif