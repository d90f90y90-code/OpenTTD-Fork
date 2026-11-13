/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file investment.cpp Industry investments: buying and selling stakes in industries, and paying dividends. */

#include "stdafx.h"

#include "cargotype.h"
#include "command_func.h"
#include "company_base.h"
#include "company_func.h"
#include "economy_func.h"
#include "industry.h"
#include "investment.h"
#include "investment_cmd.h"
#include "window_func.h"

#include "table/strings.h"

#include "safeguards.h"

/**
 * Get the value of the cargo an industry produced last month.
 * @param i Industry to look at.
 * @param transported_only Only count the cargo that was actually transported away.
 * @return Value of that cargo if it were moved over the reference distance.
 */
Money GetIndustryOutputValue(const Industry *i, bool transported_only)
{
	Money total = 0;
	for (const auto &p : i->produced) {
		if (!IsValidCargoType(p.cargo)) continue;
		const auto &h = p.history[LAST_MONTH];
		uint pieces = transported_only ? h.transported : h.production;
		total += GetTransportedGoodsIncome(pieces, INVESTMENT_REFERENCE_DISTANCE, 0, p.cargo);
	}
	return total;
}

/**
 * Total percentage of an industry that is owned by companies.
 * @param i Industry to look at.
 * @return Owned percentage (0..100).
 */
uint GetIndustryTotalStake(const Industry *i)
{
	uint total = 0;
	for (uint8_t s : i->investor_stakes) total += s;
	return total;
}

/**
 * Valuation of the part of an industry, without any fees.
 * @param i Industry to value.
 * @param percent Percentage of the industry.
 * @return Value of that part.
 */
static Money GetIndustryStakeValue(const Industry *i, uint8_t percent)
{
	Money valuation = GetIndustryOutputValue(i, false) * INVESTMENT_VALUATION_MONTHS;
	return valuation * percent / 100;
}

/**
 * Price to pay for buying a part of an industry (value plus a 5% broker fee).
 * @param i Industry to buy in.
 * @param percent Percentage to buy.
 * @return Price.
 */
Money GetIndustryStakeBuyPrice(const Industry *i, uint8_t percent)
{
	Money value = GetIndustryStakeValue(i, percent);
	return value + value / 20;
}

/**
 * Money received for selling a part of an industry (value minus a 5% broker fee).
 * @param i Industry to sell in.
 * @param percent Percentage to sell.
 * @return Proceeds.
 */
Money GetIndustryStakeSellValue(const Industry *i, uint8_t percent)
{
	Money value = GetIndustryStakeValue(i, percent);
	return value - value / 20;
}

/**
 * Buy a stake in an industry.
 * @param flags Type of operation.
 * @param ind_id Industry to invest in.
 * @param percent Percentage to buy, a multiple of #INVESTMENT_STEP.
 * @return The cost of this operation or an error.
 */
CommandCost CmdInvestInIndustry(DoCommandFlags flags, IndustryID ind_id, uint8_t percent)
{
	if (!Company::IsValidID(_current_company)) return CMD_ERROR;
	Industry *i = Industry::GetIfValid(ind_id);
	if (i == nullptr) return CMD_ERROR;
	if (percent == 0 || percent % INVESTMENT_STEP != 0) return CMD_ERROR;

	if (i->prod_level == PRODLEVEL_CLOSURE || GetIndustryOutputValue(i, false) <= 0) return CommandCost(STR_ERROR_INDUSTRY_NO_PRODUCTION_VALUE);

	uint8_t &stake = i->investor_stakes[_current_company.base()];
	if (stake + percent > INVESTMENT_MAX_STAKE) return CommandCost(STR_ERROR_INDUSTRY_STAKE_LIMIT);
	if (GetIndustryTotalStake(i) + percent > 100) return CommandCost(STR_ERROR_INDUSTRY_NO_FREE_SHARES);

	CommandCost cost(ExpensesType::Other, GetIndustryStakeBuyPrice(i, percent));

	if (flags.Test(DoCommandFlag::Execute)) {
		stake += percent;
		SetWindowDirty(WindowClass::IndustryView, i->index);
	}
	return cost;
}

/**
 * Sell a stake in an industry.
 * @param flags Type of operation.
 * @param ind_id Industry to sell a stake in.
 * @param percent Percentage to sell, a multiple of #INVESTMENT_STEP.
 * @return The cost (negative: proceeds) of this operation or an error.
 */
CommandCost CmdSellIndustryStake(DoCommandFlags flags, IndustryID ind_id, uint8_t percent)
{
	if (!Company::IsValidID(_current_company)) return CMD_ERROR;
	Industry *i = Industry::GetIfValid(ind_id);
	if (i == nullptr) return CMD_ERROR;
	if (percent == 0 || percent % INVESTMENT_STEP != 0) return CMD_ERROR;

	uint8_t &stake = i->investor_stakes[_current_company.base()];
	if (stake < percent) return CommandCost(STR_ERROR_INDUSTRY_NOT_ENOUGH_STAKE);

	CommandCost cost(ExpensesType::Other, -GetIndustryStakeSellValue(i, percent));

	if (flags.Test(DoCommandFlag::Execute)) {
		stake -= percent;
		SetWindowDirty(WindowClass::IndustryView, i->index);
	}
	return cost;
}

/**
 * Pay the monthly dividends of an industry to its investors.
 * The dividend is the investor's share of the value of the cargo that was transported away last month.
 * @param i Industry that pays.
 */
void PayIndustryDividends(const Industry *i)
{
	if (GetIndustryTotalStake(i) == 0) return;

	Money transported = GetIndustryOutputValue(i, true);
	if (transported <= 0) return;

	for (const Company *c : Company::Iterate()) {
		uint8_t stake = i->investor_stakes[c->index.base()];
		if (stake == 0) continue;
		SubtractMoneyFromCompany(c->index, CommandCost(ExpensesType::Other, -(transported * stake / 100)));
	}
}

/**
 * Move all stakes owned by a company to another company, or remove them.
 * @param old_owner Company that loses its stakes.
 * @param new_owner Company that receives them, or an invalid owner to just remove them.
 */
void TransferIndustryInvestments(Owner old_owner, Owner new_owner)
{
	if (!Company::IsValidID(old_owner)) return;

	for (Industry *i : Industry::Iterate()) {
		uint8_t &old_stake = i->investor_stakes[old_owner.base()];
		if (old_stake == 0) continue;

		if (Company::IsValidID(new_owner) && new_owner != old_owner) {
			uint8_t &new_stake = i->investor_stakes[new_owner.base()];
			new_stake = static_cast<uint8_t>(std::min<uint>(100, new_stake + old_stake));
		}
		old_stake = 0;
	}
}
