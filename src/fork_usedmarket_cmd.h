/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_usedmarket_cmd.h Command definitions of the used vehicle market. */

#ifndef FORK_USEDMARKET_CMD_H
#define FORK_USEDMARKET_CMD_H

#include "command_type.h"
#include "map_type.h"

CommandCost CmdBuyUsedVehicle(DoCommandFlags flags, TileIndex tile, uint8_t dealer, uint8_t slot);

DEF_CMD_TRAIT(Commands::BuyUsedVehicle, CmdBuyUsedVehicle, {}, CommandType::VehicleConstruction)

#endif /* FORK_USEDMARKET_CMD_H */
