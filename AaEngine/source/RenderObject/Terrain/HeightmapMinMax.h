#pragma once

#include "Utils/MathUtils.h"
#include <vector>
#include <algorithm>

class HeightmapMinMax
{
public:

	HeightmapMinMax() = default;

	void init(UINT size, float e)
	{
		extends = e;
		buildMipStorage(size);
		reset();
	}

	void load(const void* source, UINT rowPitchBytes)
	{
		auto* dst = mips[0].data();

		for (UINT y = 0; y < gridWidth; ++y)
		{
			auto* row = reinterpret_cast<const Vector2*>(
				static_cast<const uint8_t*>(source) + y * rowPitchBytes);

			for (UINT x = 0; x < gridWidth; ++x)
				*dst++ = fromNormalized(row[x]);
		}

		generateMipmaps();
	}

	void reset()
	{
		for (auto& mip : mips)
		{
			for (auto& d : mip)
				d = { -extends, extends };
		}
	}

	Vector2 get(UINT mip, UINT x, UINT y) const
	{
		return mips[mip][x + y * mipWidths[mip]];
	}

	UINT getMipCount() const
	{
		return static_cast<UINT>(mips.size());
	}

	UINT getMipWidth(UINT mip) const
	{
		return mipWidths[mip];
	}

	void buildMipStorage(UINT size)
	{
		gridWidth = size;

		mips.clear();
		mipWidths.clear();

		UINT w = size;

		while (true)
		{
			mipWidths.push_back(w);
			mips.emplace_back(w * w);

			if (w == 1)
				break;

			w = std::max(1u, w >> 1);
		}
	}

	void generateMipmaps()
	{
		for (size_t mip = 1; mip < mips.size(); ++mip)
		{
			UINT srcW = mipWidths[mip - 1];
			UINT dstW = mipWidths[mip];

			auto& src = mips[mip - 1];
			auto& dst = mips[mip];

			for (UINT y = 0; y < dstW; ++y)
			{
				for (UINT x = 0; x < dstW; ++x)
				{
					UINT sx = x * 2;
					UINT sy = y * 2;

					const Vector2& a = src[sx + sy * srcW];
					const Vector2& b = src[sx + 1 + sy * srcW];
					const Vector2& c = src[sx + (sy + 1) * srcW];
					const Vector2& d = src[sx + 1 + (sy + 1) * srcW];

					dst[x + y * dstW] =
					{
						std::min({ a.x, b.x, c.x, d.x }),
						std::max({ a.y, b.y, c.y, d.y })
					};
				}
			}
		}
	}

	Vector2 fromNormalized(Vector2 h) const
	{
		return (h - Vector2(0.5f)) * 2 * extends;
	}

	std::vector<std::vector<Vector2>> mips;
	std::vector<UINT> mipWidths;

	float extends = 500.f;
	UINT gridWidth = 0;
};
