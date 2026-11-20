/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_products_cmd.h Command definitions of the user-created products. */

#ifndef FORK_PRODUCTS_CMD_H
#define FORK_PRODUCTS_CMD_H

#include "command_type.h"
#include "company_type.h"

#include <string>

CommandCost CmdCreateForkProduct(DoCommandFlags flags, uint16_t catalog_id, uint32_t quantity, uint32_t price, const std::string &name);
CommandCost CmdSetForkProduct(DoCommandFlags flags, uint32_t product_index, uint8_t what, uint32_t value);
CommandCost CmdRenameForkProduct(DoCommandFlags flags, uint32_t product_index, const std::string &name);
CommandCost CmdSellForkCompany(DoCommandFlags flags, CompanyID company);

DEF_CMD_TRAIT(Commands::CreateForkProduct, CmdCreateForkProduct, {}, CommandType::MoneyManagement)
DEF_CMD_TRAIT(Commands::SetForkProduct, CmdSetForkProduct, {}, CommandType::MoneyManagement)
DEF_CMD_TRAIT(Commands::RenameForkProduct, CmdRenameForkProduct, {}, CommandType::OtherManagement)
DEF_CMD_TRAIT(Commands::SellForkCompany, CmdSellForkCompany, {}, CommandType::MoneyManagement)

#endif /* FORK_PRODUCTS_CMD_H */
