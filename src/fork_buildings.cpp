/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/**
 * @file fork_buildings.cpp Buildings of the fork's economy that stand on the map.
 *
 * Two families of buildings are kept here:
 * - the glass headquarters and the white laboratory of a firm, and
 * - the schools and universities of an education asset.
 *
 * Every building occupies a 2x2 block of object tiles (statues that nobody can remove). The picture is drawn on the
 * south-east tile of the block; a click on any of the four tiles opens the window of the firm or institution.
 * Both families use the same in-memory sprite scaler and the same ground line, so feet stay on the ground after scaling.
 */

#include "stdafx.h"

#include <algorithm>
#include <array>
#include <vector>

#include "clear_func.h"
#include "clear_map.h"
#include "company_func.h"
#include "fork_building_sprites.h"
#include "fork_building_sprites_data.h"
#include "fork_buildings.h"
#include "fork_economy.h"
#include "fork_education_sprites_data.h"
#include "fork_firm.h"
#include "fork_products.h"
#include "landscape.h"
#include "map_func.h"
#include "object.h"
#include "object_base.h"
#include "object_map.h"
#include "sprite.h"
#include "spritecache.h"
#include "table/sprites.h"
#include "tile_map.h"
#include "tilearea_spiral.h"
#include "town.h"
#include "transparency.h"
#include "viewport_func.h"

#include "table/strings.h"

#include "safeguards.h"

static constexpr size_t EDU_SPRITES = lengthof(_fork_edu_sprite_data); ///< Number of education sprites: 3 kinds, 3 stages.
static constexpr size_t FIRM_SPRITES = lengthof(_fork_building_sprite_data); ///< Number of firm sprites: 2 buildings, 4 designs, 3 stages.
static constexpr uint FIRM_SPRITES_PER_BUILDING = 12; ///< HQ sprites come first, laboratory sprites follow.
static constexpr uint FIRM_SPRITES_PER_DESIGN = 3; ///< Foundations, frame, finished.

static constexpr uint FORK_SPRITE_MAX_SIZE = 256; ///< Sprite encoder limit in either direction.
static constexpr int FORK_BUILDING_BASE_BOTTOM = 32; ///< C++ equivalent of the Python BASE_BOTTOM=576: common ground line for every picture.

static constexpr uint FIRM_HQ_WIDTH = 90;  ///< Requested width of a headquarters picture.
static constexpr uint FIRM_HQ_HEIGHT = 160; ///< Requested height of a headquarters picture.
static constexpr uint FIRM_LAB_WIDTH = 110; ///< Requested width of a laboratory picture.
static constexpr uint FIRM_LAB_HEIGHT = 85;  ///< Requested height of a laboratory picture.

static constexpr uint EDU_STAGE_HEIGHTS[3] = {110, 130, 150}; ///< Sensible heights for foundations, frame and finished school.

/** Bounding box of one picture that stands on a 2x2 block (32x32 tile pixels in OpenTTD). */
static const SpriteBounds FORK_BUILDING_BOUNDS{{0, 0, 0}, {32, 32, 128}, {0, 0, 0}};

static SpriteID _fork_edu_sprite_base = 0;  ///< First sprite of the schools in the sprite cache, or 0 if they are not there.
static SpriteID _fork_firm_sprite_base = 0; ///< First sprite of the firm buildings in the sprite cache, or 0 if they are not there.

/**
 * Nearest-neighbour scale a memory sprite and keep its feet on the common ground line.
 *
 * The horizontal offset is scaled with the picture. The vertical offset is not scaled: just like the Python
 * BASE_BOTTOM=576 hint, the bottom of the drawn part is pinned to one ground line. This keeps all designs and
 * stages standing on the same line even though they have different original heights.
 * @param src The source picture.
 * @param dst_width Requested width, at most #FORK_SPRITE_MAX_SIZE.
 * @param dst_height Requested height, at most #FORK_SPRITE_MAX_SIZE.
 * @return The scaled picture.
 */
