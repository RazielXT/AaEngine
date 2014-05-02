#include "AaSceneManager.h"

AaSceneManager::AaSceneManager(AaRenderSystem* rs, AaGuiSystem* guiMgr)
{
	mRenderSystem=rs;
	mGuiMgr=guiMgr;

	mMaterialLoader= new AaMaterialLoader(rs);
	mModelLoader= new AaModelLoader(rs);
	currentCamera= createCamera("main");
	defaultMaterial= new AaMaterial("Default",rs);
	mShadingMgr = new AaShadingManager(mRenderSystem);
}


AaSceneManager::~AaSceneManager()
{
	std::unordered_map<std::string,AaEntity*>::iterator it;
	
	for(it=mEntityMap.begin();it!=mEntityMap.end();it++)
		delete it->second;

	mEntityMap.clear();

	std::map<std::string,AaModelInfo*>::iterator it0;

	for(it0=mModelMap.begin();it0!=mModelMap.end();it0++)
		delete it0->second;

	mModelMap.clear();

	std::map<std::string,AaMaterial*>::iterator it2;
	
	for(it2=mMaterialMap.begin();it2!=mMaterialMap.end();it2++)
		delete it2->second;

	mMaterialMap.clear();

	delete mMaterialLoader;
	delete currentCamera;
}

AaEntity* AaSceneManager::createEntity(std::string name, std::string materialName, UCHAR renderqueue)
{
	std::map<std::string,AaMaterial*>::iterator itM = mMaterialMap.find(materialName);
	
	if(itM!=mMaterialMap.end())
	{
		return createEntity(name,itM->second,renderqueue);
	}
	else
	{
		AaLogger::getLogger()->writeMessage("WARNING material name "+materialName+" not found!");
		return createEntity(name,defaultMaterial, renderqueue);
	}
}

AaEntity* AaSceneManager::createEntity(std::string name)
{
	return createEntity(name,defaultMaterial);
}

AaEntity* AaSceneManager::createEntity(std::string name, AaMaterial* material, UCHAR renderqueue)
{
	std::unordered_map<std::string,AaEntity*>::iterator it = mEntityMap.find(name);

	if(it==mEntityMap.end())
	{
		AaEntity* ent=new AaEntity(name, this, &currentCamera);
		ent->setMaterial(material);
		ent->renderQueueOrder = renderqueue;

		mEntityMap[name]=ent;
		
		//std::map<char,std::map<RenderState*,std::map<AaMaterial*,std::vector<AaEntity*>>>>
		std::map<RenderState*,std::map<AaMaterial*,std::vector<AaEntity*>>>* RSmap;

		if(mRenderQueue.count(ent->renderQueueOrder))
		{
			RSmap=&mRenderQueue[ent->renderQueueOrder];

			
			if(RSmap->count(material->mRenderState))
			{
				std::map<AaMaterial*,std::vector<AaEntity*>>* matMap=&RSmap->find(material->mRenderState)->second;
				
				if(matMap->count(material))
				{
					matMap->find(material)->second.push_back(ent);
				}
				else
				{
					//new material to render
					std::vector<AaEntity*> entVector;
					entVector.push_back(ent);
					matMap->insert(std::pair<AaMaterial*,std::vector<AaEntity*>>(material,entVector));
				}
			}	
			else
			{
				//new render state in queue
				std::vector<AaEntity*> entVector;
				entVector.push_back(ent);
				std::map<AaMaterial*,std::vector<AaEntity*>> matMap;
				matMap[material]=entVector;
				RSmap->insert(std::pair<RenderState*,std::map<AaMaterial*,std::vector<AaEntity*>>>(material->mRenderState,matMap));
			}
		}
		else
		{
			//new render queue
			std::map<RenderState*,std::map<AaMaterial*,std::vector<AaEntity*>>> newMap;
			mRenderQueue[ent->renderQueueOrder]=newMap;
			RSmap=&mRenderQueue[ent->renderQueueOrder];
			std::vector<AaEntity*> entVector;
			entVector.push_back(ent);
			std::map<AaMaterial*,std::vector<AaEntity*>> matMap;
			matMap[material]=entVector;
			RSmap->insert(std::pair<RenderState*,std::map<AaMaterial*,std::vector<AaEntity*>>>(material->mRenderState,matMap));
		}


		return ent;

	}
	else
	{
		AaLogger::getLogger()->writeMessage("WARNING attempted creation of entity with existing name "+name);
		return it->second;
	}
}

AaEntity* AaSceneManager::getEntity(std::string name)
{
	std::unordered_map<std::string,AaEntity*>::iterator it = mEntityMap.find(name);

	if(it==mEntityMap.end())
		return NULL;
	else
		return it->second;
}

float gScale = 1;

