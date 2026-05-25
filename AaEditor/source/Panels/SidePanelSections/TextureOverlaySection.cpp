#include "TextureOverlaySection.h"

#include "FrameCompositor/Tasks/DebugOverlayTask.h"
#include "Utils/DxUtils.h"
#include <algorithm>
#include <format>
#include <string>
#include <vector>

void TextureOverlaySection::draw()
{
	auto& overlayTask = DebugOverlayTask::Get();
	static bool enabledTexture = false;
	if (ImGui::Checkbox("Enable texture overlay", &enabledTexture))
		overlayTask.enable(enabledTexture);

	if (!enabledTexture)
		return;

	UINT next = overlayTask.currentIdx();
	const uint32_t step = 1;
	if (ImGui::InputScalar("Texture idx", ImGuiDataType_U32, &next, &step))
		overlayTask.changeIdx(next);

	{
		bool fullscreen = overlayTask.isFullscreen();
		if (ImGui::Checkbox("Texture preview fullscreen", &fullscreen))
			overlayTask.setFullscreen(fullscreen);

		if (auto info = overlayTask.getCurrentDescriptor())
		{
			ImGui::Text("Texture: %s", info->name);

			auto desc = info->resource->GetDesc();
			ImGui::Text("Format: %s", DxgiFormatToString(desc.Format));

			if (info->dimension == D3D12_SRV_DIMENSION_TEXTURE3D)
			{
				ImGui::Text("Size: %u x %u x %u", desc.Width, desc.Height, desc.DepthOrArraySize);

				int slice = (int)overlayTask.getSliceIdx();
				if (ImGui::SliderInt("Slice", &slice, 0, (int)desc.DepthOrArraySize - 1))
					overlayTask.setSliceIdx((UINT)slice);
			}
			else
			{
				ImGui::Text("Size: %u x %u", desc.Width, desc.Height);
			}
		}
	}

	{
		bool remap = overlayTask.isRemapEnabled();
		if (ImGui::Checkbox("Remap color range", &remap))
			overlayTask.setRemapEnabled(remap);

		if (remap)
		{
			auto range = overlayTask.getRemapMinMax();

			float max = 256.0f;
			float epsilon = range.y > 1.0f ? 0.01f : 0.0001f;

			if (ImGui::DragFloat2("Range min max", &range.x, epsilon * 10.f, 0.0f, max, "%.4f"))
			{
				auto last = overlayTask.getRemapMinMax();

				if (range.x != last.x && range.x > range.y - epsilon)
					range.y = std::clamp(range.x + epsilon, 0.0f, max);

				if (range.y != last.y && range.y < range.x + epsilon)
					range.x = std::clamp(range.y - epsilon, 0.0f, max);

				overlayTask.setRemapMinMax(range);
			}
		}
	}

	auto textures = overlayTask.getTextureList();
	textureFilter.Draw("Texture filter");

	std::vector<UINT> filteredTextureIndices;
	filteredTextureIndices.reserve(textures.size());
	for (UINT i = 0; i < textures.size(); i++)
	{
		const char* dimLabel = textures[i].dimension == D3D12_SRV_DIMENSION_TEXTURE3D ? " (3D)" : "";
		std::string label = std::format("{}{} [{}]", textures[i].name, dimLabel, textures[i].index);

		if (textureFilter.PassFilter(label.c_str()))
			filteredTextureIndices.push_back(i);
	}

	UINT currentIdx = overlayTask.currentIdx();
	int selectedListIdx = -1;
	for (UINT i = 0; i < filteredTextureIndices.size(); i++)
	{
		if (textures[filteredTextureIndices[i]].index == currentIdx)
		{
			selectedListIdx = (int)i;
			break;
		}
	}

	if (selectedListIdx == -1 && !filteredTextureIndices.empty())
		selectedListIdx = 0;

	const int visibleItems = min((int)(filteredTextureIndices.empty() ? 1 : filteredTextureIndices.size()), 10);
	if (ImGui::BeginListBox("Textures", ImVec2(-FLT_MIN, ImGui::GetTextLineHeightWithSpacing() * visibleItems)))
	{
		bool refocus = false;
		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && !filteredTextureIndices.empty())
		{
			if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && selectedListIdx > 0)
				selectedListIdx--;

			if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && selectedListIdx < (int)filteredTextureIndices.size() - 1)
				selectedListIdx++;

			if (selectedListIdx >= 0 && textures[filteredTextureIndices[selectedListIdx]].index != currentIdx)
			{
				overlayTask.changeIdx(textures[filteredTextureIndices[selectedListIdx]].index);
				refocus = true;
			}
		}

		for (UINT i = 0; i < filteredTextureIndices.size(); i++)
		{
			auto textureIdx = filteredTextureIndices[i];
			const char* dimLabel = textures[textureIdx].dimension == D3D12_SRV_DIMENSION_TEXTURE3D ? " (3D)" : "";
			std::string label = std::format("{}{} [{}]", textures[textureIdx].name, dimLabel, textures[textureIdx].index);

			bool isSelected = ((int)i == selectedListIdx);
			if (ImGui::Selectable(label.c_str(), isSelected))
				overlayTask.setIdx(textures[textureIdx].index);

			if (isSelected)
			{
				ImGui::SetItemDefaultFocus();
				if (refocus)
					ImGui::SetScrollHereY(0.5f);
			}
		}

		if (filteredTextureIndices.empty())
			ImGui::TextDisabled("No textures match filter");

		ImGui::EndListBox();
	}
}
