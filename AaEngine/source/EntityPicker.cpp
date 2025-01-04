#include "EntityPicker.h"
#include "RenderObject.h"
#include "SceneManager.h"

EntityPicker* instance = nullptr;

EntityPicker::EntityPicker()
{
	instance = this;
}

EntityPicker::~EntityPicker()
{
	DescriptorManager::get().removeTextureView(rtt);

	clear();

	instance = nullptr;
}

EntityPicker& EntityPicker::Get()
{
	return *instance;
}

void EntityPicker::initializeGpuResources(RenderSystem& renderSystem, GraphicsResources& resources)
{
	auto device = renderSystem.core.device;

	heap.InitRtv(device, formats.size(), L"EntityPickerRtv");
	heap.InitDsv(device, 1, L"EntityPickerDsv");

	rtt = {};
	auto size = renderSystem.getOutputSize();
	rtt.Init(device, size.x, size.y, heap, formats, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	rtt.SetName("EntityPickerRttTex");

	//DescriptorManager::get().createTextureView(rtt);

	if (!readbackBuffer)
	{
		D3D12_RESOURCE_DESC bufferDesc = {};
		bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		bufferDesc.Width = sizeof(UINT); // Size of one pixel
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

		device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&readbackBuffer));
	}
}

void EntityPicker::clear()
{
}

void EntityPicker::renderEntityIds(ID3D12GraphicsCommandList* commandList, RenderQueue& queue, Camera& camera, const RenderObjectsVisibilityData& visibility, const FrameParameters& frame)
{
	//rtt.PrepareAsTarget(commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
}

void EntityPicker::scheduleEntityIdRead(ID3D12GraphicsCommandList* commandList)
{
	auto idTexture = rtt.textures[0].texture.Get();

	D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
	srcLocation.pResource = idTexture;
	srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	srcLocation.SubresourceIndex = 0;

	D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
	dstLocation.pResource = readbackBuffer.Get();
	dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
	footprint.Offset = 0;
	footprint.Footprint.Format = DXGI_FORMAT_R32_UINT;
	footprint.Footprint.Width = 1;   // Single pixel width
	footprint.Footprint.Height = 1;  // Single pixel height
	footprint.Footprint.Depth = 1;
	footprint.Footprint.RowPitch = sizeof(uint32_t);
	dstLocation.PlacedFootprint = footprint;

	UINT x = scheduled->x;
	UINT y = scheduled->y;
	scheduled.reset();
	CD3DX12_BOX srcBox(x, y, 0, x + 1, y + 1, 1); // Box to specify the region

	// Transition the texture to the COPY_SOURCE state
	auto tr = CD3DX12_RESOURCE_BARRIER::Transition(idTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
	commandList->ResourceBarrier(1, &tr);

	// Copy the specific pixel region
	commandList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, &srcBox);

	// Transition the texture back to its original state
	tr = CD3DX12_RESOURCE_BARRIER::Transition(idTexture, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
	commandList->ResourceBarrier(1, &tr);

	lastPick = {};
}

ObjectId EntityPicker::readEntityId()
{
	uint32_t* mappedData;
	D3D12_RANGE readRange = { 0, sizeof(uint32_t) };
	readbackBuffer->Map(0, &readRange, reinterpret_cast<void**>(&mappedData));

	// Access the specific pixel data
	uint32_t pixelValue = mappedData[0];

	// Unmap the buffer when done
	D3D12_RANGE writeRange = { 0, 0 }; // No need to write back any data
	readbackBuffer->Unmap(0, &writeRange);

	lastPick = ObjectId{ pixelValue };

	return lastPick;
}

bool EntityPicker::lastPickWasTransparent() const
{
	return lastPick.getObjectType() == ObjectType::Entity && lastPick.getOrder() == Order::Transparent;
}

RenderQueue EntityPicker::createRenderQueue() const
{
	RenderQueue idQueue;
	idQueue.technique = MaterialTechnique::EntityId;
	idQueue.targets = formats;

	return idQueue;
}
