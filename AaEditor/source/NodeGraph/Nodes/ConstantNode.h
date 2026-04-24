#pragma once

#include "NodeGraph/Node.h"

class ConstantFloatNode : public Node
{
public:
	float values[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

	ConstantFloatNode()
	{
		displayName = "Constant";
		setTitleColor(150, 50, 50);

		outputs.push_back({ "Value", ValueType::Float, this });
	}

	std::string Emit(ShaderGenContext& ctx) override
	{
		auto type = outputs[0].type;
		if (type == ValueType::UV)
			return "uv";

		std::string var = ctx.NewTemp(type);

		switch (type)
		{
		case ValueType::Float:
			ctx.code += "float " + var + " = " + std::to_string(values[0]) + ";\n";
			break;

		case ValueType::Float2:
			ctx.code += "float2 " + var + " = float2(" +
				std::to_string(values[0]) + ", " +
				std::to_string(values[1]) + ");\n";
			break;

		case ValueType::Float3:
			ctx.code += "float3 " + var + " = float3(" +
				std::to_string(values[0]) + ", " +
				std::to_string(values[1]) + ", " +
				std::to_string(values[2]) + ");\n";
			break;

		case ValueType::Float4:
			ctx.code += "float4 " + var + " = float4(" +
				std::to_string(values[0]) + ", " +
				std::to_string(values[1]) + ", " +
				std::to_string(values[2]) + ", " +
				std::to_string(values[3]) + ");\n";
			break;
		}

		return var;
	}

	void Draw() override
	{
		BeginDraw();
		DrawTitle();

		const char* types[] = { "Float", "Float2", "Float3", "Float4", "UV" };
		auto type = (int)outputs[0].type;

		ImGui::SetNextItemWidth(100);
		if (ImGui::Combo("##type", &type, types, IM_ARRAYSIZE(types)))
		{
			outputs[0].type = ValueType(type);
		}

		ImNodes::BeginOutputAttribute(id + 1);
		ImNodes::EndOutputAttribute();

		if (type < (int)ValueType::UV)
		{
			int count = 1 + (int)type; // Float=0 → 1, Float4=3 → 4
			for (int i = 0; i < count; i++)
			{
				ImGui::SetNextItemWidth(100);
				ImGui::InputFloat(("##v" + std::to_string(i)).c_str(), &values[i]);
			}
		}

		EndDraw();
	}
};

class SwizzleNode : public Node
{
public:
	char swizzle[5]{};

	SwizzleNode()
	{
		displayName = "Swizzle";
		setTitleColor(150, 50, 50);
		swizzle[0] = 'x';

		inputs.push_back({ "", ValueType::Float, this });
		outputs.push_back({ "", ValueType::Float, this });
	}

	std::string Emit(ShaderGenContext& ctx) override
	{
		return EmitInput(inputs[0], ctx) + "." + swizzle;
	}

	static int SwizzleCharFilter(ImGuiInputTextCallbackData* data)
	{
		auto c = data->EventChar;
		if (c == 'x' || c == 'y' || c == 'z' || c == 'w')
			return 0; // allow

		return 1; // block
	}

	void Draw() override
	{
		BeginDraw();
		DrawTitle();

		ImGui::SetNextItemWidth(100);
		if (ImGui::InputText("##swizzle", swizzle, 5, ImGuiInputTextFlags_CallbackCharFilter, SwizzleCharFilter))
		{
			if (!swizzle[0])
				swizzle[0] = 'x';

			outputs[0].type = (ValueType)strlen(swizzle);
		}

		DrawPins();
		EndDraw();
	}
};