void AaSceneManager::generalScale(float scale)
{
	std::unordered_map<std::string,AaEntity*>::iterator it = mEntityMap.begin();

	while(it!=mEntityMap.end())
	{
		AaEntity* e = it->second;
		XMFLOAT3 pos = e->getPosition();
		XMFLOAT3 sc = e->getScale();

		pos.x = scale*pos.x/gScale;
		pos.y = scale*pos.y/gScale;
		pos.z = scale*pos.z/gScale;

		sc.x = scale*sc.x/gScale;
		sc.y = scale*sc.y/gScale;
		sc.z = scale*sc.z/gScale;

		e->setPosition(pos);
		e->setScale(sc);
		
		it++;
	}

	gScale = scale;
}

AaMaterial* AaSceneManager::createMaterial(std::string name)
{
	std::map<std::string,AaMaterial*>::iterator it = mMaterialMap.find(name);
	
	if(it==mMaterialMap.end())
	{
		AaMaterial* mat=new AaMaterial(name,mRenderSystem);
		mMaterialMap[name]=mat;
		return mat;
	}
	else
	{
		AaLogger::getLogger()->writeMessage("WARNING attempted creation of material with existing name "+name);
		return it->second;
	}

}

void AaSceneManager::setCurrentCamera(AaCamera* cam)
{
	currentCamera = cam;
}

AaMaterial* AaSceneManager::getMaterial(std::string name)
{
	std::map<std::string,AaMaterial*>::iterator it = mMaterialMap.find(name);
	
	if(it==mMaterialMap.end())
		return defaultMaterial;
	else
		return it->second;		
}

void AaSceneManager::renderSceneWithMaterial(AaMaterial* usedMaterial, bool preserveTextures, UCHAR minQueue, UCHAR maxQueue)
{
	ID3D11DeviceContext* d3dContext=mRenderSystem->getContext();

	RenderState* mRenderState = usedMaterial->mRenderState;

	d3dContext->RSSetState(mRenderState->rasterizerState);
	float blendFactor[4]={0,0,0,0};
	d3dContext->OMSetBlendState(mRenderState->alphaBlendState_, blendFactor, 0xFFFFFFFF );
	d3dContext->OMSetDepthStencilState(mRenderState->dsState,1);
	usedMaterial->prepareForRendering();

	std::map<char,std::map<RenderState*,std::map<AaMaterial*,std::vector<AaEntity*>>>>::iterator queueIterator = mRenderQueue.begin();

	//for all queues
	for(;queueIterator != mRenderQueue.end();queueIterator++)
	{		
		if(queueIterator->first>maxQueue || queueIterator->first<minQueue) continue;

		std::map<RenderState*,std::map<AaMaterial*,std::vector<AaEntity*>>>::iterator rsIterator = queueIterator->second.begin();
		std::map<RenderState*,std::map<AaMaterial*,std::vector<AaEntity*>>>::iterator rsIteratorEnd = queueIterator->second.end();

		//sorted by render states
		for(;rsIterator != rsIteratorEnd; rsIterator++)
		{
			std::map<AaMaterial*,std::vector<AaEntity*>>::iterator materialsIterator = rsIterator->second.begin();
			std::map<AaMaterial*,std::vector<AaEntity*>>::iterator materialsIteratorEnd = rsIterator->second.end();

			//for all materials
			for(;materialsIterator !=materialsIteratorEnd; materialsIterator++)
			{
				
				std::vector<AaEntity*>::iterator entityIterator = materialsIterator->second.begin();

				LoadedShader* sh = materialsIterator->first->shaders[4];
				if(preserveTextures && sh)
				{
					usedMaterial->setPSTextures(sh->shaderMaps,sh->samplerStates,1);//min(sh->numTextures,usedMaterial->shaders[4]->numTextures));
				}

				//for all objects
				for(;entityIterator != materialsIterator->second.end();entityIterator++)
				{
					if(!(*entityIterator)->visible) continue;

					AaModelInfo* model=(*entityIterator)->model;
					usedMaterial->updateObjectConstants(*entityIterator);

					unsigned int strides[2] = {sizeof( VertexPos ),sizeof( VertexPos2 )};
					unsigned int offsets[2] = { 0, 0 };

					d3dContext->IASetVertexBuffers( 0, usedMaterial->usedVertexBuffersCount, model->vertexBuffers_, strides, offsets );

					if(model->usesIndexBuffer)
						d3dContext->IASetIndexBuffer( model->indexBuffer_ , DXGI_FORMAT_R16_UINT, 0 );

					d3dContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

					if(model->usesIndexBuffer)
						d3dContext->DrawIndexed( model->vertexCount, 0, 0 );
					else
						d3dContext->Draw( model->vertexCount, 0 );
				}

				/*if(preserveTextures && sh)
				{
					usedMaterial->clearPSTextures(sh->numTextures);
				}*/
			}
		}
	}


	usedMaterial->clearAfterRendering();
}

