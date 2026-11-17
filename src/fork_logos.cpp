/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/**
 * @file fork_logos.cpp Drawing of the fork's logos.
 *
 * The logos are small run-length encoded images (see fork_logos_data.h, made from media/fork/*.png).
 * They are scaled to the size that is asked for, matched to the colours of the game palette and drawn as
 * horizontal lines of one colour, so they need no sprite and work with every blitter and zoom level.
 */

#include "stdafx.h"

#include "fork_logos.h"
#include "gfx_func.h"
#include "palette_func.h"

#include "fork_buildings_data.h"
#include "fork_logos_data.h"

#include "safeguards.h"

/** An encoded logo. */
struct ForkLogoSource {
	int width; ///< Width of the image in pixels.
	int height; ///< Height of the image in pixels.
	const uint8_t *palette; ///< 15 colours as RGB triplets; index 15 is transparent.
	const uint8_t *rle; ///< Pairs of run length and colour index.
	size_t rle_size; ///< Size of #rle in bytes.
};

static const ForkLogoSource _fork_logo_sources[] = {
	{128, 97, _fork_logo_carmarket_palette, _fork_logo_carmarket_rle, sizeof(_fork_logo_carmarket_rle)},
	{128, 91, _fork_logo_ttdmax_palette, _fork_logo_ttdmax_rle, sizeof(_fork_logo_ttdmax_rle)},
	{128, 54, _fork_logo_tajcar_palette, _fork_logo_tajcar_rle, sizeof(_fork_logo_tajcar_rle)},
	{128, 33, _fork_logo_limano_palette, _fork_logo_limano_rle, sizeof(_fork_logo_limano_rle)},
	{128, 128, _fork_logo_seamar_palette, _fork_logo_seamar_rle, sizeof(_fork_logo_seamar_rle)},
	{128, 128, _fork_logo_airix_palette, _fork_logo_airix_rle, sizeof(_fork_logo_airix_rle)},
	{96, 96, _fork_building_hq0_0_palette, _fork_building_hq0_0_rle, sizeof(_fork_building_hq0_0_rle)},
	{96, 96, _fork_building_hq0_1_palette, _fork_building_hq0_1_rle, sizeof(_fork_building_hq0_1_rle)},
	{96, 96, _fork_building_hq0_2_palette, _fork_building_hq0_2_rle, sizeof(_fork_building_hq0_2_rle)},
	{96, 96, _fork_building_hq1_0_palette, _fork_building_hq1_0_rle, sizeof(_fork_building_hq1_0_rle)},
	{96, 96, _fork_building_hq1_1_palette, _fork_building_hq1_1_rle, sizeof(_fork_building_hq1_1_rle)},
	{96, 96, _fork_building_hq1_2_palette, _fork_building_hq1_2_rle, sizeof(_fork_building_hq1_2_rle)},
	{96, 96, _fork_building_hq2_0_palette, _fork_building_hq2_0_rle, sizeof(_fork_building_hq2_0_rle)},
	{96, 96, _fork_building_hq2_1_palette, _fork_building_hq2_1_rle, sizeof(_fork_building_hq2_1_rle)},
	{96, 96, _fork_building_hq2_2_palette, _fork_building_hq2_2_rle, sizeof(_fork_building_hq2_2_rle)},
	{96, 96, _fork_building_hq3_0_palette, _fork_building_hq3_0_rle, sizeof(_fork_building_hq3_0_rle)},
	{96, 96, _fork_building_hq3_1_palette, _fork_building_hq3_1_rle, sizeof(_fork_building_hq3_1_rle)},
	{96, 96, _fork_building_hq3_2_palette, _fork_building_hq3_2_rle, sizeof(_fork_building_hq3_2_rle)},
	{96, 96, _fork_building_lab0_0_palette, _fork_building_lab0_0_rle, sizeof(_fork_building_lab0_0_rle)},
	{96, 96, _fork_building_lab0_1_palette, _fork_building_lab0_1_rle, sizeof(_fork_building_lab0_1_rle)},
	{96, 96, _fork_building_lab0_2_palette, _fork_building_lab0_2_rle, sizeof(_fork_building_lab0_2_rle)},
	{96, 96, _fork_building_lab1_0_palette, _fork_building_lab1_0_rle, sizeof(_fork_building_lab1_0_rle)},
	{96, 96, _fork_building_lab1_1_palette, _fork_building_lab1_1_rle, sizeof(_fork_building_lab1_1_rle)},
	{96, 96, _fork_building_lab1_2_palette, _fork_building_lab1_2_rle, sizeof(_fork_building_lab1_2_rle)},
	{96, 96, _fork_building_lab2_0_palette, _fork_building_lab2_0_rle, sizeof(_fork_building_lab2_0_rle)},
	{96, 96, _fork_building_lab2_1_palette, _fork_building_lab2_1_rle, sizeof(_fork_building_lab2_1_rle)},
	{96, 96, _fork_building_lab2_2_palette, _fork_building_lab2_2_rle, sizeof(_fork_building_lab2_2_rle)},
	{96, 96, _fork_building_lab3_0_palette, _fork_building_lab3_0_rle, sizeof(_fork_building_lab3_0_rle)},
	{96, 96, _fork_building_lab3_1_palette, _fork_building_lab3_1_rle, sizeof(_fork_building_lab3_1_rle)},
	{96, 96, _fork_building_lab3_2_palette, _fork_building_lab3_2_rle, sizeof(_fork_building_lab3_2_rle)},
};

