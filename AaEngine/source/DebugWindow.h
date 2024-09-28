#pragma once

class AaRenderSystem;
struct ID3D12GraphicsCommandList;
struct ID3D12DescriptorHeap;

namespace imgui
{
	class DebugWindow
	{
	public:

		void init(AaRenderSystem* renderer, bool* reloadShaders);
		void deinit();

		void draw(ID3D12GraphicsCommandList* list);
	};
}
