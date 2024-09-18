#include "AaSceneManager.h"
#include "AaLogger.h"
#include "Directories.h"
#include "AaMaterial.h"
#include "AaShaderManager.h"

AaSceneManager::AaSceneManager(AaRenderSystem* rs)
{
	mRenderSystem = rs;
}

AaSceneManager::~AaSceneManager()
{
	for (auto & it : mEntityMap)
		delete it.second;

	for (auto& it : layoutMap)
		if (it.second)
			it.second->Release();
}

AaEntity* AaSceneManager::createEntity(std::string name, std::string materialName, UCHAR renderqueue)
{
	return createEntity(name, AaMaterialResources::get().getMaterial(materialName), renderqueue);
}

AaEntity* AaSceneManager::createEntity(std::string name, AaMaterial* material, UCHAR renderqueue)
{
	auto it = mEntityMap.find(name);

	if (it == mEntityMap.end())
	{
		auto ent = new AaEntity(name, this);
		ent->setMaterial(material);
		ent->renderQueueOrder = renderqueue;

		mEntityMap[name] = ent;
		mRenderQueue[renderqueue][material->mRenderState][material].push_back(ent);

		return ent;
	}

	AaLogger::logWarning("entity duplicate " + name);
	return it->second;
}

AaEntity* AaSceneManager::getEntity(std::string name)
{
	auto it = mEntityMap.find(name);

	if (it == mEntityMap.end())
		return nullptr;

	return it->second;
}

ID3D11InputLayout* AaSceneManager::getInputLayout(AaModel* model, AaMaterial* material)
{
	uint64_t hashId = model->getLayoutId();
	hashId += (size_t)material;

	auto it = layoutMap.find(hashId);
	if (it != layoutMap.end())
		return it->second;

	ID3D11InputLayout* layout = nullptr;
	auto shader = material->shaders[ShaderTypeVertex];
	HRESULT result = mRenderSystem->getDevice()->CreateInputLayout(model->vertexLayout.data(), model->vertexLayout.size(), shader->shader->compiled->vsBuffer->GetBufferPointer(), shader->shader->compiled->vsBuffer->GetBufferSize(), &layout);

	layoutMap[hashId] = layout;

	if (FAILED(result))
	{
		AaLogger::logErrorD3D("failed to create input layout for material " + material->getName(), result);
		return nullptr;
	}

	return layout;
}

void AaSceneManager::renderSceneWithMaterial(AaCamera& camera, AaMaterial* usedMaterial, bool preserveTextures, UCHAR minQueue, UCHAR maxQueue)
{
	auto d3dContext = mRenderSystem->getContext();
	auto mRenderState = usedMaterial->mRenderState;

	d3dContext->RSSetState(mRenderState->rasterizerState);
	float blendFactor[4] = { 0,0,0,0 };
	d3dContext->OMSetBlendState(mRenderState->alphaBlendState_, blendFactor, 0xFFFFFFFF);
	d3dContext->OMSetDepthStencilState(mRenderState->dsState, 1);
	usedMaterial->prepareForRendering();

	auto queueIterator = mRenderQueue.begin();

	//for all queues
	for (auto& queue : mRenderQueue)
	{
		if (queue.first > maxQueue || queue.first < minQueue) continue;

		//sorted by render states
		for (auto& rs : queue.second)
		{
			//for all materials
			for (auto& material : rs.second)
			{
				MaterialShader* sh = material.first->shaders[ShaderTypePixel];
				if (preserveTextures && sh)
				{
					usedMaterial->setPSTextures(sh->shaderMaps, sh->samplerStates, 1);//min(sh->numTextures,usedMaterial->shaders[4]->numTextures));
				}

				//for all objects
				for (auto entity : material.second)
				{
					if (!entity->visible || !entity->model) continue;

					usedMaterial->updateObjectConstants(entity, camera);

					auto model = entity->model;

					d3dContext->IASetInputLayout(getInputLayout(model, usedMaterial));

					unsigned int strides[2] = { model->getLayoutVertexSize(0), 0 };
					unsigned int offsets[2] = { 0, 0 };

					d3dContext->IASetVertexBuffers(0, 1, &model->vertexBuffer_, strides, offsets);

					if (model->indexBuffer_)
						d3dContext->IASetIndexBuffer(model->indexBuffer_, DXGI_FORMAT_R16_UINT, 0);

					d3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

					if (model->indexBuffer_)
						d3dContext->DrawIndexed(model->indexCount, 0, 0);
					else
						d3dContext->Draw(model->vertexCount, 0);
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

void AaSceneManager::renderScene(AaCamera& camera, UCHAR minQueue, UCHAR maxQueue)
{
	auto d3dContext = mRenderSystem->getContext();
	auto frustum = camera.prepareFrustum();

	//for all queues
	for (const auto& queue : mRenderQueue)
	{
		if (queue.first > maxQueue || queue.first < minQueue) continue;

		//sorted by render states
		for (auto& rsIt : queue.second)
		{
			RenderState* currentRenderState = rsIt.first;
			d3dContext->RSSetState(currentRenderState->rasterizerState);
			float blendFactor[4] = { 0,0,0,0 };
			d3dContext->OMSetBlendState(currentRenderState->alphaBlendState_, blendFactor, 0xFFFFFFFF);
			d3dContext->OMSetDepthStencilState(currentRenderState->dsState, 1);

			//for all materials
			for (auto& materialIt : rsIt.second)
			{
				AaMaterial* mat = materialIt.first;
				mat->prepareForRendering();

				//for all entities
				for (auto entity : materialIt.second)
				{
					if (!frustum.Intersects(entity->getWorldBoundingBox()))
						continue;

					if (!entity->visible || !entity->model) continue;
	
					mat->updateObjectConstants(entity, camera);

					auto model = entity->model;

					d3dContext->IASetInputLayout(getInputLayout(model, mat));

					unsigned int strides[2] = { model->getLayoutVertexSize(0), 0 };
					unsigned int offsets[2] = { 0, 0 };

					d3dContext->IASetVertexBuffers(0, 1, &model->vertexBuffer_, strides, offsets);

					if (model->indexBuffer_)
						d3dContext->IASetIndexBuffer(model->indexBuffer_, DXGI_FORMAT_R16_UINT, 0);

					d3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

					if (model->indexBuffer_)
						d3dContext->DrawIndexed(model->indexCount, 0, 0);
					else
						d3dContext->Draw(model->vertexCount, 0);
				}

				mat->clearAfterRendering();
			}
		}
	}

	d3dContext->RSSetState(mRenderSystem->getDefaultRenderState()->rasterizerState);
	float blendFactor[4] = { 0,0,0,0 };
	d3dContext->OMSetBlendState(mRenderSystem->getDefaultRenderState()->alphaBlendState_, blendFactor, 0xFFFFFFFF);
	d3dContext->OMSetDepthStencilState(mRenderSystem->getDefaultRenderState()->dsState, 1);
}
