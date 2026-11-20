/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_realestate.cpp Real estate market: companies buy houses in towns, rent them out, set the rent and sell them again. */

#include "stdafx.h"

#include "command_func.h"
#include "company_base.h"
#include "company_func.h"
#include "economy_func.h"
#include "fork_economy.h"
#include "fork_realestate_cmd.h"
#include "town.h"
#include "timer/timer_game_economy.h"
#include "window_func.h"

#include "table/strings.h"

#include "safeguards.h"

static constexpr uint FORK_HOUSE_MIN_POPULATION = 100; ///< A town needs this many inhabitants before houses can be bought.
static constexpr uint FORK_HOUSE_MAX_PER_TOWN = 100; ///< Never more houses than this per company in one town.
static constexpr uint FORK_HOUSE_FEE_PERCENT = 5; ///< Fee of the broker when buying and selling.
static constexpr uint FORK_HOUSE_RENT_PERMILLE = 15; ///< Monthly rent in permille of the value of a house, at the market rent and full occupancy.
static constexpr uint FORK_RENT_MIN = 50; ///< Lowest rent a company can ask, in percent of the market rent.
static constexpr uint FORK_RENT_MAX = 250; ///< Highest rent a company can ask, in percent of the market rent.
static constexpr uint FORK_RENT_STEP = 10; ///< Rents are changed in steps of this many percent.
static constexpr uint FORK_RENT_DEFAULT = 100; ///< Rent that is asked when nothing else was set.

/** @return Number of houses an asset stands for. Older assets have no count and are a single house. */
static uint AssetHouses(const ForkAsset *a)
{
	return a->data[2] > 0 ? static_cast<uint>(a->data[2]) : 1;
}

/** @return Rent a company asks for an asset, in percent of the market rent. */
static uint AssetRentPercent(const ForkAsset *a)
{
	return a->data[0] > 0 ? static_cast<uint>(a->data[0]) : FORK_RENT_DEFAULT;
}

/** @return Whether this asset is a house of the company in the town. */
static bool IsHouseOf(const ForkAsset *a, CompanyID company, TownID town)
{
	return a->owner == company && a->type == static_cast<uint8_t>(ForkAssetType::Property) && a->location == town.base();
}

/**
 * The property market moves up and down from month to month, on top of the economic cycle.
 * It is derived from the date, so every player sees the same movement and nothing needs to be saved.
 * @return Movement of this month in permille, from -80 to +80.
 */
int GetForkPropertySwing()
{
	uint32_t m = static_cast<uint32_t>(TimerGameEconomy::year.base()) * 12 + TimerGameEconomy::month;
	uint32_t h = m * 2654435761U;
	h ^= h >> 15;
	h *= 2246822519U;
	h ^= h >> 13;
	return static_cast<int>(h % 161) - 80;
}

/**
 * Current market value of one house in a town. It follows the size of the town, inflation, the economic cycle and the monthly swing.
 * @param t Town to look at.
 * @return Value of a house.
 */
Money GetForkHouseValue(const Town *t)
{
	Money value = _price[Price::BuildStationRail] * (50 + GetForkTownSizeLevel(t) * 3 / 2);
	value = value * GetForkMarketFactor(ForkMarketFactor::Rent) / 100;
	return value * (1000 + GetForkPropertySwing() / 2) / 1000;
}

/**
 * Price to pay for a house: its value plus the fee of the broker.
 * @param t Town to look at.
 * @return Price of a house.
 */
Money GetForkHouseBuyPrice(const Town *t)
{
	Money value = GetForkHouseValue(t);
	return value + value * FORK_HOUSE_FEE_PERCENT / 100;
}

/**
 * Money received for selling a house: its value minus the fee of the broker.
 * @param t Town to look at.
 * @return Proceeds of selling a house.
 */
Money GetForkHouseSellValue(const Town *t)
{
	Money value = GetForkHouseValue(t);
	return value - value * FORK_HOUSE_FEE_PERCENT / 100;
}

/**
 * How many houses a company may own in a town: not more than the town has, and never more than 100.
 * @param t Town to look at.
 * @return Maximum number of houses.
 */
uint GetForkHouseLimit(const Town *t)
{
	return std::clamp<uint>(t->cache.num_houses, 5, FORK_HOUSE_MAX_PER_TOWN);
}

/**
 * Count the houses a company owns in a town.
 * @param company Company to look at.
 * @param town Town to look in.
 * @return Number of houses.
 */
