/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/**
 * @file fork_buildings.cpp Buildings of the fork's economy that stand on the map.
 *
 * A school or university takes 2x2 free tiles near the middle of its town. The tiles are objects of the game that nobody can
 * remove (statues); they are drawn with the picture of the building instead, and a click on one opens the window of the
 * institution. The building goes through three stages (foundations, frame, finished) as the institution gets older.
 */

#include "stdafx.h"

#include "clear_func.h"
#include "clear_map.h"
#include "company_func.h"
#include "fork_building_sprites.h"
#include "fork_buildings.h"
#include "fork_economy.h"
#include "fork_education_sprites_data.h"
#include "landscape.h"
#include "map_func.h"
#include "object.h"
#include "object_base.h"
#include "spritecache.h"
#include "table/sprites.h"
#include "tile_map.h"
#include "tilearea_spiral.h"
#include "town.h"
#include "transparency.h"
#include "viewport_func.h"

#include "table/strings.h"

#include "safeguards.h"

static constexpr size_t EDU_SPRITES = lengthof(_fork_edu_sprite_data); ///< Number of sprites: 3 kinds, 3 stages.
static SpriteID _fork_edu_sprite_base = 0; ///< First sprite of the schools in the sprite cache, or 0 if they are not there.

/** Make the sprites of the schools, if they are not in the sprite cache (yet). */
static SpriteID EnsureEducationSprites()
{
	if (_fork_edu_sprite_base != 0 && ForkMemorySpritesIntact(_fork_edu_sprite_base, EDU_SPRITES)) return _fork_edu_sprite_base;
	_fork_edu_sprite_base = AddForkMemorySprites(MakeForkMemorySprites(_fork_edu_sprite_data, EDU_SPRITES));
	return _fork_edu_sprite_base;
}

/** @return The tile in the north corner of the building of an institution, or false when it has no building. */
static bool GetOrigin(const ForkAsset *a, TileIndex &origin)
{
	if (a->type != static_cast<uint8_t>(ForkAssetType::Education) || a->data2[3] <= 0) return false;
	origin = TileIndex{static_cast<uint32_t>(a->data2[3] - 1)};
	return true;
}

/**
 * Find the institution that a tile belongs to.
 * @param tile The tile.
 * @param[out] school The institution.
 * @param[out] anchor Whether it is the southern tile of the building, the one the picture is drawn on.
 * @return True if the tile is part of the building of an institution.
 */
static bool FindBuilding(TileIndex tile, const ForkAsset *&school, bool &anchor)
{
	if (!IsTileType(tile, TileType::Object)) return false;
	for (const ForkAsset *a : ForkAsset::Iterate()) {
		TileIndex origin;
		if (!GetOrigin(a, origin)) continue;
		const int dx = static_cast<int>(TileX(tile)) - static_cast<int>(TileX(origin));
		const int dy = static_cast<int>(TileY(tile)) - static_cast<int>(TileY(origin));
		if (dx < 0 || dy < 0 || dx > 1 || dy > 1) continue;
		school = a;
		anchor = dx == 1 && dy == 1;
		return true;
	}
	return false;
}

/** @return The number of the stage the building is in: 0 foundations, 1 frame, 2 finished. */
uint GetForkSchoolStage(const ForkAsset *a)
{
	TileIndex origin;
	if (!GetOrigin(a, origin)) return 2; // Institutions without a building are always open.

	static const uint framed[] = {2, 2, 3}; // Months until the frame stands.
	static const uint finished[] = {4, 5, 8}; // Months until the building is finished.
	const uint kind = std::min<uint>(static_cast<uint>(a->data[0]), 2);
	if (a->age_months < framed[kind]) return 0;
	return a->age_months < finished[kind] ? 1 : 2;
}

bool DrawForkBuildingTile(const TileInfo *ti)
{
	const ForkAsset *a;
	bool anchor;
	if (!FindBuilding(ti->tile, a, anchor)) return false;

	DrawClearLandTile(ti, 3);
	if (!anchor || IsInvisibilitySet(TransparencyOption::Structures)) return true;

	const SpriteID base = EnsureEducationSprites();
	if (base == 0) return true;

	const uint kind = std::min<uint>(static_cast<uint>(a->data[0]), 2);
	AddSortableSpriteToDraw(base + kind * 3 + GetForkSchoolStage(a), PAL_NONE, ti->x, ti->y, ti->z, SpriteBounds{{0, 0, 0}, {16, 16, 64}, {0, 0, 0}}, IsTransparencySet(TransparencyOption::Structures));
	return true;
}

