/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_labor.cpp Labor market: companies employ managers and administrative staff in towns. Staff costs salaries, and the assets of the company in that town work better with enough staff. */

#include "stdafx.h"

#include "command_func.h"
#include "company_base.h"
#include "company_func.h"
#include "economy_func.h"
#include "fork_economy.h"
#include "fork_firm.h"
#include "fork_labor_cmd.h"
#include "town.h"
#include "window_func.h"

#include "table/strings.h"

#include "safeguards.h"

static constexpr uint FORK_STAFF_MIN_POPULATION = 100; ///< A town needs this many inhabitants before staff can be hired there.
static constexpr uint FORK_STAFF_MAX_PER_TOWN = 30; ///< Never more staff than this per company in one town.
static constexpr uint FORK_STAFF_HIRE_MONTHS = 2; ///< Hiring costs this many monthly salaries per person.
static constexpr uint FORK_STAFF_SEVERANCE_MONTHS = 1; ///< Firing costs this many monthly salaries per person.
static constexpr uint FORK_HOUSES_PER_STAFF = 10; ///< One staff member looks after this many houses.
static constexpr uint FORK_SHOPS_PER_STAFF = 8; ///< One staff member looks after this many shops.

/** @return Whether this asset holds staff of the company in the town. */
static bool IsStaffOf(const ForkAsset *a, CompanyID company, TownID town)
{
	return a->owner == company && a->type == static_cast<uint8_t>(ForkAssetType::Staff) && a->location == town.base();
}

/**
 * Monthly salary of one staff member in a town. Bigger towns pay more, and wages follow the economic cycle:
 * in a boom workers are hard to find and ask more.
 * @param t Town to look at.
 * @return Salary per month.
 */
Money GetForkWage(const Town *t)
{
	Money wage = _price[Price::BuildStationRail] * (15 + GetForkTownSizeLevel(t)) / 10;
	return wage * GetForkMarketFactor(ForkMarketFactor::Wages) / 100;
}

/**
 * How many staff members a company may employ in a town.
 * @param t Town to look at.
 * @return Maximum number of staff.
 */
uint GetForkStaffLimit(const Town *t)
{
	return std::clamp<uint>(t->cache.population / 400, 2, FORK_STAFF_MAX_PER_TOWN);
}

/**
 * Count the staff a company employs in a town.
 * @param company Company to look at.
 * @param town Town to look in.
 * @return Number of staff.
 */
uint GetForkCompanyStaff(CompanyID company, TownID town)
{
	uint count = 0;
	for (const ForkAsset *a : ForkAsset::Iterate()) {
		if (IsStaffOf(a, company, town)) count += static_cast<uint>(std::max<int64_t>(a->data[2], 0));
	}
	return count;
}

/**
 * How many staff members the assets of a company in a town need.
 * @param houses Number of houses.
 * @param shops Number of shops.
 * @return Number of staff needed.
 */
uint GetForkRequiredStaff(uint houses, uint shops, uint firms)
{
	/* Every firm needs a manager of its own. */
	return (houses + FORK_HOUSES_PER_STAFF - 1) / FORK_HOUSES_PER_STAFF + (shops + FORK_SHOPS_PER_STAFF - 1) / FORK_SHOPS_PER_STAFF + firms;
}

/**
 * Efficiency of the assets of a company in a town, from how well they are staffed.
 * Without any staff they work at 50%, with the needed staff at 100%, and extra staff adds up to 10%.
 * @param company Owner of the assets.
 * @param town Town the assets are in.
 * @param houses Number of houses to take into account.
 * @param shops Number of shops to take into account.
 * @return Efficiency in percent.
 */
uint GetForkStaffEfficiencyFor(CompanyID company, TownID town, uint houses, uint shops, uint firms)
{
	const uint required = GetForkRequiredStaff(houses, shops, firms);
	if (required == 0) return 100;

	const uint ratio = GetForkCompanyStaff(company, town) * 100 / required;
	if (ratio < 100) return 50 + ratio / 2;
	return 100 + std::min<uint>(10, (ratio - 100) / 5);
}

/**
 * Efficiency of the assets of a company in a town, with the houses and shops it has now.
 * @param company Owner of the assets.
 * @param town Town the assets are in.
 * @return Efficiency in percent.
 */
uint GetForkStaffEfficiency(CompanyID company, TownID town)
{
	return GetForkStaffEfficiencyFor(company, town, GetForkCompanyHouses(company, town), GetForkCompanyShops(company, town), GetForkCompanyFirms(company, town));
}

/**
 * Hire staff in a town.
 * @param flags Type of operation.
 * @param town_id Town to hire in.
 * @param count Number of people.
 * @return The cost of this operation or an error.
 */
