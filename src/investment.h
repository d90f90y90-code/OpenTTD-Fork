/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file investment.h Industry investments: buying and selling stakes in industries. */

#ifndef INVESTMENT_H
#define INVESTMENT_H

#include "company_type.h"
#include "economy_type.h"

struct Industry;

static constexpr uint8_t INVESTMENT_STEP = 10;       ///< Stakes are traded in steps of this many percent.
static constexpr uint8_t INVESTMENT_MAX_STAKE = 50;  ///< A single company can own at most this many percent of an industry.
static constexpr uint INVESTMENT_REFERENCE_DISTANCE = 20; ///< Distance (in tiles) used to value the cargo of an industry.
static constexpr int INVESTMENT_VALUATION_MONTHS = 36; ///< An industry is worth this many months of its production value.

Money GetIndustryOutputValue(const Industry *i, bool transported_only);
Money GetIndustryStakeBuyPrice(const Industry *i, uint8_t percent);
Money GetIndustryStakeSellValue(const Industry *i, uint8_t percent);
uint GetIndustryTotalStake(const Industry *i);
void PayIndustryDividends(const Industry *i);
void TransferIndustryInvestments(Owner old_owner, Owner new_owner);

#endif /* INVESTMENT_H */
