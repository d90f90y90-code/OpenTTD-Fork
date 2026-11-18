/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_buildings.h The buildings of the firms in the towns: a glass headquarters and a white laboratory. */

#ifndef FORK_BUILDINGS_H
#define FORK_BUILDINGS_H

#include "tile_cmd.h"
#include "tile_type.h"

struct ForkFirm;

void PlaceForkFirmBuildings(ForkFirm *f);
bool DrawForkBuildingTile(const TileInfo *ti);
bool ClickForkBuildingTile(TileIndex tile);
bool GetForkBuildingTileDesc(TileIndex tile, TileDesc &td);

#endif /* FORK_BUILDINGS_H */
