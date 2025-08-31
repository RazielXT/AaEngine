#include "ScreenQuad.h"
#include "RenderQueue.h"
#include "Material.h"

void ScreenQuad::Render(AssignedMaterial* material, const RenderTargetTexture& target, const RenderProvider& provider, RenderContext& ctx, ID3D12GraphicsCommandList* commandList) const
{
	ShaderConstantsProvider constants(provider.params, {}, * ctx.camera, target);
	MaterialDataStorage storage;

	material->GetBase()->BindSignature(commandList);

	material->LoadMaterialConstants(storage);
	memcpy(storage.rootParams.data(), &data, storage.rootParams.size() * sizeof(float));

	material->UpdatePerFrame(storage, constants);
	material->BindPipeline(commandList);
	material->BindTextures(commandList);
	material->BindConstants(commandList, storage, constants);

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawInstanced(6, 1, 0, 0);
}

void ScreenQuad::SetPosition(Vector2 offset, float size, RelativePosition relative, float aspectRatio)
{
	data = {};

	for (auto& v : data.vertices)
	{
		v.x *= size;  // Adjust position for width and move to top-right
		v.y *= size;                      // Adjust height for quad width
	}

	if (aspectRatio > 1.0f)
	{
		// Width is greater or equal to height
		float scaleX = 1.0f / aspectRatio;
		for (auto& v : data.vertices)
		{
			v.x *= scaleX;
		}
	}
	else if (aspectRatio < 1.0f)
	{
		// Height is greater than width
		float scaleY = aspectRatio;
		for (auto& v : data.vertices)
		{
			v.y *= scaleY;
		}
	}

	if (relative & Right)
	{
		auto& v = data.vertices;
		auto prev = v[1];
		v[1].x = 1 - v[0].x;
		v[0].x = 1 - prev.x;
		prev = v[3];
		v[3].x = 1 - v[2].x;
		v[2].x = 1 - prev.x;
	}
	if (relative & Top)
	{
		auto& v = data.vertices;
		auto prev = v[1];
		v[1].y = 1 - v[3].y;
		v[3].y = 1 - prev.y;
		prev = v[0];
		v[0].y = 1 - v[2].y;
		v[2].y = 1 - prev.y;
	}

	for (auto& v : data.vertices)
	{
		v.x += offset.x;
		v.y += offset.y;
		v.x = v.x * 2 - 1;
		v.y = v.y * 2 - 1;
	}
}

void ScreenQuad::SetPosition(Vector2 size)
{
	data = {};

	for (auto& v : data.vertices)
	{
		if (v.x == 0) v.x = -1.f;
		if (v.y == 0) v.y = -1.f;
	}
}
