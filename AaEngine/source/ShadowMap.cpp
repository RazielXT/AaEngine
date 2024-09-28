#include "ShadowMap.h"
#include "AaMaterialResources.h"

AaShadowMap::AaShadowMap(aa::SceneLights::Light& l) : light(l)
{
}

void AaShadowMap::init(AaRenderSystem* renderSystem)
{
	texture.Init(renderSystem->device, 512, 512, renderSystem->FrameCount);

	AaMaterialResources::get().PrepareDepthShaderResourceView(texture);
	AaTextureResources::get().setNamedTexture("ShadowMap", &texture.depthView);

	camera.setOrthograhicCamera(400, 400, 1, 1000);
	update();
}

void AaShadowMap::update()
{
	camera.lookTo(light.direction);
	camera.setPosition(XMFLOAT3{ light.direction.x * -300, light.direction.y * -300, light.direction.z * -300 });
	camera.updateMatrix();
}
