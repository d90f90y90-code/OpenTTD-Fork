/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file investment_cmd.h Command definitions related to industry investments. */

#ifndef INVESTMENT_CMD_H
#define INVESTMENT_CMD_H

#include "command_type.h"
#include "industry_type.h"

CommandCost CmdInvestInIndustry(DoCommandFlags flags, IndustryID ind_id, uint8_t percent);
CommandCost CmdSellIndustryStake(DoCommandFlags flags, IndustryID ind_id, uint8_t percent);

DEF_CMD_TRAIT(Commands::InvestInIndustry, CmdInvestInIndustry, {}, CommandType::MoneyManagement)
DEF_CMD_TRAIT(Commands::SellIndustryStake, CmdSellIndustryStake, {}, CommandType::MoneyManagement)

#endif /* INVESTMENT_CMD_H */
