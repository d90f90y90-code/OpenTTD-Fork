/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_realestate.cpp Real estate market: companies buy plots of land in towns, rent them out and sell them again. */

#include "stdafx.h"

#include "command_func.h"
#include "company_base.h"
#include "company_func.h"
#include "economy_func.h"
#include "fork_economy.h"
#include "fork_realestate_cmd.h"
#include "town.h"
#include "window_func.h"

#include "table/strings.h"

#include "safeguards.h"

static constexpr uint FORK_PLOT_MIN_POPULATION = 200; ///< A town needs this many inhabitants before land can be bought.
static constexpr uint FORK_PLOT_MAX_PER_TOWN = 30; ///< Never more plots than this per company in one town.
static constexpr uint FORK_PLOT_FEE_PERCENT = 5; ///< Fee of the broker when buying and selling.
static constexpr uint FORK_PLOT_RENT_PERMILLE = 12; ///< Monthly rent in permille of the value of a plot.

/**
 * Current market value of one plot of land in a town. It follows the size of the town, inflation and the economic cycle.
 * @param t Town to look at.
 * @return Value of a plot.
 */
Money GetForkPlotValue(const Town *t)
{
	Money per_inhabitant = _price[Price::BuildStationRail] / 50;
	return per_inhabitant * (t->cache.population + 300) * GetForkMarketFactor(ForkMarketFactor::Rent) / 100;
}

/**
 * Price to pay for a plot of land: its value plus the fee of the broker.
 * @param t Town to look at.
 * @return Price of a plot.
 */
Money GetForkPlotBuyPrice(const Town *t)
{
	Money value = GetForkPlotValue(t);
	return value + value * FORK_PLOT_FEE_PERCENT / 100;
}

/**
 * Money received for selling a plot of land: its value minus the fee of the broker.
 * @param t Town to look at.
 * @return Proceeds of selling a plot.
 */
Money GetForkPlotSellValue(const Town *t)
{
	Money value = GetForkPlotValue(t);
	return value - value * FORK_PLOT_FEE_PERCENT / 100;
}

/**
 * Rent a plot of land brings per month.
 * @param t Town to look at.
 * @return Rent in one month.
 */
Money GetForkPlotRent(const Town *t)
{
	return GetForkPlotValue(t) * FORK_PLOT_RENT_PERMILLE / 1000;
}

/**
 * How many plots a company may own in a town; bigger towns allow more.
 * @param t Town to look at.
 * @return Maximum number of plots.
 */
uint GetForkPlotLimit(const Town *t)
{
	return std::clamp<uint>(t->cache.population / 400, 1, FORK_PLOT_MAX_PER_TOWN);
}

/**
 * Count the plots a company owns in a town.
 * @param company Company to look at.
 * @param town Town to look in.
 * @return Number of plots.
 */
uint GetForkCompanyPlots(CompanyID company, TownID town)
{
	uint count = 0;
	for (const ForkAsset *a : ForkAsset::Iterate()) {
		if (a->owner == company && a->type == static_cast<uint8_t>(ForkAssetType::Property) && a->location == town.base()) count++;
	}
	return count;
}

/**
 * Buy a plot of land in a town.
 * @param flags Type of operation.
 * @param town_id Town to buy in.
 * @return The cost of this operation or an error.
 */
CommandCost CmdBuyForkPlot(DoCommandFlags flags, TownID town_id)
{
	if (!Company::IsValidID(_current_company)) return CMD_ERROR;
	Town *t = Town::GetIfValid(town_id);
	if (t == nullptr) return CMD_ERROR;

	if (t->cache.population < FORK_PLOT_MIN_POPULATION) return CommandCost(STR_ERROR_FORK_TOWN_TOO_SMALL);
	if (GetForkCompanyPlots(_current_company, town_id) >= GetForkPlotLimit(t)) return CommandCost(STR_ERROR_FORK_PLOT_LIMIT);
	if (!ForkAsset::CanAllocateItem()) return CommandCost(STR_ERROR_FORK_TOO_MANY_ASSETS);

	CommandCost cost(ExpensesType::Other, GetForkPlotBuyPrice(t));

	if (flags.Test(DoCommandFlag::Execute)) {
		ForkAsset *a = ForkAsset::Create();
		a->owner = _current_company;
		a->type = static_cast<uint8_t>(ForkAssetType::Property);
		a->location_kind = static_cast<uint8_t>(ForkAssetLocation::Town);
		a->location = town_id.base();
		a->purchase_price = GetForkPlotBuyPrice(t);
		a->value = GetForkPlotValue(t);
		a->monthly_income = GetForkPlotRent(t);
		SetWindowDirty(WindowClass::TownView, town_id.base());
		InvalidateWindowData(WindowClass::ForkBusiness, 0);
	}
	return cost;
}

/**
 * Sell the oldest plot of land a company owns in a town.
 * @param flags Type of operation.
 * @param town_id Town to sell in.
 * @return The cost (negative: proceeds) of this operation or an error.
 */
CommandCost CmdSellForkPlot(DoCommandFlags flags, TownID town_id)
{
	if (!Company::IsValidID(_current_company)) return CMD_ERROR;
	Town *t = Town::GetIfValid(town_id);
	if (t == nullptr) return CMD_ERROR;

	ForkAsset *oldest = nullptr;
	for (ForkAsset *a : ForkAsset::Iterate()) {
		if (a->owner != _current_company || a->type != static_cast<uint8_t>(ForkAssetType::Property) || a->location != town_id.base()) continue;
		if (oldest == nullptr || a->age_months > oldest->age_months) oldest = a;
	}
	if (oldest == nullptr) return CommandCost(STR_ERROR_FORK_NO_PLOT);

	CommandCost cost(ExpensesType::Other, -GetForkPlotSellValue(t));

	if (flags.Test(DoCommandFlag::Execute)) {
		delete oldest;
		SetWindowDirty(WindowClass::TownView, town_id.base());
		InvalidateWindowData(WindowClass::ForkBusiness, 0);
	}
	return cost;
}

/** Every month a plot is revalued and pays its rent. */
static void ForkPropertyMonthly(ForkAsset *a)
{
	const Town *t = Town::GetIfValid(static_cast<TownID>(a->location));
	if (t == nullptr) {
		a->value = 0;
		a->monthly_income = 0;
		return;
	}

	a->value = GetForkPlotValue(t);
	a->monthly_income = GetForkPlotRent(t);
	ForkLedgerBook(a->owner, ForkLedgerCategory::Rent, a->monthly_income);
}

/** Make the monthly update of plots known to the extended economy. */
void RegisterForkRealEstate()
{
	RegisterForkAssetHandler(ForkAssetType::Property, ForkPropertyMonthly);
}
