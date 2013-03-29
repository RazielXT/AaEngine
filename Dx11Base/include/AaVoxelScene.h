#ifndef _AA_VOXELSCENE_
#define _AA_VOXELSCENE_

#include "AaRenderSystem.h"

class AaVoxelScene
{

public:

	AaVoxelScene(AaRenderSystem* mRS);
	~AaVoxelScene();

	void initScene(int size);

	ID3D11Texture3D* voxelTexture;
	ID3D11UnorderedAccessView* voxelUAV;
	ID3D11ShaderResourceView* voxelSRV;

	AaRenderSystem* mRenderSystem;

};


#endif