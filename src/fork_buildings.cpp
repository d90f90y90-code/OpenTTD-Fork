/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/**
 * @file fork_buildings.cpp The buildings of the firms in the towns.
 *
 * When a firm is founded, a glass headquarters and a white laboratory are built on free tiles near the middle of the town.
 * They go through three stages (foundations, under construction, finished) as the firm gets older. The tiles are objects of the
 * game that nobody can remove; they are drawn with the pictures of the firm instead of the normal object, and a click on one of
 * them opens the window of the firm.
 */

#include "stdafx.h"

#include "clear_func.h"
#include "clear_map.h"
#include "company_func.h"
#include "fork_buildings.h"
#include "fork_building_sprites_data.h"
#include "fork_firm.h"
#include "landscape.h"
#include "map_func.h"
#include "object.h"
#include "object_base.h"
#include "spritecache.h"
#include "table/sprites.h"
#include "tilearea_spiral.h"
#include "tile_map.h"
#include "town.h"
#include "transparency.h"
#include "viewport_func.h"

#include "table/strings.h"

#include "safeguards.h"

static constexpr size_t BUILDING_SPRITES = lengthof(_fork_building_sprite_data); ///< Number of sprites: 2 buildings, 4 designs, 3 stages.
static SpriteID _fork_building_sprite_base = 0; ///< First sprite of the buildings in the sprite cache, or 0 if they are not there.

/** Make the sprites of the buildings, if they are not in the sprite cache (yet). */
static SpriteID EnsureBuildingSprites()
{
	if (_fork_building_sprite_base != 0 && ForkMemorySpritesIntact(_fork_building_sprite_base, BUILDING_SPRITES)) return _fork_building_sprite_base;

	std::vector<ForkMemorySprite> sprites;
	sprites.reserve(BUILDING_SPRITES);
	for (const ForkBuildingSpriteData &d : _fork_building_sprite_data) {
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
			if (index < 15) colour = 0xFF000000U | (d.palette[index * 3] << 16) | (d.palette[index * 3 + 1] << 8) | d.palette[index * 3 + 2];
			for (uint j = 0; j < run && pos < s.pixels.size(); j++) s.pixels[pos++] = colour;
		}
		sprites.push_back(std::move(s));
	}

	_fork_building_sprite_base = AddForkMemorySprites(std::move(sprites));
	return _fork_building_sprite_base;
}

/**
 * Find the firm that a building tile belongs to.
 * @param tile The tile.
 * @param[out] firm The firm.
 * @param[out] laboratory Whether it is the laboratory (otherwise the headquarters).
 * @return True if the tile is a building of a firm.
 */
static bool FindBuilding(TileIndex tile, const ForkFirm *&firm, bool &laboratory)
{
	if (!IsTileType(tile, TileType::Object)) return false;
	for (const ForkFirm *f : ForkFirm::Iterate()) {
		if (f->hq_tile != 0 && f->hq_tile == tile.base()) {
			firm = f;
			laboratory = false;
			return true;
		}
		if (f->lab_tile != 0 && f->lab_tile == tile.base()) {
			firm = f;
			laboratory = true;
			return true;
		}
	}
	return false;
}

/**
 * Draw a building of a firm instead of the object that is on the tile.
 * @param ti Information about the tile.
 * @return True if the tile was drawn.
 */
bool DrawForkBuildingTile(const TileInfo *ti)
{
	const ForkFirm *f;
	bool lab;
	if (!FindBuilding(ti->tile, f, lab)) return false;

	DrawClearLandTile(ti, 3);
	if (IsInvisibilitySet(TransparencyOption::Structures)) return true;

	const SpriteID base = EnsureBuildingSprites();
	if (base == 0) return true;

	const uint stage = f->age_months < 2 ? 0 : (f->age_months < 6 ? 1 : 2);
	const uint design = lab ? f->product % 4 : f->index.base() % 4;
	AddSortableSpriteToDraw(base + (lab ? 12 : 0) + design * 3 + stage, PAL_NONE, ti->x, ti->y, ti->z, SpriteBounds{{0, 0, 0}, {16, 16, 64}, {0, 0, 0}}, IsTransparencySet(TransparencyOption::Structures));
	return true;
}

/**
 * A click on a tile: a building of a firm opens the window of the firm.
 * @param tile The tile.
 * @return True if the click was used.
 */
bool ClickForkBuildingTile(TileIndex tile)
{
	const ForkFirm *f;
	bool lab;
	if (!FindBuilding(tile, f, lab)) return false;

	if (f->owner == _local_company) {
		ShowForkFirmWindow(f->index.base());
	} else {
		ShowForkFirmsWindow(f->town);
	}
	return true;
}

/**
 * Describe a building of a firm for the land information window.
 * @param tile The tile.
 * @param[out] td The description.
 * @return True if the tile is a building of a firm.
 */
bool GetForkBuildingTileDesc(TileIndex tile, TileDesc &td)
{
	const ForkFirm *f;
	bool lab;
	if (!FindBuilding(tile, f, lab)) return false;

	td.str = lab ? STR_FORK_BUILDING_LAB : STR_FORK_BUILDING_HQ;
	td.owner[0] = OWNER_NONE;
	return true;
}

/** @return Whether a building can be put on the tile: free flat ground, or trees that can be cut down. */
static bool IsFreeForBuilding(TileIndex tile)
{
	if (!IsValidTile(tile) || !IsTileFlat(tile)) return false;
	if (IsTileType(tile, TileType::Trees)) return true;
	return IsTileType(tile, TileType::Clear) && !IsClearGround(tile, ClearGround::Fields) && IsTileOwner(tile, OWNER_NONE);
}

/** Put the object that is the building on a tile. */
static void BuildOnTile(TileIndex tile, Town *t)
{
	if (IsTileType(tile, TileType::Trees)) DoClearSquare(tile);
	BuildObject(OBJECT_STATUE, tile, OWNER_NONE, t);
}

/**
 * Build the headquarters and the laboratory of a firm on free tiles next to each other, near the middle of the town.
 * Nothing is built if there is no room; it is tried again later.
 * @param f The firm.
 */
void PlaceForkFirmBuildings(ForkFirm *f)
{
	if (f->hq_tile != 0 || f->lab_tile != 0) return;

	Town *t = Town::GetIfValid(f->GetTown());
	if (t == nullptr || !Object::CanAllocateItem(2)) return;

	static const int offsets[4][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
	for (TileIndex a : SpiralTileSequence(t->xy, 2 * 14 + 1)) {
		const uint distance = DistanceManhattan(a, t->xy);
		if (distance < 3 || !IsFreeForBuilding(a)) continue;

		for (const auto &o : offsets) {
			const int x = static_cast<int>(TileX(a)) + o[0];
			const int y = static_cast<int>(TileY(a)) + o[1];
			if (x < 1 || y < 1 || x >= static_cast<int>(Map::SizeX()) - 1 || y >= static_cast<int>(Map::SizeY()) - 1) continue;

			const TileIndex b = TileXY(x, y);
			if (!IsFreeForBuilding(b)) continue;

			BuildOnTile(a, t);
			BuildOnTile(b, t);
			f->hq_tile = a.base();
			f->lab_tile = b.base();
			return;
		}
	}
}
