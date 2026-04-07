#pragma once

#include "Node.h"

class TextureSampleNode : public Node
{
public:
	uint32_t textureId = 0;

	TextureSampleNode()
	{
		displayName = "Texture";
		setTitleColor(50, 150, 50);

		inputs.push_back({ "UV", ValueType::Float2, this });
		outputs.push_back({ "Height", ValueType::Float, this });
	}

	std::string Emit(ShaderGenContext& ctx) override
	{
		std::string uv = EmitInput(inputs[0], ctx);

		std::string var = ctx.NewTemp(ValueType::Float);
		ctx.code +=
			"Texture2D tex" + std::to_string(textureId) +
			" = ResourceDescriptorHeap[TexIdNoise];\n";
		ctx.code +=
			"float " + var +
			" = tex" + std::to_string(textureId) +
			".SampleLevel(LinearWrapSampler, " + uv + ", 0).r;\n";

		return var;
	}
};
