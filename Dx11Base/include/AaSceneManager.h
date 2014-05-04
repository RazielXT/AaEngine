#ifndef _AA_SCENE_MANAGER_
#define _AA_SCENE_MANAGER_

#include <vector>
#include <map>
#include "AaLogger.h"
#include "AaEntity.h"
#include "AaMaterialLoader.h"
#include "AaModelLoader.h"
#include "AaShadingManager.h"
#include "AaCamera.h"
#include "AaGuiSystem.h"
#include <unordered_map>


struct materialWithEntities
{
	AaMaterial* material;
	std::vector<AaEntity*> entities;
};


class AaSceneManager
{

public:
	
	AaSceneManager(AaRenderSystem* rs, AaGuiSystem* guiMgr);
	~AaSceneManager();

	void loadMaterialFiles(std::string directory,bool subDirectories=false);

	AaEntity* createEntity(std::string name,std::string materialName, UCHAR renderqueue = 5);
	AaEntity* createEntity(std::string name,AaMaterial* material, UCHAR renderqueue = 5);
	AaEntity* createEntity(std::string name);
	AaEntity* getEntity(std::string name);
	void removeEntity(AaEntity* ent);
	void entityChangedMaterial(AaEntity* ent,AaMaterial* mat);

	void generalScale(float scale);

	AaMaterial* createMaterial(std::string name);
	AaMaterial* getMaterial(std::string name);
	void setDefaultMaterial(AaMaterial* material);
	AaMaterial* getDefaultMaterial() {return defaultMaterial;}

	AaModelInfo* getModel(std::string name);

	void setCurrentCamera(AaCamera* cam);
	AaCamera* createCamera(std::string name);
	AaCamera* getCamera(std::string name);
	AaCamera* getCamera();
	
	void renderScene(UCHAR minQueue = 1, UCHAR maxQueue = 5);
	void renderSceneWithMaterial(AaMaterial* usedMaterial, bool preserveTextures = true, UCHAR minQueue = 1, UCHAR maxQueue = 5);


	AaMaterialLoader* getMaterialLoader() {return mMaterialLoader;}
	AaRenderSystem* getRenderSystem() {return mRenderSystem;}
	AaGuiSystem* getGuiManager() {return mGuiMgr;}
	AaShadingManager* mShadingMgr;

private:

	std::map<std::string,AaCamera*> mCameraMap;
	std::unordered_map<std::string,AaEntity*> mEntityMap;
	std::map<std::string,AaMaterial*> mMaterialMap;
	std::map<std::string,AaModelInfo*> mModelMap;
	std::map<char,std::map<RenderState*,std::map<AaMaterial*,std::vector<AaEntity*>>>> mRenderQueue;
	AaMaterialLoader* mMaterialLoader;
	AaModelLoader* mModelLoader;
	AaMaterial* defaultMaterial;
	AaRenderSystem* mRenderSystem;
	AaGuiSystem* mGuiMgr;
	AaCamera* currentCamera;

};

#endif