static constexpr size_t FORK_LOGO_COUNT = static_cast<size_t>(ForkLogo::End);
static_assert(std::size(_fork_logo_sources) == FORK_LOGO_COUNT);

/** A logo that has been scaled to a size, ready to be drawn. */
struct ForkLogoRender {
	int width = 0; ///< Width it was made for.
	int height = 0; ///< Height it was made for.
	std::vector<uint8_t> pixels; ///< Palette index of each pixel; 0 is transparent.
};

static std::array<std::vector<uint8_t>, FORK_LOGO_COUNT> _fork_logo_decoded; ///< Colour index of each pixel of the logos, once they are used.
static std::array<ForkLogoRender, FORK_LOGO_COUNT> _fork_logo_renders; ///< Scaled versions of the logos.

/** Unpack the run-length encoded image of a logo. */
static const std::vector<uint8_t> &GetDecodedLogo(size_t id)
{
	std::vector<uint8_t> &decoded = _fork_logo_decoded[id];
	if (!decoded.empty()) return decoded;

	const ForkLogoSource &src = _fork_logo_sources[id];
	decoded.reserve(static_cast<size_t>(src.width) * src.height);
	for (size_t i = 0; i + 1 < src.rle_size; i += 2) decoded.insert(decoded.end(), src.rle[i], src.rle[i + 1]);
	decoded.resize(static_cast<size_t>(src.width) * src.height, 15);
	return decoded;
}