static ForkMemorySprite ScaleForkSprite(const ForkMemorySprite &src, uint16_t dst_width, uint16_t dst_height)
{
	assert(src.width != 0 && src.height != 0);
	assert(!src.pixels.empty());
	assert(dst_width > 0 && dst_width <= FORK_SPRITE_MAX_SIZE);
	assert(dst_height > 0 && dst_height <= FORK_SPRITE_MAX_SIZE);

	if (src.width == 0 || src.height == 0 || src.pixels.empty()) return src;

	dst_width = static_cast<uint16_t>(std::clamp<uint>(dst_width, 1, FORK_SPRITE_MAX_SIZE));
	dst_height = static_cast<uint16_t>(std::clamp<uint>(dst_height, 1, FORK_SPRITE_MAX_SIZE));

	ForkMemorySprite dst;
	dst.width = dst_width;
	dst.height = dst_height;
	dst.pixels.assign(static_cast<size_t>(dst_width) * dst_height, 0);

	for (uint y = 0; y < dst_height; y++) {
		const uint sy = std::min<uint>(src.height - 1, static_cast<uint>(y) * src.height / dst_height);
		for (uint x = 0; x < dst_width; x++) {
			const uint sx = std::min<uint>(src.width - 1, static_cast<uint>(x) * src.width / dst_width);
			const size_t index = static_cast<size_t>(sy) * src.width + sx;
			dst.pixels[static_cast<size_t>(y) * dst_width + x] = index < src.pixels.size() ? src.pixels[index] : 0;
		}
	}

	/* Round to nearest, without floating point, so the result is fully deterministic. */
	const int64_t numerator = static_cast<int64_t>(src.x_offs) * dst_width;
	const int64_t denominator = src.width;
	const int64_t scaled_x = numerator >= 0
		? (numerator + denominator / 2) / denominator
		: -((-numerator + denominator / 2) / denominator);
	dst.x_offs = static_cast<int16_t>(std::clamp<int64_t>(scaled_x, -dst_width, dst_width));
	dst.y_offs = static_cast<int16_t>(FORK_BUILDING_BASE_BOTTOM - static_cast<int>(dst_height));

	return dst;
}

/**
 * Width that keeps the aspect ratio of a source picture when it is scaled to a given height.
 * @param src The source picture.
 * @param height Requested height.
 * @return Proportional width, clamped to the sprite limit.
 */
static uint16_t ProportionalForkSpriteWidth(const ForkMemorySprite &src, uint16_t height)
{
	if (src.height == 0) return 1;
	const uint width = static_cast<uint>(src.width) * height / src.height;
	return static_cast<uint16_t>(std::clamp<uint>(width, 1, FORK_SPRITE_MAX_SIZE));
}

/** Make the education sprites, scaled proportionally to the requested stage height. */
static std::vector<ForkMemorySprite> MakeScaledEducationSprites()
{
	std::vector<ForkMemorySprite> sprites = MakeForkMemorySprites(_fork_edu_sprite_data, EDU_SPRITES);
	for (size_t i = 0; i < sprites.size(); i++) {
		const uint stage = static_cast<uint>(i % FIRM_SPRITES_PER_DESIGN);
		const uint16_t height = EDU_STAGE_HEIGHTS[std::min<uint>(stage, static_cast<uint>(lengthof(EDU_STAGE_HEIGHTS) - 1))];
		const uint16_t width = ProportionalForkSpriteWidth(sprites[i], height);
		sprites[i] = ScaleForkSprite(sprites[i], width, height);
	}
	return sprites;
}

