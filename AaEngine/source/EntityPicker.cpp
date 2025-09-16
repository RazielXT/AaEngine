#include "EntityPicker.h"
#include "RenderObject.h"
#include "SceneManager.h"
#include <chrono>

EntityPicker* instance = nullptr;

EntityPicker::EntityPicker(RenderSystem& rs) : renderSystem(rs)
{
	instance = this;
}

EntityPicker::~EntityPicker()
{
	DescriptorManager::get().removeTextureView(rtt);

	instance = nullptr;
}

EntityPicker& EntityPicker::Get()
{
	return *instance;
}

void EntityPicker::initializeGpuResources()
{
	auto device = renderSystem.core.device;

	heap.InitRtv(device, formats.size(), L"EntityPickerRtv");
	heap.InitDsv(device, 1, L"EntityPickerDsv");

	rtt = {};
	auto size = renderSystem.getOutputSize();
	rtt.Init(device, size.x, size.y, heap, formats, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	rtt.SetName("EntityPickerRttTex");

	//DescriptorManager::get().createTextureView(rtt);

	if (!readbackBuffer[0])
	{
		D3D12_RESOURCE_DESC bufferDesc = {};
		bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		bufferDesc.Width = formatSize; // Size of one pixel
		bufferDesc.Height = 1;
		bufferDesc.DepthOrArraySize = 1;
		bufferDesc.MipLevels = 1;
		bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
		bufferDesc.SampleDesc.Count = 1;
		bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		D3D12_HEAP_PROPERTIES heapProps = {};
		heapProps.Type = D3D12_HEAP_TYPE_READBACK;
		heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProps.CreationNodeMask = 1;
		heapProps.VisibleNodeMask = 1;

		for (auto& b : readbackBuffer)
		{
			device->CreateCommittedResource(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&bufferDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(&b));
		}
	}
}

const EntityPicker::PickInfo& EntityPicker::getLastPick()
{
	if (nextPickPrepared)
	{
		readPickResult();
		nextPickPrepared = false;
	}

	return lastPick;
}

void EntityPicker::scheduleReadback(ID3D12GraphicsCommandList* commandList)
{
	D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
	//srcLocation.pResource = source;
	srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	srcLocation.SubresourceIndex = 0;

	D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
	//dstLocation.pResource = readbackBuffer;
	dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
	footprint.Offset = 0;
	//footprint.Footprint.Format = format;
	footprint.Footprint.Width = 1;   // Single pixel width
	footprint.Footprint.Height = 1;  // Single pixel height
	footprint.Footprint.Depth = 1;
	footprint.Footprint.RowPitch = formatSize;
	dstLocation.PlacedFootprint = footprint;

	UINT x = scheduled->x;
	UINT y = scheduled->y;
	CD3DX12_BOX srcBox(x, y, 0, x + 1, y + 1, 1); // Box to specify the region

	CD3DX12_RESOURCE_BARRIER barriers[_countof(readbackBuffer)];
	for (int i = 0; auto & t : rtt.textures)
		barriers[i++] = CD3DX12_RESOURCE_BARRIER::Transition(t.texture.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
	commandList->ResourceBarrier(_countof(barriers), barriers);

	for (size_t i = 0; i < rtt.textures.size(); i++)
	{
		srcLocation.pResource = rtt.textures[i].texture.Get();
		dstLocation.pResource = readbackBuffer[i].Get();
		dstLocation.PlacedFootprint.Footprint.Format = formats[i];

		// Copy the specific pixel region
		commandList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, &srcBox);
	}

	for (int i = 0; auto & t : rtt.textures)
		barriers[i++] = CD3DX12_RESOURCE_BARRIER::Transition(t.texture.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
	commandList->ResourceBarrier(_countof(barriers), barriers);
}

void EntityPicker::readPickResult()
{
	renderSystem.core.WaitForCurrentFrame();
	{
		uint32_t* mappedData;
		D3D12_RANGE readRange = { 0, sizeof(uint32_t) };
		readbackBuffer[0]->Map(0, &readRange, reinterpret_cast<void**>(&mappedData));

		// Access the specific pixel data
		lastPick.id = mappedData[0];

		// Unmap the buffer when done
		D3D12_RANGE writeRange = { 0, 0 }; // No need to write back any data
		readbackBuffer[0]->Unmap(0, &writeRange);
	}
	{
		Vector3* mappedData{};
		D3D12_RANGE readRange = { 0, sizeof(Vector3) };
		readbackBuffer[1]->Map(0, &readRange, reinterpret_cast<void**>(&mappedData));

		// Access the specific pixel data
		lastPick.position = mappedData[0];

		// Unmap the buffer when done
		D3D12_RANGE writeRange = { 0, 0 }; // No need to write back any data
		readbackBuffer[1]->Unmap(0, &writeRange);
	}
	{
		Vector3* mappedData{};
		D3D12_RANGE readRange = { 0, sizeof(Vector3) };
		readbackBuffer[2]->Map(0, &readRange, reinterpret_cast<void**>(&mappedData));

		// Access the specific pixel data
		lastPick.normal = mappedData[0];

		// Unmap the buffer when done
		D3D12_RANGE writeRange = { 0, 0 }; // No need to write back any data
		readbackBuffer[2]->Unmap(0, &writeRange);
	}
}

bool EntityPicker::lastPickWasTransparent() const
{
	return lastPick.id.getObjectType() == ObjectType::Entity && lastPick.id.getOrder() == Order::Transparent;
}

void EntityPicker::scheduleNextPick(XMUINT2 position)
{
	scheduled = position;
}

RenderQueue EntityPicker::createRenderQueue() const
{
	RenderQueue idQueue;
	idQueue.technique = MaterialTechnique::EntityId;
	idQueue.targets = formats;

	return idQueue;
}

void EntityPicker::update(ID3D12GraphicsCommandList* commandList, RenderProvider& provider, Camera& camera, SceneManager& sceneMgr)
{
	if (!scheduled)
		return;

	rtt.PrepareAsTarget(commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	//render only needed pixel
	auto rect = CD3DX12_RECT(scheduled->x, scheduled->y, scheduled->x + 1, scheduled->y + 1);
	commandList->RSSetScissorRects(1, &rect);

	CommandsMarker marker(commandList, "EntityPicker");
	RenderQueue idQueue = createRenderQueue();
	{
		auto opaqueRenderables = sceneMgr.getRenderables(Order::Normal);

		RenderObjectsVisibilityData opaqueVisibility;
		opaqueVisibility.visibility.resize(opaqueRenderables->objectsData.objects.size(), false);
		opaqueRenderables->updateVisibility(camera, opaqueVisibility);

		opaqueRenderables->iterateObjects([this, &idQueue, &opaqueVisibility, &provider](RenderObject& obj)
			{
				if (obj.isVisible(opaqueVisibility.visibility))
				{
					idQueue.update({ EntityChange::Add, Order::Normal, (SceneEntity*)&obj }, provider.resources);
				}
			});

		ShaderConstantsProvider constants(provider.params, opaqueVisibility, camera, rtt);
		idQueue.renderObjects(constants, commandList);
	}
	idQueue.reset();

	auto pickTransparent = [this]()
		{
			auto currentPickTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
			constexpr auto transparentSkipMaxDelay = std::chrono::milliseconds(500);
			static auto lastPickTime = currentPickTime - transparentSkipMaxDelay;

			bool pick = !(currentPickTime - lastPickTime < transparentSkipMaxDelay && lastPickWasTransparent());

			lastPickTime = currentPickTime;
			return pick;
		}();

	if (pickTransparent)
	{
		auto transparentRenderables = sceneMgr.getRenderables(Order::Transparent);

		RenderObjectsVisibilityData transparentVisibility;
		transparentVisibility.visibility.resize(transparentRenderables->objectsData.objects.size(), false);
		transparentRenderables->updateVisibility(camera, transparentVisibility);

		idQueue.reset();
		transparentRenderables->iterateObjects([this, &idQueue, &transparentVisibility, &provider](RenderObject& obj)
			{
				if (obj.isVisible(transparentVisibility.visibility))
				{
					idQueue.update({ EntityChange::Add, Order::Normal, (SceneEntity*)&obj }, provider.resources);
				}
			});

		ShaderConstantsProvider constants(provider.params, transparentVisibility, camera, rtt);
		idQueue.renderObjects(constants, commandList);
	}

	scheduleReadback(commandList);
	nextPickPrepared = true;
	scheduled.reset();
}