/** Find the colour of the game palette that is closest to a colour. */
static uint8_t FindNearestPaletteIndex(int r, int g, int b)
{
	static std::map<uint32_t, uint8_t> cache;
	const uint32_t key = (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
	auto it = cache.find(key);
	if (it != cache.end()) return it->second;

	/* Only the steady colours: not the animated ones (water, lights) nor the special ranges. */
	int best = 1;
	int best_distance = INT32_MAX;
	for (int i = 1; i < 0xC0; i++) {
		const Colour &c = _cur_palette.palette[i];
		const int dr = r - c.r;
		const int dg = g - c.g;
		const int db = b - c.b;
		const int distance = 3 * dr * dr + 4 * dg * dg + 2 * db * db;
		if (distance < best_distance) {
			best_distance = distance;
			best = i;
		}
	}
	cache[key] = static_cast<uint8_t>(best);
	return static_cast<uint8_t>(best);
}

/** Scale a logo to a size, averaging the pixels it covers. */
static void RenderLogo(size_t id, int width, int height)
{
	const ForkLogoSource &src = _fork_logo_sources[id];
	const std::vector<uint8_t> &decoded = GetDecodedLogo(id);

	ForkLogoRender &render = _fork_logo_renders[id];
	render.width = width;
	render.height = height;
	render.pixels.assign(static_cast<size_t>(width) * height, 0);

	for (int ty = 0; ty < height; ty++) {
		const int sy0 = ty * src.height / height;
		const int sy1 = std::max(sy0 + 1, (ty + 1) * src.height / height);
		for (int tx = 0; tx < width; tx++) {
			const int sx0 = tx * src.width / width;
			const int sx1 = std::max(sx0 + 1, (tx + 1) * src.width / width);

			int total = 0;
			int opaque = 0;
			int sum_r = 0;
			int sum_g = 0;
			int sum_b = 0;
			for (int sy = sy0; sy < sy1 && sy < src.height; sy++) {
				for (int sx = sx0; sx < sx1 && sx < src.width; sx++) {
					const uint8_t v = decoded[static_cast<size_t>(sy) * src.width + sx];
					total++;
					if (v >= 15) continue;
					opaque++;
					sum_r += src.palette[v * 3];
					sum_g += src.palette[v * 3 + 1];
					sum_b += src.palette[v * 3 + 2];
				}
			}
			if (opaque == 0 || opaque * 2 < total) continue;

			render.pixels[static_cast<size_t>(ty) * width + tx] = FindNearestPaletteIndex(sum_r / opaque, sum_g / opaque, sum_b / opaque);
		}
	}
}

/**
 * Get the image of a building of a firm.
 * @param laboratory True for the laboratory, false for the glass headquarters.
 * @param design Which of the 4 designs.
 * @param stage How far the construction is: 0 foundations, 1 under construction, 2 finished.
 * @return The logo.
 */
ForkLogo GetForkBuildingLogo(bool laboratory, uint design, uint stage)
{
	return static_cast<ForkLogo>(static_cast<uint>(ForkLogo::Buildings) + (laboratory ? 12 : 0) + (design % 4) * 3 + std::min<uint>(stage, 2));
}

/**
 * Draw a logo as large as possible inside a rectangle, centered, keeping its proportions.
 * @param logo Logo to draw.
 * @param r Rectangle to draw in.
 */
void DrawForkLogo(ForkLogo logo, const Rect &r)
{
	const size_t id = static_cast<size_t>(logo);
	if (id >= FORK_LOGO_COUNT) return;

	const ForkLogoSource &src = _fork_logo_sources[id];
	const int avail_w = r.Width();
	const int avail_h = r.Height();
	if (avail_w <= 0 || avail_h <= 0) return;

	int width;
	int height;
	if (avail_w * src.height <= avail_h * src.width) {
		width = avail_w;
		height = std::max(1, avail_w * src.height / src.width);
	} else {
		height = avail_h;
		width = std::max(1, avail_h * src.width / src.height);
	}

	ForkLogoRender &render = _fork_logo_renders[id];
	if (render.width != width || render.height != height) RenderLogo(id, width, height);

	const int left = r.left + (avail_w - width) / 2;
	const int top = r.top + (avail_h - height) / 2;
	for (int y = 0; y < height; y++) {
		int x = 0;
		while (x < width) {
			const uint8_t colour = render.pixels[static_cast<size_t>(y) * width + x];
			if (colour == 0) {
				x++;
				continue;
			}
			int end = x + 1;
			while (end < width && render.pixels[static_cast<size_t>(y) * width + end] == colour) end++;
			GfxFillRect(left + x, top + y, left + end - 1, top + y, PixelColour(colour));
			x = end;
		}
	}
}
