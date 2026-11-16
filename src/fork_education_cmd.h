/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_education_cmd.h Command definitions of the private schools and universities. */

#ifndef FORK_EDUCATION_CMD_H
#define FORK_EDUCATION_CMD_H

#include "command_type.h"
#include "town_type.h"

CommandCost CmdBuildForkSchool(DoCommandFlags flags, TownID town_id, uint8_t kind);
CommandCost CmdSellForkSchool(DoCommandFlags flags, TownID town_id, uint8_t kind);
CommandCost CmdForkSchoolAction(DoCommandFlags flags, TownID town_id, uint8_t kind, uint8_t action, uint16_t argument);
CommandCost CmdRenameForkSchool(DoCommandFlags flags, TownID town_id, uint8_t kind, const std::string &text);

DEF_CMD_TRAIT(Commands::BuildForkSchool, CmdBuildForkSchool, {}, CommandType::MoneyManagement)
DEF_CMD_TRAIT(Commands::SellForkSchool, CmdSellForkSchool, {}, CommandType::MoneyManagement)
DEF_CMD_TRAIT(Commands::ForkSchoolAction, CmdForkSchoolAction, {}, CommandType::MoneyManagement)
DEF_CMD_TRAIT(Commands::RenameForkSchool, CmdRenameForkSchool, {}, CommandType::OtherManagement)

#endif /* FORK_EDUCATION_CMD_H */
