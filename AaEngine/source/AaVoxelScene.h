#pragma once

#include "AaSceneManager.h"

class AaVoxelScene
{
public:

	AaVoxelScene(AaSceneManager* mSceneMgr);
	~AaVoxelScene();

	void initScene(int size);
	void voxelizeScene(XMFLOAT3 orthoSize, XMFLOAT3 offset);
	void endFrame(XMFLOAT3 orthoHalfSize, XMFLOAT3 offset);

private:

	void fadeVoxels();

	ID3D11Texture3D* finalVoxelTexture;
	ID3D11UnorderedAccessView* finalVoxelUAV;
	ID3D11ShaderResourceView* finalVoxelSRV;

	ID3D11Texture3D* bounceVoxelTexture;
	ID3D11UnorderedAccessView* bounceVoxelUAV;
	ID3D11ShaderResourceView* bounceVoxelSRV;

	ID3D11Texture3D* voxelShadowTexture;
	ID3D11UnorderedAccessView* voxelShadowUAV;
	ID3D11ShaderResourceView* voxelShadowSRV;

	float size{};
	AaSceneManager* mSceneMgr{};
	AaCamera voxelizingLookCamera;
	ID3D11ComputeShader* csFadeVoxels{};
};
