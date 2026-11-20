/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_rights_cmd.h Command definitions of the rights and licenses. */

#ifndef FORK_RIGHTS_CMD_H
#define FORK_RIGHTS_CMD_H

#include "command_type.h"
#include "town_type.h"

CommandCost CmdBuyForkRight(DoCommandFlags flags, TownID town_id, uint8_t category);
CommandCost CmdSignForkLicense(DoCommandFlags flags, TownID town_id, uint8_t category, uint8_t years);

DEF_CMD_TRAIT(Commands::BuyForkRight, CmdBuyForkRight, {}, CommandType::MoneyManagement)
DEF_CMD_TRAIT(Commands::SignForkLicense, CmdSignForkLicense, {}, CommandType::MoneyManagement)

#endif /* FORK_RIGHTS_CMD_H */
