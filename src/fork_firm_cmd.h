/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_firm_cmd.h Command definitions of the firms. */

#ifndef FORK_FIRM_CMD_H
#define FORK_FIRM_CMD_H

#include "command_type.h"
#include "town_type.h"

CommandCost CmdCreateForkFirm(DoCommandFlags flags, TownID town_id, uint8_t product, const std::string &name);
CommandCost CmdSetForkFirm(DoCommandFlags flags, uint32_t firm_index, uint8_t what, uint8_t value);
CommandCost CmdDumpForkFirmStock(DoCommandFlags flags, uint32_t firm_index);

DEF_CMD_TRAIT(Commands::CreateForkFirm, CmdCreateForkFirm, {}, CommandType::MoneyManagement)
DEF_CMD_TRAIT(Commands::SetForkFirm, CmdSetForkFirm, {}, CommandType::OtherManagement)
DEF_CMD_TRAIT(Commands::DumpForkFirmStock, CmdDumpForkFirmStock, {}, CommandType::OtherManagement)

#endif /* FORK_FIRM_CMD_H */
