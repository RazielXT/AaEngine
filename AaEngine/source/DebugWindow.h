#pragma once

#include "AaMath.h"

class AaRenderSystem;
struct ID3D12GraphicsCommandList;
struct ID3D12DescriptorHeap;

namespace imgui
{
	struct DebugState
	{
		bool reloadShaders = false;
		int TexturePreviewIndex = -1;

		const char* changeScene{};

		bool stopUpdatingVoxel = false;
		float voxelSteppingBounces = 0.075f;
		float voxelSteppingDiffuse = 0.0f;
		Vector2 middleConeRatioDistance = { 1.05f, 1.5f };
		Vector2 sideConeRatioDistance = { 2.2f, 5.f };
	};

	struct DebugWindow
	{
		static DebugWindow& Get();

		void init(AaRenderSystem* renderer);
		void deinit();

		void draw(ID3D12GraphicsCommandList* list);

		static DebugState state;
	};
}