/** Make the firm sprites: exact requested dimensions for the headquarters and the laboratory. */
static std::vector<ForkMemorySprite> MakeScaledFirmSprites()
{
	std::vector<ForkMemorySprite> sprites;
	sprites.reserve(FIRM_SPRITES);
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
			if (index < 15) colour = 0xFF000000U | (static_cast<uint32_t>(d.palette[index * 3]) << 16) | (static_cast<uint32_t>(d.palette[index * 3 + 1]) << 8) | d.palette[index * 3 + 2];
			for (uint j = 0; j < run && pos < s.pixels.size(); j++) s.pixels[pos++] = colour;
		}

		const bool laboratory = sprites.size() >= FIRM_SPRITES_PER_BUILDING;
		sprites.push_back(ScaleForkSprite(s,
			laboratory ? FIRM_LAB_WIDTH : FIRM_HQ_WIDTH,
			laboratory ? FIRM_LAB_HEIGHT : FIRM_HQ_HEIGHT));
	}
	return sprites;
}

/** Make the sprites of the schools, if they are not in the sprite cache (yet). */
static SpriteID EnsureEducationSprites()
{
	if (_fork_edu_sprite_base != 0 && ForkMemorySpritesIntact(_fork_edu_sprite_base, EDU_SPRITES)) return _fork_edu_sprite_base;
	_fork_edu_sprite_base = AddForkMemorySprites(MakeScaledEducationSprites());
	return _fork_edu_sprite_base;
}

/** Make the sprites of the firm buildings, if they are not in the sprite cache (yet). */
static SpriteID EnsureFirmSprites()
{
	if (_fork_firm_sprite_base != 0 && ForkMemorySpritesIntact(_fork_firm_sprite_base, FIRM_SPRITES)) return _fork_firm_sprite_base;
	_fork_firm_sprite_base = AddForkMemorySprites(MakeScaledFirmSprites());
	return _fork_firm_sprite_base;
}

/** @return The four tiles of a 2x2 block whose northern tile is #origin. */
static std::array<TileIndex, 4> GetBlockTiles(TileIndex origin)
{
	const uint x = TileX(origin);
	const uint y = TileY(origin);
	return {TileXY(x, y), TileXY(x + 1, y), TileXY(x, y + 1), TileXY(x + 1, y + 1)};
}

/** @return Whether two 2x2 blocks share a tile. */
static bool BlocksOverlap(TileIndex a, TileIndex b)
{
	for (TileIndex ta : GetBlockTiles(a)) {
		for (TileIndex tb : GetBlockTiles(b)) {
			if (ta == tb) return true;
		}
	}
	return false;
}

/** @return The northern tile of an institution's building, or false when it has no building. */
static bool GetOrigin(const ForkAsset *a, TileIndex &origin)
{
	if (a->type != static_cast<uint8_t>(ForkAssetType::Education) || a->data2[3] <= 0) return false;
	origin = TileIndex{static_cast<uint32_t>(a->data2[3] - 1)};
	return true;
}

/** @return The northern tile of a firm's headquarters or laboratory, or false when it is not built. */
static bool GetFirmOrigin(const ForkFirm *f, bool laboratory, TileIndex &origin)
{
	const uint32_t stored = laboratory ? f->lab_tile : f->hq_tile;
	if (stored == 0 || stored >= Map::Size()) return false;
	origin = TileIndex{stored};
	return true;
}

/**
 * Find the institution that a tile belongs to.
 * @param tile The tile.
 * @param[out] school The institution.
 * @param[out] anchor Whether it is the south-east tile of the building, the one the picture is drawn on.
 * @return True if the tile is part of the building of an institution.
 */
static bool FindEducationBuilding(TileIndex tile, const ForkAsset *&school, bool &anchor)
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

/**
 * Find the firm that a building tile belongs to.
 * @param tile The tile.
 * @param[out] firm The firm.
 * @param[out] laboratory Whether it is a tile of the laboratory (otherwise of the headquarters).
 * @param[out] anchor Whether it is the south-east tile of the block, the one the picture is drawn on.
 * @return True if the tile is part of a building of a firm.
 */
