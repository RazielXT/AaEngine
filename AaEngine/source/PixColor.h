#pragma once

#include <cstdint>

enum class PixColor : uint32_t
{
	None = 0,
	OrangeRed = 0xFFFF4500,
	Orange = 0xFFFFA500,
	Crimson = 0xFFDC143C,
	RedWine = 0xFFA1304a,
	JeansBlue = 0xFF083880,
	DodgerBlue = 0xFF1E90FF,
	DarkTurquoise = 0xFF00CED1,
	SteelBlue = 0xFF4682B4,
	LightBlue = 0xFF9fe5fc,
	ForestGreen = 0xFF228B22,
	OliveDrab = 0xFF6B8E23,
	BlueViolet = 0xFF8A2BE2,
	Orchid = 0xFFDA70D6,

	Nvidia = 0xFF76B900,
	Amd = 0xFFED1C24,

	Shadows = Crimson,
	EarlyZ = RedWine,
	SceneRender = JeansBlue,
	SceneTransparent = JeansBlue,
	Editor = Orchid,
	Debug = OliveDrab,
	Compositor = LightBlue,
	Compositor2 = SteelBlue,
	CompositorCompute = OrangeRed,
	SSAO = DodgerBlue,
	Terrain = ForestGreen,
	Foliage = ForestGreen,
	Upscale = Nvidia,
	Voxelize = BlueViolet,
	Load = Orange
};
