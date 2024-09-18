#pragma once

#include <vector>
#include <map>
#include "AaEntity.h"
#include "AaMaterialResources.h"
#include "AaModelResources.h"
#include "AaShaderBuffersManager.h"
#include "AaCamera.h"

class AaSceneManager
{
public:

	AaSceneManager(AaRenderSystem* rs);
	~AaSceneManager();

	AaEntity* createEntity(std::string name, std::string materialName, UCHAR renderqueue = 5);
	AaEntity* createEntity(std::string name, AaMaterial* material, UCHAR renderqueue = 5);
	AaEntity* getEntity(std::string name);

	void renderScene(AaCamera& camera, UCHAR minQueue = 1, UCHAR maxQueue = 5);
	void renderSceneWithMaterial(AaCamera& camera, AaMaterial* usedMaterial, bool preserveTextures = true, UCHAR minQueue = 1, UCHAR maxQueue = 5);

	AaRenderSystem* getRenderSystem() { return mRenderSystem; }

private:

	ID3D11InputLayout* getInputLayout(AaModel* model, AaMaterial* material);
	std::map<uint64_t, ID3D11InputLayout*> layoutMap;

	std::map<std::string, AaEntity*> mEntityMap;
	std::map<UCHAR, std::map<RenderState*, std::map<AaMaterial*, std::vector<AaEntity*>>>> mRenderQueue;
	AaRenderSystem* mRenderSystem;
};
