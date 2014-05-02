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
	void customMipmapping();
	void endFrame(XMFLOAT3 orthoHalfSize, XMFLOAT3 offset);

	ID3D11Texture3D* finalVoxelTexture;
	ID3D11UnorderedAccessView* fVoxelUAV;
	ID3D11ShaderResourceView* fVoxelSRV;

	ID3D11Texture3D* colorVoxelTexture;
	ID3D11UnorderedAccessView* cVoxelUAV;
	ID3D11ShaderResourceView* cVoxelSRV;

	ID3D11Texture3D* color2VoxelTexture;
	ID3D11UnorderedAccessView* cVoxel2UAV;
	ID3D11ShaderResourceView* cVoxel2SRV;

	ID3D11Texture3D* voxelShadowTexture;
	ID3D11UnorderedAccessView* voxelShadowUAV;
	ID3D11ShaderResourceView* voxelShadowSRV;

	ID3D11Texture3D* causticsVoxelTexture;
	ID3D11UnorderedAccessView* voxelCausticUAV;
	ID3D11ShaderResourceView* voxelCausticSRV;

	ID3D11Texture3D* voxelNormalTexture;
	ID3D11UnorderedAccessView* voxelNormalUAV;
	ID3D11ShaderResourceView* voxelNormalSRV;

	//mipmapping
	ID3D11UnorderedAccessView* voxelNormalUAVmip[3];
	ID3D11UnorderedAccessView* fVoxelUAVmip[3];


	float size;
	int mipmaps_count;
	AaSceneManager* mSceneMgr;
	AaMaterialLoader* mMaterialLoader;
	AaCamera* voxelizingLookCamera;
	ID3D11ComputeShader* csMipVoxels;
};


#endif