uint GetForkCompanyHouses(CompanyID company, TownID town)
{
	uint count = 0;
	for (const ForkAsset *a : ForkAsset::Iterate()) {
		if (IsHouseOf(a, company, town)) count += AssetHouses(a);
	}
	return count;
}

/**
 * Rent a company asks in a town, in percent of the market rent.
 * @param company Company to look at.
 * @param town Town to look in.
 * @return Rent in percent; the default when the company owns no houses there.
 */
uint GetForkRentPercent(CompanyID company, TownID town)
{
	for (const ForkAsset *a : ForkAsset::Iterate()) {
		if (IsHouseOf(a, company, town)) return AssetRentPercent(a);
	}
	return FORK_RENT_DEFAULT;
}

/**
 * Share of the houses that is rented out. A lower rent fills the houses, a higher rent drives tenants away.
 * Good times and the monthly swing of the property market change it a little.
 * @param rent_percent Rent that is asked, in percent of the market rent.
 * @return Occupancy in percent.
 */
uint GetForkOccupancy(uint rent_percent)
{
	int occupancy = 95;
	occupancy += std::clamp((static_cast<int>(GetForkMarketFactor(ForkMarketFactor::Rent)) - 100) / 2, -10, 10);
	occupancy += GetForkPropertySwing() / 20;
	if (rent_percent > FORK_RENT_DEFAULT) {
		occupancy -= static_cast<int>(rent_percent - FORK_RENT_DEFAULT) / 2;
	} else {
		occupancy += static_cast<int>(FORK_RENT_DEFAULT - rent_percent) / 4;
	}
	return static_cast<uint>(std::clamp(occupancy, 20, 100));
}

/**
 * Rent the market pays for a house per month at the asked rent, with the occupancy and the monthly swing taken into account.
 * @param t Town to look at.
 * @param rent_percent Rent that is asked, in percent of the market rent.
 * @return Rent income of one house in one month.
 */
Money GetForkHouseRent(const Town *t, uint rent_percent)
{
	Money rent = GetForkHouseValue(t) * FORK_HOUSE_RENT_PERMILLE / 1000;
	rent = rent * rent_percent / 100;
	rent = rent * GetForkOccupancy(rent_percent) / 100;
	return rent * (1000 + GetForkPropertySwing()) / 1000;
}

/** Bring value and income of an asset up to date. */
static void RefreshForkProperty(ForkAsset *a, const Town *t)
{
	uint houses = AssetHouses(a);
	uint rent_percent = AssetRentPercent(a);
	a->data[0] = rent_percent;
	a->data[2] = houses;
	a->data[1] = GetForkOccupancy(rent_percent);
	a->value = GetForkHouseValue(t) * houses;
	/* Houses that are not looked after well earn less rent; the staff of the labor market decides how well. */
	a->monthly_income = GetForkHouseRent(t, rent_percent) * houses * GetForkStaffEfficiency(a->owner, t->index) / 100;
}

/**
 * Buy houses in a town.
 * @param flags Type of operation.
 * @param town_id Town to buy in.
 * @param count Number of houses.
 * @return The cost of this operation or an error.
 */
CommandCost CmdBuyForkHouses(DoCommandFlags flags, TownID town_id, uint8_t count)
{
	if (!Company::IsValidID(_current_company) || count == 0 || count > FORK_HOUSE_MAX_PER_TOWN) return CMD_ERROR;
	Town *t = Town::GetIfValid(town_id);
	if (t == nullptr) return CMD_ERROR;

	if (t->cache.population < FORK_HOUSE_MIN_POPULATION) return CommandCost(STR_ERROR_FORK_TOWN_TOO_SMALL);
	if (GetForkCompanyHouses(_current_company, town_id) + count > GetForkHouseLimit(t)) return CommandCost(STR_ERROR_FORK_HOUSE_LIMIT);

	ForkAsset *existing = nullptr;
	for (ForkAsset *a : ForkAsset::Iterate()) {
		if (IsHouseOf(a, _current_company, town_id)) {
			existing = a;
			break;
		}
	}
	if (existing == nullptr && !ForkAsset::CanAllocateItem()) return CommandCost(STR_ERROR_FORK_TOO_MANY_ASSETS);

	Money price = GetForkHouseBuyPrice(t) * count;
	CommandCost cost(ExpensesType::Other, price);

	if (flags.Test(DoCommandFlag::Execute)) {
		ForkAsset *a = existing;
		if (a == nullptr) {
			a = ForkAsset::Create();
			a->owner = _current_company;
			a->type = static_cast<uint8_t>(ForkAssetType::Property);
			a->location_kind = static_cast<uint8_t>(ForkAssetLocation::Town);
			a->location = town_id.base();
			a->data[0] = FORK_RENT_DEFAULT;
			a->data[2] = 0;
		}
		a->data[2] = (existing != nullptr ? AssetHouses(a) : 0) + count;
		a->purchase_price += price;
		RefreshForkProperty(a, t);
		SetWindowClassesDirty(WindowClass::ForkRealEstate);
		InvalidateWindowData(WindowClass::ForkBusiness, 0);
	}
	return cost;
}

