/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_labor_cmd.h Command definitions of the labor market. */

#ifndef FORK_LABOR_CMD_H
#define FORK_LABOR_CMD_H

#include "command_type.h"
#include "town_type.h"

CommandCost CmdHireForkStaff(DoCommandFlags flags, TownID town_id, uint8_t count);
CommandCost CmdFireForkStaff(DoCommandFlags flags, TownID town_id, uint8_t count);

DEF_CMD_TRAIT(Commands::HireForkStaff, CmdHireForkStaff, {}, CommandType::MoneyManagement)
DEF_CMD_TRAIT(Commands::FireForkStaff, CmdFireForkStaff, {}, CommandType::MoneyManagement)

#endif /* FORK_LABOR_CMD_H */