static bool FindFirmBuilding(TileIndex tile, const ForkFirm *&firm, bool &laboratory, bool &anchor)
{
	if (!IsTileType(tile, TileType::Object)) return false;
	for (const ForkFirm *f : ForkFirm::Iterate()) {
		for (uint b = 0; b < 2; b++) {
			const bool lab = b != 0;
			TileIndex origin;
			if (!GetFirmOrigin(f, lab, origin)) continue;
			const int dx = static_cast<int>(TileX(tile)) - static_cast<int>(TileX(origin));
			const int dy = static_cast<int>(TileY(tile)) - static_cast<int>(TileY(origin));
			if (dx < 0 || dy < 0 || dx > 1 || dy > 1) continue;
			firm = f;
			laboratory = lab;
			anchor = dx == 1 && dy == 1;
			return true;
		}
	}
	return false;
}

/** @return The number of the stage the school building is in: 0 foundations, 1 frame, 2 finished. */
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

/** Draw the picture of a firm building on the south-east tile of its 2x2 block. */
bool DrawForkFirmBuildingTile(const TileInfo *ti)
{
	const ForkFirm *f;
	bool lab;
	bool anchor;
	if (!FindFirmBuilding(ti->tile, f, lab, anchor)) return false;

	DrawClearLandTile(ti, 3);
	if (!anchor || IsInvisibilitySet(TransparencyOption::Structures)) return true;

	const SpriteID base = EnsureFirmSprites();
	if (base == 0) return true;

	const uint stage = f->age_months < 2 ? 0 : (f->age_months < 6 ? 1 : 2);
	const uint design = lab ? f->product % 4 : f->index.base() % 4;
	AddSortableSpriteToDraw(base + (lab ? FIRM_SPRITES_PER_BUILDING : 0) + design * FIRM_SPRITES_PER_DESIGN + stage,
		PAL_NONE, ti->x, ti->y, ti->z, FORK_BUILDING_BOUNDS, IsTransparencySet(TransparencyOption::Structures));
	return true;
}

/** Draw the picture of a school or university on the south-east tile of its 2x2 block. */
bool DrawForkEducationBuildingTile(const TileInfo *ti)
{
	const ForkAsset *a;
	bool anchor;
	if (!FindEducationBuilding(ti->tile, a, anchor)) return false;

	DrawClearLandTile(ti, 3);
	if (!anchor || IsInvisibilitySet(TransparencyOption::Structures)) return true;

	const SpriteID base = EnsureEducationSprites();
	if (base == 0) return true;

	const uint kind = std::min<uint>(static_cast<uint>(a->data[0]), 2);
	AddSortableSpriteToDraw(base + kind * FIRM_SPRITES_PER_DESIGN + GetForkSchoolStage(a),
		PAL_NONE, ti->x, ti->y, ti->z, FORK_BUILDING_BOUNDS, IsTransparencySet(TransparencyOption::Structures));
	return true;
}

/** A click on a firm building opens the window of the firm. */
bool ClickForkFirmBuildingTile(TileIndex tile)
{
	const ForkFirm *f;
	bool lab;
	bool anchor;
	if (!FindFirmBuilding(tile, f, lab, anchor)) return false;

	if (Company::IsValidID(f->owner) && f->owner == _local_company) {
		/* The company window shows the manager's face, the company name and the company statistics. */
		ShowForkCompanyWindow(f->owner);
	} else {
		ShowForkFirmsWindow(f->town);
	}
	return true;
}

/** A click on a school or university opens the window of the institution. */
bool ClickForkEducationBuildingTile(TileIndex tile)
{
	const ForkAsset *a;
	bool anchor;
	if (!FindEducationBuilding(tile, a, anchor) || a->owner != _local_company) return false;

	ShowForkEducationWindow(static_cast<uint>(a->location));
	return true;
}