/**
 * Sell houses in a town, the ones that are owned the longest first.
 * @param flags Type of operation.
 * @param town_id Town to sell in.
 * @param count Number of houses.
 * @return The cost (negative: proceeds) of this operation or an error.
 */
CommandCost CmdSellForkHouses(DoCommandFlags flags, TownID town_id, uint8_t count)
{
	if (!Company::IsValidID(_current_company) || count == 0 || count > FORK_HOUSE_MAX_PER_TOWN) return CMD_ERROR;
	Town *t = Town::GetIfValid(town_id);
	if (t == nullptr) return CMD_ERROR;

	if (GetForkCompanyHouses(_current_company, town_id) < count) return CommandCost(STR_ERROR_FORK_NO_HOUSES);

	CommandCost cost(ExpensesType::Other, -(GetForkHouseSellValue(t) * count));

	if (flags.Test(DoCommandFlag::Execute)) {
		std::vector<ForkAsset *> owned;
		for (ForkAsset *a : ForkAsset::Iterate()) {
			if (IsHouseOf(a, _current_company, town_id)) owned.push_back(a);
		}
		std::sort(owned.begin(), owned.end(), [](const ForkAsset *a, const ForkAsset *b) { return a->age_months > b->age_months; });

		uint remaining = count;
		for (ForkAsset *a : owned) {
			if (remaining == 0) break;
			uint houses = AssetHouses(a);
			if (houses <= remaining) {
				remaining -= houses;
				delete a;
			} else {
				a->purchase_price = a->purchase_price * (houses - remaining) / houses;
				a->data[2] = houses - remaining;
				remaining = 0;
				RefreshForkProperty(a, t);
			}
		}
		SetWindowClassesDirty(WindowClass::ForkRealEstate);
		InvalidateWindowData(WindowClass::ForkBusiness, 0);
	}
	return cost;
}

/**
 * Change the rent a company asks for its houses in a town.
 * @param flags Type of operation.
 * @param town_id Town of the houses.
 * @param percent New rent in percent of the market rent.
 * @return The cost of this operation (none) or an error.
 */
CommandCost CmdSetForkRent(DoCommandFlags flags, TownID town_id, uint16_t percent)
{
	if (!Company::IsValidID(_current_company)) return CMD_ERROR;
	Town *t = Town::GetIfValid(town_id);
	if (t == nullptr) return CMD_ERROR;
	if (percent < FORK_RENT_MIN || percent > FORK_RENT_MAX || percent % FORK_RENT_STEP != 0) return CMD_ERROR;

	if (GetForkCompanyHouses(_current_company, town_id) == 0) return CommandCost(STR_ERROR_FORK_NO_HOUSES);

	if (flags.Test(DoCommandFlag::Execute)) {
		for (ForkAsset *a : ForkAsset::Iterate()) {
			if (!IsHouseOf(a, _current_company, town_id)) continue;
			a->data[0] = percent;
			RefreshForkProperty(a, t);
		}
		SetWindowClassesDirty(WindowClass::ForkRealEstate);
	}
	return CommandCost();
}

/** Every month the houses are revalued and pay their rent. */
static void ForkPropertyMonthly(ForkAsset *a)
{
	const Town *t = Town::GetIfValid(static_cast<TownID>(a->location));
	if (t == nullptr) {
		a->value = 0;
		a->monthly_income = 0;
		return;
	}

	RefreshForkProperty(a, t);
	ForkLedgerBook(a->owner, ForkLedgerCategory::Rent, a->monthly_income);
	SetWindowClassesDirty(WindowClass::ForkRealEstate);
}

/** Make the monthly update of houses known to the extended economy. */
void RegisterForkRealEstate()
{
	RegisterForkAssetHandler(ForkAssetType::Property, ForkPropertyMonthly);
}
