#pragma once

#include "MathUtils.h"

class RenderSystem;
class MaterialResources;
struct ID3D12GraphicsCommandList;

namespace imgui
{
	struct DebugState
	{
		bool reloadShaders = false;

		const char* changeScene{};

		bool stopUpdatingVoxel = false;
		float voxelSteppingBounces = 0.075f;
		float voxelSteppingDiffuse = 0.0f;
		Vector2 middleConeRatioDistance = { 1.05f, 1.5f };
		Vector2 sideConeRatioDistance = { 2.2f, 5.f };

		struct 
		{
			float RejectionFalloff = 1.0f;// 1.0f - 10.0f
			float Accentuation = 0.0f; //0.0f - 1.0f
			float BlurTolerance = -5.0f; // -8.0f, -1.0f
			float UpsampleTolerance = -7.0f; //-12.0f, -1.0f
			float NoiseFilterTolerance = -3.0f; // -8.0f, 0.0f
		}
		ssao;

		int DlssMode{};
		int FsrMode{};
	};

	struct DebugWindow
	{
		static DebugWindow& Get();

		DebugWindow(RenderSystem& renderer);
		~DebugWindow();

		void draw(ID3D12GraphicsCommandList* list, MaterialResources& materials);

		static DebugState state;
	};
}