bool ClickForkBuildingTile(TileIndex tile)
{
	const ForkAsset *a;
	bool anchor;
	if (!FindBuilding(tile, a, anchor) || a->owner != _local_company) return false;

	ShowForkEducationWindow(static_cast<uint>(a->location));
	return true;
}

bool GetForkBuildingTileDesc(TileIndex tile, TileDesc &td)
{
	const ForkAsset *a;
	bool anchor;
	if (!FindBuilding(tile, a, anchor)) return false;

	td.str = STR_FORK_EDU_NAME_PRIMARY + std::min<uint>(static_cast<uint>(a->data[0]), 2);
	td.owner[0] = a->owner;
	return true;
}

/** @return Whether a building can be put on the tile: free flat ground, or trees that can be cut down. */
static bool IsFreeForBuilding(TileIndex tile)
{
	if (!IsValidTile(tile) || !IsTileFlat(tile)) return false;
	if (IsTileType(tile, TileType::Trees)) return true;
	return IsTileType(tile, TileType::Clear) && !IsClearGround(tile, ClearGround::Fields) && IsTileOwner(tile, OWNER_NONE);
}

/**
 * Look for a place near the middle of a town where a building of 2x2 tiles fits.
 * @param t The town.
 * @param[out] origin The northern tile of the place.
 * @return True if there is a place.
 */
bool FindForkBuildingSite(const Town *t, TileIndex &origin)
{
	if (!Object::CanAllocateItem(4)) return false;

	for (TileIndex a : SpiralTileSequence(t->xy, 2 * 20 + 1)) {
		if (DistanceManhattan(a, t->xy) < 2) continue;

		const int x = static_cast<int>(TileX(a));
		const int y = static_cast<int>(TileY(a));
		if (x < 1 || y < 1 || x + 2 >= static_cast<int>(Map::SizeX()) || y + 2 >= static_cast<int>(Map::SizeY())) continue;

		if (IsFreeForBuilding(a) && IsFreeForBuilding(TileXY(x + 1, y)) && IsFreeForBuilding(TileXY(x, y + 1)) && IsFreeForBuilding(TileXY(x + 1, y + 1))) {
			origin = a;
			return true;
		}
	}
	return false;
}

/**
 * Build the building of an institution on the map.
 * @param a The institution.
 * @param t The town.
 * @param origin The northern tile of the place, found with #FindForkBuildingSite.
 */
void PlaceForkSchoolBuilding(ForkAsset *a, Town *t, TileIndex origin)
{
	const int x = static_cast<int>(TileX(origin));
	const int y = static_cast<int>(TileY(origin));
	for (int dy = 0; dy < 2; dy++) {
		for (int dx = 0; dx < 2; dx++) {
			const TileIndex tile = TileXY(x + dx, y + dy);
			if (IsTileType(tile, TileType::Trees)) DoClearSquare(tile);
			BuildObject(OBJECT_STATUE, tile, OWNER_NONE, t);
		}
	}
	a->data2[3] = static_cast<int64_t>(origin.base()) + 1;
}

/**
 * Take the building of an institution off the map.
 * @param a The institution.
 */
void RemoveForkSchoolBuilding(ForkAsset *a)
{
	TileIndex origin;
	if (!GetOrigin(a, origin)) return;

	const int x = static_cast<int>(TileX(origin));
	const int y = static_cast<int>(TileY(origin));
	for (int dy = 0; dy < 2; dy++) {
		for (int dx = 0; dx < 2; dx++) {
			ForkClearObjectTile(TileXY(x + dx, y + dy));
		}
	}
	a->data2[3] = 0;
}

/** Draw the building again, for example because it moved on to the next stage. */
void RefreshForkSchoolBuilding(const ForkAsset *a)
{
	TileIndex origin;
	if (!GetOrigin(a, origin)) return;

	const int x = static_cast<int>(TileX(origin));
	const int y = static_cast<int>(TileY(origin));
	for (int dy = 0; dy < 2; dy++) {
		for (int dx = 0; dx < 2; dx++) MarkTileDirtyByTile(TileXY(x + dx, y + dy));
	}
}