/** Describe a firm building for the land information window. */
bool GetForkFirmBuildingTileDesc(TileIndex tile, TileDesc &td)
{
	const ForkFirm *f;
	bool lab;
	bool anchor;
	if (!FindFirmBuilding(tile, f, lab, anchor)) return false;

	/* The dedicated HQ/laboratory strings of the Android branch do not exist in this tree; reuse existing ones. */
	td.str = lab ? static_cast<StringID>(STR_FORK_FIRMS_CAPTION) : static_cast<StringID>(STR_LAI_OBJECT_DESCRIPTION_COMPANY_HEADQUARTERS);
	td.dparam = f->town;
	td.owner[0] = OWNER_NONE;
	return true;
}

/** Describe a school or university for the land information window. */
bool GetForkEducationBuildingTileDesc(TileIndex tile, TileDesc &td)
{
	const ForkAsset *a;
	bool anchor;
	if (!FindEducationBuilding(tile, a, anchor)) return false;

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

/** @return Whether a free, flat 2x2 block starts at (x,y); tile 0 is reserved for "not built". */
static bool FindFreeBlock(uint x, uint y, TileIndex &origin)
{
	if (x == 0 || y == 0 || x >= Map::SizeX() || y >= Map::SizeY() || x + 1 >= Map::SizeX() || y + 1 >= Map::SizeY()) return false;

	const TileIndex a = TileXY(x, y);
	if (!IsFreeForBuilding(a) || !IsFreeForBuilding(TileXY(x + 1, y)) ||
			!IsFreeForBuilding(TileXY(x, y + 1)) || !IsFreeForBuilding(TileXY(x + 1, y + 1))) return false;

	origin = a;
	return true;
}

/**
 * Look for a place near the middle of a town where a school building of 2x2 tiles fits.
 * @param t The town.
 * @param[out] origin The northern tile of the place.
 * @return True if there is a place.
 */
bool FindForkBuildingSite(const Town *t, TileIndex &origin)
{
	if (!Object::CanAllocateItem(4)) return false;

	for (TileIndex a : SpiralTileSequence(t->xy, 2 * 20 + 1)) {
		if (!IsValidTile(a) || DistanceManhattan(a, t->xy) < 2) continue;
		TileIndex found;
		if (FindFreeBlock(TileX(a), TileY(a), found)) {
			origin = found;
			return true;
		}
	}
	return false;
}

/** Build the four statue objects of one 2x2 block, recording every tile that was made. */
static bool BuildForkBlock(TileIndex origin, Town *t, std::vector<TileIndex> &built)
{
	for (TileIndex tile : GetBlockTiles(origin)) {
		if (IsTileType(tile, TileType::Trees)) DoClearSquare(tile);
		BuildObject(OBJECT_STATUE, tile, OWNER_NONE, t);
		/* BuildObject is void in this version; check the resulting tile instead of a return value. */
		if (!IsTileType(tile, TileType::Object) || !IsObjectType(tile, OBJECT_STATUE)) return false;
		built.push_back(tile);
	}
	return true;
}

/** Remove the objects that were built before a failure, so no half-finished building is left behind. */
static void UndoForkBlocks(const std::vector<TileIndex> &built)
{
	for (TileIndex tile : built) ForkClearObjectTile(tile);
}

/**
 * Build the building of an institution on the map.
 * @param a The institution.
 * @param t The town.
 * @param origin The northern tile of the place, found with #FindForkBuildingSite.
 */
void PlaceForkSchoolBuilding(ForkAsset *a, Town *t, TileIndex origin)
{
	std::vector<TileIndex> built;
	if (!BuildForkBlock(origin, t, built)) {
		UndoForkBlocks(built);
		return;
	}
	a->data2[3] = static_cast<int64_t>(origin.base()) + 1;
}

/**
 * Build the headquarters and the laboratory of a firm on two free 2x2 blocks near the middle of the town.
 * Nothing is built if there is no room; it is tried again later.
 * @param f The firm.
 */
void PlaceForkFirmBuildings(ForkFirm *f)
{
	if (f->hq_tile != 0 || f->lab_tile != 0) return;

	Town *t = Town::GetIfValid(f->GetTown());
	if (t == nullptr || !Object::CanAllocateItem(2 * 4)) return; // 4 tiles for the HQ and 4 for the laboratory.

	static const int offsets[12][2] = {
		{ 3,  0}, { 0,  3}, {-3,  0}, { 0, -3},
		{ 3,  3}, { 3, -3}, {-3,  3}, {-3, -3},
		{ 4,  0}, { 0,  4}, {-4,  0}, { 0, -4},
	};

	for (TileIndex a : SpiralTileSequence(t->xy, 2 * 20 + 1)) {
		if (!IsValidTile(a) || DistanceManhattan(a, t->xy) < 3) continue;

		TileIndex hq;
		if (!FindFreeBlock(TileX(a), TileY(a), hq)) continue;

		for (const auto &o : offsets) {
			const int x = static_cast<int>(TileX(hq)) + o[0];
			const int y = static_cast<int>(TileY(hq)) + o[1];
			if (x < 0 || y < 0) continue;
			TileIndex lab;
			if (!FindFreeBlock(static_cast<uint>(x), static_cast<uint>(y), lab)) continue;
			if (BlocksOverlap(hq, lab)) continue;

			std::vector<TileIndex> built;
			if (!BuildForkBlock(hq, t, built) || !BuildForkBlock(lab, t, built)) {
				UndoForkBlocks(built);
				return; // Leave both origins empty and let the monthly check try again.
			}

			f->hq_tile = hq.base();
			f->lab_tile = lab.base();
			return;
		}
	}
}

/** @return Whether all eight object tiles of both firm buildings are still there. */
bool AreForkFirmBuildingsPresent(const ForkFirm *f)
{
	TileIndex hq;
	TileIndex lab;
	if (!GetFirmOrigin(f, false, hq) || !GetFirmOrigin(f, true, lab)) return false;

	const auto block_intact = [](TileIndex origin) {
		const uint x = TileX(origin);
		const uint y = TileY(origin);
		if (x == 0 || y == 0 || x >= Map::SizeX() || y >= Map::SizeY() || x + 1 >= Map::SizeX() || y + 1 >= Map::SizeY()) return false;
		for (TileIndex tile : GetBlockTiles(origin)) {
			if (!IsValidTile(tile) || !IsTileType(tile, TileType::Object) || !IsObjectType(tile, OBJECT_STATUE)) return false;
		}
		return true;
	};

	if (!block_intact(hq) || !block_intact(lab)) return false;
	return !BlocksOverlap(hq, lab);
}

/**
 * Take the building of an institution off the map.
 * @param a The institution.
 */
void RemoveForkSchoolBuilding(ForkAsset *a)
{
	TileIndex origin;
	if (!GetOrigin(a, origin)) return;

	for (TileIndex tile : GetBlockTiles(origin)) ForkClearObjectTile(tile);
	a->data2[3] = 0;
}

/** Draw the school buildings again, for example because one moved on to the next stage. */
void RefreshForkSchoolBuilding(const ForkAsset *a)
{
	TileIndex origin;
	if (!GetOrigin(a, origin)) return;

	for (TileIndex tile : GetBlockTiles(origin)) MarkTileDirtyByTile(tile);
}

/** Old drawing hook: try the firm buildings, then the education buildings. */
bool DrawForkBuildingTile(const TileInfo *ti)
{
	return DrawForkFirmBuildingTile(ti) || DrawForkEducationBuildingTile(ti);
}

/** Old click hook: try the firm buildings, then the education buildings. */
bool ClickForkBuildingTile(TileIndex tile)
{
	return ClickForkFirmBuildingTile(tile) || ClickForkEducationBuildingTile(tile);
}

/** Old description hook: try the firm buildings, then the education buildings. */
bool GetForkBuildingTileDesc(TileIndex tile, TileDesc &td)
{
	return GetForkFirmBuildingTileDesc(tile, td) || GetForkEducationBuildingTileDesc(tile, td);
}
