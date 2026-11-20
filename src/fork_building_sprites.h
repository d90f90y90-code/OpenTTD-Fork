/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_building_sprites.h Turn the building pictures that are compiled into the game into sprites. */

#ifndef FORK_BUILDING_SPRITES_H
#define FORK_BUILDING_SPRITES_H

#include "spritecache.h"

#include <vector>

/**
 * Make the sprites for pictures that are stored as palette plus run lengths.
 * @tparam Data Type of one picture: a #ForkMapSpriteData.
 * @param data The pictures.
 * @param count Number of pictures.
 * @return The sprites, to be put in the sprite cache.
 */
template <class Data>
std::vector<ForkMemorySprite> MakeForkMemorySprites(const Data *data, size_t count)
{
	std::vector<ForkMemorySprite> sprites;
	sprites.reserve(count);
	for (size_t n = 0; n < count; n++) {
		const Data &d = data[n];
		ForkMemorySprite s;
		s.width = d.width;
		s.height = d.height;
		s.x_offs = d.x_offs;
		s.y_offs = d.y_offs;
		s.pixels.assign(static_cast<size_t>(d.width) * d.height, 0);

		size_t pos = 0;
		for (size_t i = 0; i + 1 < d.rle_size && pos < s.pixels.size(); i += 2) {
			const uint run = d.rle[i];
			const uint index = d.rle[i + 1];
			uint32_t colour = 0;
			if (index < d.palette_size) colour = 0xFF000000U | (static_cast<uint32_t>(d.palette[index * 3]) << 16) | (static_cast<uint32_t>(d.palette[index * 3 + 1]) << 8) | d.palette[index * 3 + 2];
			for (uint j = 0; j < run && pos < s.pixels.size(); j++) s.pixels[pos++] = colour;
		}
		sprites.push_back(std::move(s));
	}
	return sprites;
}

#endif /* FORK_BUILDING_SPRITES_H */
