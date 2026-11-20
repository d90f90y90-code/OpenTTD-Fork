/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_buildings.h Buildings of the fork's economy that stand on the map: company headquarters/laboratories and schools/universities. */

#ifndef FORK_BUILDINGS_H
#define FORK_BUILDINGS_H

#include "tile_cmd.h"
#include "tile_type.h"

struct ForkAsset;
struct ForkFirm;
struct Town;

/** Firm buildings: the glass headquarters and the white laboratory, each on a 2x2 block of object tiles. */
void PlaceForkFirmBuildings(ForkFirm *f);
bool AreForkFirmBuildingsPresent(const ForkFirm *f);
bool DrawForkFirmBuildingTile(const TileInfo *ti);
bool ClickForkFirmBuildingTile(TileIndex tile);
bool GetForkFirmBuildingTileDesc(TileIndex tile, TileDesc &td);

/** Education buildings: schools and universities, each on a 2x2 block of object tiles. */
bool DrawForkEducationBuildingTile(const TileInfo *ti);
bool ClickForkEducationBuildingTile(TileIndex tile);
bool GetForkEducationBuildingTileDesc(TileIndex tile, TileDesc &td);

bool FindForkBuildingSite(const Town *t, TileIndex &origin);
void PlaceForkSchoolBuilding(ForkAsset *a, Town *t, TileIndex origin);
void RemoveForkSchoolBuilding(ForkAsset *a);
void RefreshForkSchoolBuilding(const ForkAsset *a);
uint GetForkSchoolStage(const ForkAsset *a);

/**
 * Old names, kept as thin wrappers for callers that have not been updated yet.
 * They try the firm buildings first, then the education buildings.
 */
bool DrawForkBuildingTile(const TileInfo *ti);
bool ClickForkBuildingTile(TileIndex tile);
bool GetForkBuildingTileDesc(TileIndex tile, TileDesc &td);

/** Remove an object from the map without any checks, defined in object_cmd.cpp. */
void ForkClearObjectTile(TileIndex tile);

#endif /* FORK_BUILDINGS_H */
