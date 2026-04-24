#pragma once

#include "NodeGraph/Node.h"

class AddNode : public Node
{
public:
	AddNode()
	{
		displayName = "Add";

		inputs.push_back({ "A", ValueType::Float, this });
		inputs.push_back({ "B", ValueType::Float, this });
		outputs.push_back({ "Result", ValueType::Float, this });
	}

	std::string Emit(ShaderGenContext& ctx) override
	{
		std::string a = EmitInput(inputs[0], ctx);
		std::string b = EmitInput(inputs[1], ctx);

		auto type = outputs[0].type;
		std::string var = ctx.NewTemp(type);
		std::string typeName = ctx.TypeName(type);

		ctx.code += typeName + " " + var + " = " + a + " + " + b + ";\n";
		return var;
	}
};

class MulNode : public Node
{
public:
	MulNode()
	{
		displayName = "Mul";

		inputs.push_back({ "A", ValueType::Float, this });
		inputs.push_back({ "B", ValueType::Float, this });
		outputs.push_back({ "Result", ValueType::Float, this });
	}

	std::string Emit(ShaderGenContext& ctx) override
	{
		std::string a = EmitInput(inputs[0], ctx);
		std::string b = EmitInput(inputs[1], ctx);

		auto type = outputs[0].type;
		std::string var = ctx.NewTemp(type);
		std::string typeName = ctx.TypeName(type);

		ctx.code += typeName + " " + var + " = " + a + " * " + b + ";\n";
		return var;
	}
};

class SinNode : public Node
{
public:
	SinNode()
	{
		displayName = "Sin";
		inputs.push_back({ "In", ValueType::Float, this });
		outputs.push_back({ "Out", ValueType::Float, this });
	}

	std::string Emit(ShaderGenContext& ctx) override
	{
		std::string in = EmitInput(inputs[0], ctx);

		auto type = outputs[0].type;
		std::string var = ctx.NewTemp(type);
		std::string typeName = ctx.TypeName(type);

		ctx.code += typeName + " " + var + " = sin(" + in + ");\n";
		return var;
	}
};

class PowNode : public Node
{
public:
	int power = 2;

	PowNode()
	{
		displayName = "Pow";
		inputs.push_back({ "", ValueType::Float, this });
		outputs.push_back({ "", ValueType::Float, this });
	}

	std::string Emit(ShaderGenContext& ctx) override
	{
		std::string in = EmitInput(inputs[0], ctx);

		auto type = outputs[0].type;
		std::string var = ctx.NewTemp(type);
		std::string typeName = ctx.TypeName(type);

		ctx.code += typeName + " " + var + " = pow(" + in + ", " + std::to_string(power) + ");\n";
		return var;
	}

	void Draw() override
	{
		BeginDraw();
		DrawTitle();

		ImGui::SetNextItemWidth(100);
		ImGui::InputInt("##power", &power);

		DrawPins();
		EndDraw();
	}
};

class SaturateNode : public Node
{
public:
	SaturateNode()
	{
		displayName = "Saturate";
		inputs.push_back({ "In", ValueType::Float, this });
		outputs.push_back({ "Out", ValueType::Float, this });
	}

	std::string Emit(ShaderGenContext& ctx) override
	{
		std::string in = EmitInput(inputs[0], ctx);
		return "saturate(" + in + ")";
	}
};