void AaSceneManager::renderScene(UCHAR minQueue, UCHAR maxQueue)
{
	ID3D11DeviceContext* d3dContext=mRenderSystem->getContext();

	std::map<char,std::map<RenderState*,std::map<AaMaterial*,std::vector<AaEntity*>>>>::iterator queueIterator = mRenderQueue.begin();
	
	//for all queues
	for(;queueIterator != mRenderQueue.end();queueIterator++)
	{
			if(queueIterator->first>maxQueue || queueIterator->first<minQueue) continue;

			//sorted by render states
			std::map<RenderState*,std::map<AaMaterial*,std::vector<AaEntity*>>>::iterator rsIterator = queueIterator->second.begin();
			std::map<RenderState*,std::map<AaMaterial*,std::vector<AaEntity*>>>::iterator rsIteratorEnd = queueIterator->second.end();
			RenderState* currentRenderState=NULL;

			for(;rsIterator != rsIteratorEnd; rsIterator++)
			{
				if(currentRenderState!=rsIterator->first)
					currentRenderState=rsIterator->first;

				d3dContext->RSSetState(currentRenderState->rasterizerState);
				float blendFactor[4]={0,0,0,0};
				d3dContext->OMSetBlendState( currentRenderState->alphaBlendState_, blendFactor, 0xFFFFFFFF );
				d3dContext->OMSetDepthStencilState(currentRenderState->dsState,1);

				std::map<AaMaterial*,std::vector<AaEntity*>>::iterator materialsIterator = rsIterator->second.begin();
				std::map<AaMaterial*,std::vector<AaEntity*>>::iterator materialsIteratorEnd = rsIterator->second.end();

				//for all materials
				for(;materialsIterator !=materialsIteratorEnd; materialsIterator++)
				{
					//update material
					AaMaterial* mat=materialsIterator->first;
					mat->prepareForRendering();

					std::vector<AaEntity*>::iterator entityIterator = materialsIterator->second.begin();

					//for all objects
					for(;entityIterator != materialsIterator->second.end();entityIterator++)
					{
						if(!(*entityIterator)->visible) continue;

						AaModelInfo* model=(*entityIterator)->model;
						mat->updateObjectConstants(*entityIterator);

						unsigned int strides[2] = {sizeof( VertexPos ),sizeof( VertexPos2 )};
						unsigned int offsets[2] = { 0, 0 };

						d3dContext->IASetVertexBuffers( 0, mat->usedVertexBuffersCount, model->vertexBuffers_, strides, offsets );

						if(model->usesIndexBuffer)
						d3dContext->IASetIndexBuffer( model->indexBuffer_ , DXGI_FORMAT_R16_UINT, 0 );

						d3dContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

						if(model->usesIndexBuffer)
						d3dContext->DrawIndexed( model->vertexCount, 0, 0 );
						else
						d3dContext->Draw( model->vertexCount, 0 );
					}

					mat->clearAfterRendering();
				}
			}
	}

	d3dContext->RSSetState(mRenderSystem->getDefaultRenderState()->rasterizerState);
	float blendFactor[4]={0,0,0,0};
	d3dContext->OMSetBlendState( mRenderSystem->getDefaultRenderState()->alphaBlendState_, blendFactor, 0xFFFFFFFF );
	d3dContext->OMSetDepthStencilState(mRenderSystem->getDefaultRenderState()->dsState,1);
}

AaModelInfo* AaSceneManager::getModel(std::string name)
{
	std::map<std::string,AaModelInfo*>::iterator it = mModelMap.find(name);

	if(it==mModelMap.end())
	{
		AaLogger::getLogger()->writeMessage("Loading mesh file " +name);

		AaModelInfo* mi = mModelLoader->loadModel(name);
		if(!mi)
			return NULL;
		mModelMap[name] = mi;
		return mi;
	}
	else
		return it->second;
}

AaCamera* AaSceneManager::createCamera(std::string name)
{
	std::map<std::string,AaCamera*>::iterator it = mCameraMap.find(name);
	
	if(it==mCameraMap.end())
	{
		AaCamera* cam=new AaCamera();
		mCameraMap[name]=cam;
		return cam;
	}
	else
	{
		AaLogger::getLogger()->writeMessage("WARNING attempted creation of camera with existing name "+name);
		return it->second;
	}
}

void AaSceneManager::loadMaterialFiles(std::string directory,bool subDirectories)
{
	mMaterialLoader->loadShaderReferences(directory,subDirectories);
	std::map<std::string,AaMaterial*> mats=mMaterialLoader->loadMaterials(directory,subDirectories);
	mMaterialMap.insert(mats.begin(),mats.end());
}

AaCamera* AaSceneManager::getCamera(std::string name)
{
	std::map<std::string,AaCamera*>::iterator it = mCameraMap.find(name);
	
	if(it==mCameraMap.end())
		return NULL;
	else
		return it->second;
}

AaCamera* AaSceneManager::getCamera()
{
	return currentCamera;
}
