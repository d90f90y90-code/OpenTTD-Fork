/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_buildings.h Buildings of the fork's economy that stand on the map: schools and universities. */

#ifndef FORK_BUILDINGS_H
#define FORK_BUILDINGS_H

#include "tile_cmd.h"
#include "tile_type.h"

struct ForkAsset;
struct Town;

bool DrawForkBuildingTile(const TileInfo *ti);
bool ClickForkBuildingTile(TileIndex tile);
bool GetForkBuildingTileDesc(TileIndex tile, TileDesc &td);

bool FindForkBuildingSite(const Town *t, TileIndex &origin);
void PlaceForkSchoolBuilding(ForkAsset *a, Town *t, TileIndex origin);
void RemoveForkSchoolBuilding(ForkAsset *a);
void RefreshForkSchoolBuilding(const ForkAsset *a);
uint GetForkSchoolStage(const ForkAsset *a);

/** Remove an object from the map without any checks, defined in object_cmd.cpp. */
void ForkClearObjectTile(TileIndex tile);

#endif /* FORK_BUILDINGS_H */
