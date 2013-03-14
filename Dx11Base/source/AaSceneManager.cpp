#include "AaSceneManager.h"

AaSceneManager::AaSceneManager(AaRenderSystem* rs, AaGuiSystem* guiMgr)
{
	mRenderSystem=rs;
	mGuiMgr=guiMgr;
	mMaterialLoader= new AaMaterialLoader(rs);
	mModelLoader= new AaModelLoader(rs);
	currentCamera= new AaCamera();
	defaultMaterial= new AaMaterial("Default",rs);
	mShadingMgr = new AaShadingManager(mRenderSystem);
}


AaSceneManager::~AaSceneManager()
{
	std::unordered_map<std::string,AaEntity*>::iterator it;
	
	for(it=mEntityMap.begin();it!=mEntityMap.end();it++)
		delete it->second;

	mEntityMap.clear();


	std::map<std::string,AaMaterial*>::iterator it2;
	
	for(it2=mMaterialMap.begin();it2!=mMaterialMap.end();it2++)
		delete it2->second;

	mMaterialMap.clear();

	delete mMaterialLoader;
	delete currentCamera;
}

AaEntity* AaSceneManager::createEntity(std::string name, std::string materialName)
{
	std::map<std::string,AaMaterial*>::iterator itM = mMaterialMap.find(materialName);
	
	if(itM!=mMaterialMap.end())
	{
		return createEntity(name,itM->second);
	}
	else
	{
		AaLogger::getLogger()->writeMessage("WARNING material name "+materialName+" not found!");
		return createEntity(name,defaultMaterial);
	}
}

AaEntity* AaSceneManager::createEntity(std::string name)
{
	return createEntity(name,defaultMaterial);
}

AaEntity* AaSceneManager::createEntity(std::string name, AaMaterial* material)
{
	std::unordered_map<std::string,AaEntity*>::iterator it = mEntityMap.find(name);

	if(it==mEntityMap.end())
	{
		AaEntity* ent=new AaEntity(name, this, &currentCamera);
		ent->setMaterial(material);

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

AaMaterial* AaSceneManager::getMaterial(std::string name)
{
	std::map<std::string,AaMaterial*>::iterator it = mMaterialMap.find(name);
	
	if(it==mMaterialMap.end())
		return defaultMaterial;
	else
		return it->second;		
}


void AaSceneManager::renderScene()
{
	ID3D11DeviceContext* d3dContext=mRenderSystem->getContext();

	std::map<char,std::map<RenderState*,std::map<AaMaterial*,std::vector<AaEntity*>>>>::iterator queueIterator = mRenderQueue.begin();
	
	//for all queues
	for(;queueIterator != mRenderQueue.end();queueIterator++)
	{
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
					mat->updatePerMaterialConstants();

					std::vector<AaEntity*>::iterator entityIterator = materialsIterator->second.begin();
					
					//for all objects
					for(;entityIterator != materialsIterator->second.end();entityIterator++)
					{
						if(!(*entityIterator)->visible) continue;

						AaModelInfo* model=(*entityIterator)->model;
						mat->updatePerObjectConstants(*entityIterator);

						unsigned int stride = sizeof( VertexPos );
						unsigned int offset = 0;
						d3dContext->IASetInputLayout( model->inputLayout );
						d3dContext->IASetVertexBuffers( 0, 1, &model->vertexBuffer_, &stride, &offset );

						if(model->usesIndexBuffer)
						d3dContext->IASetIndexBuffer( model->indexBuffer_ , DXGI_FORMAT_R16_UINT, 0 );

						d3dContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

						mat->prepareForRendering();

						if(model->usesIndexBuffer)
						d3dContext->DrawIndexed( model->vertexCount, 0, 0 );
						else
						d3dContext->Draw( model->vertexCount, 0 );
					}
				}
			}
	}

	/*
	while(it!=mEntityMap.end())
	{
		AaEntity* ent=it->second;

		//poslanie batch
		AaModelInfo* model=ent->model;
		
		mat->updateMaterialConstants(ent);

		unsigned int stride = sizeof( VertexPos );
		unsigned int offset = 0;
		rs->getContext()->IASetInputLayout( model->inputLayout );
		rs->getContext()->IASetVertexBuffers( 0, 1, &model->vertexBuffer_, &stride, &offset );

		if(model->usesIndexBuffer)
		rs->getContext()->IASetIndexBuffer( model->indexBuffer_ , DXGI_FORMAT_R16_UINT, 0 );

		rs->getContext()->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
		rs->getContext()->VSSetShader( mat->vertexShaderProgram, 0, 0 );
		rs->getContext()->PSSetShader( mat->pixelShaderProgram, 0, 0 );
	
		shaderTextureInfo info=mat->getTexturesInfoPS();
		if(info.numTextures>0)
		{
			rs->getContext()->PSSetShaderResources( 0, info.numTextures, info.shaderMaps );
			rs->getContext()->PSSetSamplers( 0, info.numTextures, info.samplerStates );
		}

		if(model->usesIndexBuffer)
		rs->getContext()->DrawIndexed( model->vertexCount, 0, 0 );
		else
		rs->getContext()->Draw( model->vertexCount, 0 );

		rs->swapChain_->Present(0,0);
	}*/
}

AaModelInfo* AaSceneManager::getModel(std::string name)
{
	std::map<std::string,AaModelInfo*>::iterator it = mModelMap.find(name);

	if(it==mModelMap.end())
		return mModelLoader->loadModel(name);
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
