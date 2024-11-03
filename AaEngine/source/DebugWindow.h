#pragma once

class AaRenderSystem;
struct ID3D12GraphicsCommandList;
struct ID3D12DescriptorHeap;

namespace imgui
{
	struct DebugState
	{
		bool reloadShaders = false;
		const char* changeScene{};
		int TexturePreviewIndex = -1;
	};

	struct DebugWindow
	{
		static DebugWindow& Get();

		void init(AaRenderSystem* renderer);
		void deinit();

		void draw(ID3D12GraphicsCommandList* list);

		DebugState state;
	};
}
