/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_stock_cmd.h Command definitions of the stock exchange. */

#ifndef FORK_STOCK_CMD_H
#define FORK_STOCK_CMD_H

#include "command_type.h"

CommandCost CmdBuyForkStock(DoCommandFlags flags, uint8_t listed, uint8_t count);
CommandCost CmdSellForkStock(DoCommandFlags flags, uint8_t listed, uint8_t count);
CommandCost CmdBuyForkFirmShare(DoCommandFlags flags, uint32_t firm_index, uint8_t percent);
CommandCost CmdSellForkFirmShare(DoCommandFlags flags, uint32_t firm_index, uint8_t percent);
CommandCost CmdTakeOverForkFirm(DoCommandFlags flags, uint32_t firm_index);

DEF_CMD_TRAIT(Commands::BuyForkStock, CmdBuyForkStock, {}, CommandType::MoneyManagement)
DEF_CMD_TRAIT(Commands::SellForkStock, CmdSellForkStock, {}, CommandType::MoneyManagement)
DEF_CMD_TRAIT(Commands::BuyForkFirmShare, CmdBuyForkFirmShare, {}, CommandType::MoneyManagement)
DEF_CMD_TRAIT(Commands::SellForkFirmShare, CmdSellForkFirmShare, {}, CommandType::MoneyManagement)
DEF_CMD_TRAIT(Commands::TakeOverForkFirm, CmdTakeOverForkFirm, {}, CommandType::MoneyManagement)

#endif /* FORK_STOCK_CMD_H */
