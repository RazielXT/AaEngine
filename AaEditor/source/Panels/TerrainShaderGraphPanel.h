#pragma once

class ShaderDefines;

class TerrainShaderGraphPanel
{
public:

	TerrainShaderGraphPanel(ShaderDefines& shaderDefines);

	void draw();

private:

	ShaderDefines& shaderDefines;
};
