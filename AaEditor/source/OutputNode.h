#pragma once

#include "Node.h"

class FloatOutputNode : public Node
{
public:

	std::function<void(std::string)> onBuild;

	FloatOutputNode(std::function<void(std::string)> cb) : onBuild(cb)
	{
		displayName = "Float Output";
		setTitleColor(0, 140, 140);

		inputs.push_back({ "Output", ValueType::Float });
	}

	std::string Emit(ShaderGenContext& ctx) override
	{
		return EmitInput(inputs[0], ctx);
	}

	void Draw() override
	{
		BeginDraw();
		DrawTitle();
		ImGui::SetNextItemWidth(100);
		if (ImGui::Button(ICON_FA_ARROWS_ROTATE))
		{
			ShaderGenContext ctx;
			std::string heightVar = Emit(ctx);

			std::string generated =
				"float heightFunc(float2 uv)\n{\n" +
				ctx.code +
				"return " + heightVar + ";\n}\n";

			onBuild(generated);
		}
		DrawPins();
		EndDraw();
	}
};
