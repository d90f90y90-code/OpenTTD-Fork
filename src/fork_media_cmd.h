/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_media_cmd.h Command definitions of the television and radio channels. */

#ifndef FORK_MEDIA_CMD_H
#define FORK_MEDIA_CMD_H

#include "command_type.h"
#include "fork_economy.h"
#include "town_type.h"

CommandCost CmdBuildForkChannel(DoCommandFlags flags, TownID town_id, uint8_t kind, uint16_t package, const std::string &name);
CommandCost CmdSetForkChannelPackage(DoCommandFlags flags, ForkAssetID asset_id, uint16_t package);

DEF_CMD_TRAIT(Commands::BuildForkChannel, CmdBuildForkChannel, {}, CommandType::OtherManagement)
DEF_CMD_TRAIT(Commands::SetForkChannelPackage, CmdSetForkChannelPackage, {}, CommandType::OtherManagement)

#endif /* FORK_MEDIA_CMD_H */
