#include "AaShadingManager.h"
#include "ShaderConstsPtr.h"

AaShadingManager::AaShadingManager(AaRenderSystem* mRS)
{
	mRenderSystem = mRS;

	directionalLight = NULL;
	ambientColor = XMFLOAT3(0,0,0);
}

AaShadingManager::~AaShadingManager()
{
}

void AaShadingManager::updatePerFrameConstants(float timeSinceLastFrame, AaCamera* cam, AaCamera* sun)
{
	mRenderSystem->perFrameConstantsBuffer.time_delta = timeSinceLastFrame;
	mRenderSystem->perFrameConstantsBuffer.time += timeSinceLastFrame;
	mRenderSystem->perFrameConstantsBuffer.ambientColour = ambientColor;

	if(directionalLight!=NULL)
	{
		mRenderSystem->perFrameConstantsBuffer.directional_light_direction = directionalLight->direction;
		mRenderSystem->perFrameConstantsBuffer.directional_light_color = directionalLight->color;
	}
	else
	{
		mRenderSystem->perFrameConstantsBuffer.directional_light_direction = XMFLOAT3(0,0,0);
		mRenderSystem->perFrameConstantsBuffer.directional_light_color = XMFLOAT3(0,0,0);
	}
	
	mRenderSystem->perFrameConstantsBuffer.cameraPos = cam->getPosition();

	XMStoreFloat4x4(&mRenderSystem->perFrameConstantsBuffer.viewProjectionMatrix,*cam->getViewProjectionMatrix());
	XMStoreFloat4x4(&mRenderSystem->perFrameConstantsBuffer.directional_light_VPMatrix,*sun->getViewProjectionMatrix());

	mRenderSystem->getContext()->UpdateSubresource( mRenderSystem->perFrameBuffer, 0, 0, &mRenderSystem->perFrameConstantsBuffer, 0, 0 );
}

void AaShadingManager::updatePerFrameConstants(AaCamera* cam)
{
	mRenderSystem->perFrameConstantsBuffer.cameraPos = cam->getPosition();
	XMStoreFloat4x4(&mRenderSystem->perFrameConstantsBuffer.viewProjectionMatrix,*cam->getViewProjectionMatrix());

	mRenderSystem->getContext()->UpdateSubresource( mRenderSystem->perFrameBuffer, 0, 0, &mRenderSystem->perFrameConstantsBuffer, 0, 0 );
}