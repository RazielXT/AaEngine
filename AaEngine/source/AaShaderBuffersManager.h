#pragma once

#include <vector>
#include "AaRenderSystem.h"
#include "AaCamera.h"
#include "AaSceneLight.h"
#include <string>

class AaEntity;

class AaShaderBuffersManager
{
public:

	AaShaderBuffersManager(AaRenderSystem* mRS);
	~AaShaderBuffersManager();

	static AaShaderBuffersManager& get();

	void updatePerFrameConstants(float timeSinceLastFrame, AaCamera& cam, AaCamera& sun, AaSceneLight& light);
	void updatePerFrameConstants(AaCamera& cam);

	void updateObjectConstants(AaEntity* ent, AaCamera& camera);

	ID3D11Buffer* perFrameBuffer;
	ID3D11Buffer* perObjectBuffer;

	void setCbuffer(const std::string& name, const float* data, size_t size);
	ID3D11Buffer* getCbuffer(const std::string& name);

private:
	
	AaRenderSystem* mRenderSystem;

	std::map<std::string, ID3D11Buffer*> globalCbuffers;

	XM_ALIGNED_STRUCT(16) PerFrameConstants
	{
		XMFLOAT4X4 viewProjectionMatrix;			//0-3
		XMFLOAT4X4 directional_light_VPMatrix;			//4-7
		__declspec(align(16)) XMFLOAT3 cameraPos;						//8
		__declspec(align(16)) XMFLOAT3 ambientColour;					//9
		__declspec(align(16)) XMFLOAT3 directional_light_direction;	//10
		__declspec(align(16)) XMFLOAT3 directional_light_color;		//11

		float time;								//12.x
		float time_delta;						//12.y			
	};

	XM_ALIGNED_STRUCT(16) PerObjectConstants
	{
		XMFLOAT4X4 worldViewProjectionMatrix;	//0-3
		XMFLOAT4X4 worldMatrix;					//4-7
		__declspec(align(16)) XMFLOAT3 worldPosition;				//8
	};

	PerFrameConstants perFrameConstantsBuffer{};
	PerObjectConstants perObjectConstantsBuffer{};
};
