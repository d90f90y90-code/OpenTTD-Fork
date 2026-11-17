/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_stock.h Stock exchange of the fork's economy: shares of listed companies and of the firms of the towns. */

#ifndef FORK_STOCK_H
#define FORK_STOCK_H

#include "company_type.h"
#include "economy_type.h"

static constexpr size_t FORK_LISTED_COUNT = 12; ///< Number of companies that are listed on the stock exchange.
static constexpr uint FORK_FIRM_MAX_STAKE = 50; ///< All investors together can hold at most this many percent of a firm before a takeover.

Money GetForkStockPrice(uint index);
int GetForkStockChangePermille(uint index);
uint GetForkStockYieldTenths(uint index);
uint GetForkStockHeld(CompanyID company, uint index);
Money GetForkStockCost(CompanyID company, uint index);

struct ForkFirm;
Money GetForkFirmShareValue(const ForkFirm *f);
Money GetForkFirmTakeoverPrice(const ForkFirm *f);
uint GetForkFirmStake(CompanyID company, uint32_t firm);
uint GetForkFirmStakeTotal(uint32_t firm);

void ForkStocksMonthly();
void RegisterForkStocks();
void ShowForkExchangeWindow();

#endif /* FORK_STOCK_H */
