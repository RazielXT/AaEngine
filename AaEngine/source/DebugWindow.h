#pragma once

class AaRenderSystem;
struct ID3D12GraphicsCommandList;
struct ID3D12DescriptorHeap;

namespace imgui
{
	struct DebugState
	{
		bool reloadShaders = false;
		bool changeScene = false;
	};

	class DebugWindow
	{
	public:

		void init(AaRenderSystem* renderer);
		void deinit();

		void draw(ID3D12GraphicsCommandList* list);

		DebugState state;
	};
}