CommandCost CmdHireForkStaff(DoCommandFlags flags, TownID town_id, uint8_t count)
{
	if (!Company::IsValidID(_current_company) || count == 0 || count > FORK_STAFF_MAX_PER_TOWN) return CMD_ERROR;
	Town *t = Town::GetIfValid(town_id);
	if (t == nullptr) return CMD_ERROR;

	if (t->cache.population < FORK_STAFF_MIN_POPULATION) return CommandCost(STR_ERROR_FORK_STAFF_TOWN_TOO_SMALL);
	if (GetForkCompanyStaff(_current_company, town_id) + count > GetForkStaffLimit(t)) return CommandCost(STR_ERROR_FORK_STAFF_LIMIT);

	ForkAsset *existing = nullptr;
	for (ForkAsset *a : ForkAsset::Iterate()) {
		if (IsStaffOf(a, _current_company, town_id)) {
			existing = a;
			break;
		}
	}
	if (existing == nullptr && !ForkAsset::CanAllocateItem()) return CommandCost(STR_ERROR_FORK_TOO_MANY_ASSETS);

	const Money wage = GetForkWage(t);
	const Money fee = wage * (FORK_STAFF_HIRE_MONTHS * count);
	CommandCost cost(ExpensesType::Other, fee);

	if (flags.Test(DoCommandFlag::Execute)) {
		ForkAsset *a = existing;
		if (a == nullptr) {
			a = ForkAsset::Create();
			a->owner = _current_company;
			a->type = static_cast<uint8_t>(ForkAssetType::Staff);
			a->location_kind = static_cast<uint8_t>(ForkAssetLocation::Town);
			a->location = town_id.base();
			a->data[2] = 0;
		}
		a->data[2] += count;
		a->purchase_price += fee;
		a->value = 0; // Staff is not property, so it is not taxed as such.
		a->monthly_cost = wage * a->data[2];
		SetWindowClassesDirty(WindowClass::ForkStaff);
		SetWindowClassesDirty(WindowClass::ForkRealEstate);
		SetWindowClassesDirty(WindowClass::ForkShops);
		InvalidateWindowData(WindowClass::ForkBusiness, 0);
	}
	return cost;
}

/**
 * Fire staff in a town. This costs severance pay.
 * @param flags Type of operation.
 * @param town_id Town to fire in.
 * @param count Number of people.
 * @return The cost of this operation or an error.
 */
CommandCost CmdFireForkStaff(DoCommandFlags flags, TownID town_id, uint8_t count)
{
	if (!Company::IsValidID(_current_company) || count == 0 || count > FORK_STAFF_MAX_PER_TOWN) return CMD_ERROR;
	Town *t = Town::GetIfValid(town_id);
	if (t == nullptr) return CMD_ERROR;

	if (GetForkCompanyStaff(_current_company, town_id) < count) return CommandCost(STR_ERROR_FORK_NO_STAFF);

	CommandCost cost(ExpensesType::Other, GetForkWage(t) * (FORK_STAFF_SEVERANCE_MONTHS * count));

	if (flags.Test(DoCommandFlag::Execute)) {
		int64_t remaining = count;
		for (ForkAsset *a : ForkAsset::Iterate()) {
			if (remaining == 0) break;
			if (!IsStaffOf(a, _current_company, town_id)) continue;
			if (a->data[2] <= remaining) {
				remaining -= a->data[2];
				delete a;
			} else {
				a->data[2] -= remaining;
				a->monthly_cost = GetForkWage(t) * a->data[2];
				remaining = 0;
			}
		}
		SetWindowClassesDirty(WindowClass::ForkStaff);
		SetWindowClassesDirty(WindowClass::ForkRealEstate);
		SetWindowClassesDirty(WindowClass::ForkShops);
		InvalidateWindowData(WindowClass::ForkBusiness, 0);
	}
	return cost;
}

/** Every month the staff is paid. */
static void ForkStaffMonthly(ForkAsset *a)
{
	const Town *t = Town::GetIfValid(static_cast<TownID>(a->location));
	if (t == nullptr) {
		a->monthly_cost = 0;
		return;
	}

	a->value = 0;
	a->monthly_cost = GetForkWage(t) * a->data[2];
	ForkLedgerBook(a->owner, ForkLedgerCategory::Salaries, -a->monthly_cost);
	SetWindowClassesDirty(WindowClass::ForkStaff);
}

/** Make the monthly update of staff known to the extended economy. */
void RegisterForkLabor()
{
	RegisterForkAssetHandler(ForkAssetType::Staff, ForkStaffMonthly);
}
