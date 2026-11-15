/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_shop.cpp Retail shops: companies open shops in towns and earn from what the inhabitants spend, not from transported cargo. */

#include "stdafx.h"

#include "command_func.h"
#include "company_base.h"
#include "company_func.h"
#include "economy_func.h"
#include "fork_economy.h"
#include "fork_shop_cmd.h"
#include "town.h"
#include "window_func.h"

#include "table/strings.h"

#include "safeguards.h"

static constexpr uint FORK_SHOP_MIN_POPULATION = 300; ///< A town needs this many inhabitants before shops can be opened.
static constexpr uint FORK_SHOP_MAX_PER_TOWN = 40; ///< Never more shops than this per company in one town.
static constexpr uint FORK_SHOP_FEE_PERCENT = 5; ///< Fee when buying and selling a shop.
static constexpr uint FORK_SHOP_COGS_PERCENT = 70; ///< Goods cost this much of the normal selling price.
static constexpr uint FORK_SHOP_PRICE_MIN = 70; ///< Lowest price level, in percent of the normal prices.
static constexpr uint FORK_SHOP_PRICE_MAX = 150; ///< Highest price level, in percent of the normal prices.
static constexpr uint FORK_SHOP_PRICE_STEP = 10; ///< Price levels are changed in steps of this many percent.
static constexpr uint FORK_SHOP_PRICE_DEFAULT = 100; ///< Price level used when nothing else was set.

/** @return Number of shops an asset stands for. */
static uint AssetShops(const ForkAsset *a)
{
	return a->data[2] > 0 ? static_cast<uint>(a->data[2]) : 1;
}

/** @return Price level an asset sells at, in percent of the normal prices. */
static uint AssetPrice(const ForkAsset *a)
{
	return a->data[0] > 0 ? static_cast<uint>(a->data[0]) : FORK_SHOP_PRICE_DEFAULT;
}

/** @return Whether this asset holds shops of the company in the town. */
static bool IsShopOf(const ForkAsset *a, CompanyID company, TownID town)
{
	return a->owner == company && a->type == static_cast<uint8_t>(ForkAssetType::Shop) && a->location == town.base();
}

/**
 * How strongly shops attract customers. Low prices attract many, high prices drive customers to the competition.
 * @param price_percent Price level in percent of the normal prices.
 * @return Weight of one shop; 100 is a shop with normal prices.
 */
static uint ShopAttraction(uint price_percent)
{
	return static_cast<uint>(std::max<int>(5, 100 + (100 - static_cast<int>(price_percent)) * 3));
}

/**
 * What the inhabitants of a town spend in shops per month. It depends on the number of inhabitants,
 * on how well off the town is (bigger towns spend more per person) and on the economic cycle.
 * @param t Town to look at.
 * @return Money spent per month.
 */
Money GetForkTownRetailSpending(const Town *t)
{
	Money spending = _price[Price::BuildStationRail] * t->cache.population * 108 / 1000;
	spending = spending * (70 + GetForkTownSizeLevel(t)) / 90;
	return spending * GetForkMarketFactor(ForkMarketFactor::Consumption) / 100;
}

/**
 * Current value of one shop in a town. It follows the size of the town and the economic cycle.
 * @param t Town to look at.
 * @return Value of a shop.
 */
Money GetForkShopValue(const Town *t)
{
	Money value = _price[Price::BuildStationRail] * (60 + GetForkTownSizeLevel(t) * 2);
	return value * GetForkMarketFactor(ForkMarketFactor::Rent) / 100;
}

/**
 * Price to pay for a shop: its value plus the fee.
 * @param t Town to look at.
 * @return Price of a shop.
 */
Money GetForkShopBuyPrice(const Town *t)
{
	Money value = GetForkShopValue(t);
	return value + value * FORK_SHOP_FEE_PERCENT / 100;
}

/**
 * Money received for selling a shop: its value minus the fee.
 * @param t Town to look at.
 * @return Proceeds of selling a shop.
 */
Money GetForkShopSellValue(const Town *t)
{
	Money value = GetForkShopValue(t);
	return value - value * FORK_SHOP_FEE_PERCENT / 100;
}

/**
 * How many shops a company may own in a town: not more than half the houses of the town, and never more than 40.
 * @param t Town to look at.
 * @return Maximum number of shops.
 */
uint GetForkShopLimit(const Town *t)
{
	return std::clamp<uint>(t->cache.num_houses / 2, 3, FORK_SHOP_MAX_PER_TOWN);
}

/**
 * Count the shops a company owns in a town.
 * @param company Company to look at.
 * @param town Town to look in.
 * @return Number of shops.
 */
uint GetForkCompanyShops(CompanyID company, TownID town)
{
	uint count = 0;
	for (const ForkAsset *a : ForkAsset::Iterate()) {
		if (IsShopOf(a, company, town)) count += AssetShops(a);
	}
	return count;
}

/**
 * Price level a company sells at in a town.
 * @param company Company to look at.
 * @param town Town to look in.
 * @return Price level in percent of the normal prices; the default when the company owns no shops there.
 */
uint GetForkShopPrice(CompanyID company, TownID town)
{
	for (const ForkAsset *a : ForkAsset::Iterate()) {
		if (IsShopOf(a, company, town)) return AssetPrice(a);
	}
	return FORK_SHOP_PRICE_DEFAULT;
}

/**
 * Work out what a company earns with its shops in a town.
 * The inhabitants spend a fixed amount in the town's shops. That is divided over all shops, the shops that were
 * already there and the shops of all companies, according to how attractive their prices are. A cheap shop gets
 * more customers but earns less per sale; too many shops in a small town share too little spending.
 * @param t Town to look at.
 * @param company Company that owns the shops.
 * @param shops Number of shops the company has (can be a number that it does not have yet).
 * @param price_percent Price level in percent of the normal prices.
 * @return The result per month.
 */
ForkShopResult GetForkShopResult(const Town *t, CompanyID company, uint shops, uint price_percent)
{
	ForkShopResult r{};
	if (shops == 0) return r;

	/* The shops that were already in the town count as 1 shop per 100 inhabitants. */
	uint64_t others = std::max<uint>(t->cache.population, 100);
	for (const ForkAsset *a : ForkAsset::Iterate()) {
		if (a->type != static_cast<uint8_t>(ForkAssetType::Shop) || a->location != t->index.base() || a->owner == company) continue;
		others += static_cast<uint64_t>(AssetShops(a)) * ShopAttraction(AssetPrice(a));
	}

	const uint64_t own = static_cast<uint64_t>(shops) * ShopAttraction(price_percent);
	const uint64_t share = own * 1000 / (others + own);

	/* Shops with too little staff serve fewer customers. */
	const uint efficiency = GetForkStaffEfficiencyFor(company, t->index, GetForkCompanyHouses(company, t->index), shops);
	const Money volume = GetForkTownRetailSpending(t) * static_cast<int64_t>(share) / 1000 * efficiency / 100; // Sales at normal prices.
	r.share_permille = static_cast<uint>(share);
	r.sales = volume * price_percent / 100;
	r.gross = r.sales - volume * FORK_SHOP_COGS_PERCENT / 100;
	r.fixed = _price[Price::BuildStationRail] * shops / 4; // Premises; the staff is paid through the labor market.
	r.profit = r.gross - r.fixed;
	return r;
}

/** Bring value and income of an asset up to date. */
static void RefreshForkShops(ForkAsset *a, const Town *t)
{
	const uint shops = AssetShops(a);
	const uint price = AssetPrice(a);
	const ForkShopResult r = GetForkShopResult(t, a->owner, shops, price);
	a->data[0] = price;
	a->data[1] = r.share_permille;
	a->data[2] = shops;
	a->value = GetForkShopValue(t) * shops;
	a->monthly_income = r.profit;
}

/**
 * Buy shops in a town.
 * @param flags Type of operation.
 * @param town_id Town to buy in.
 * @param count Number of shops.
 * @return The cost of this operation or an error.
 */
CommandCost CmdBuyForkShops(DoCommandFlags flags, TownID town_id, uint8_t count)
{
	if (!Company::IsValidID(_current_company) || count == 0 || count > FORK_SHOP_MAX_PER_TOWN) return CMD_ERROR;
	Town *t = Town::GetIfValid(town_id);
	if (t == nullptr) return CMD_ERROR;

	if (t->cache.population < FORK_SHOP_MIN_POPULATION) return CommandCost(STR_ERROR_FORK_SHOP_TOWN_TOO_SMALL);
	if (GetForkCompanyShops(_current_company, town_id) + count > GetForkShopLimit(t)) return CommandCost(STR_ERROR_FORK_SHOP_LIMIT);

	ForkAsset *existing = nullptr;
	for (ForkAsset *a : ForkAsset::Iterate()) {
		if (IsShopOf(a, _current_company, town_id)) {
			existing = a;
			break;
		}
	}
	if (existing == nullptr && !ForkAsset::CanAllocateItem()) return CommandCost(STR_ERROR_FORK_TOO_MANY_ASSETS);

	Money price = GetForkShopBuyPrice(t) * count;
	CommandCost cost(ExpensesType::Other, price);

	if (flags.Test(DoCommandFlag::Execute)) {
		ForkAsset *a = existing;
		if (a == nullptr) {
			a = ForkAsset::Create();
			a->owner = _current_company;
			a->type = static_cast<uint8_t>(ForkAssetType::Shop);
			a->location_kind = static_cast<uint8_t>(ForkAssetLocation::Town);
			a->location = town_id.base();
			a->data[0] = FORK_SHOP_PRICE_DEFAULT;
			a->data[2] = 0;
		}
		a->data[2] = (existing != nullptr ? AssetShops(a) : 0) + count;
		a->purchase_price += price;
		RefreshForkShops(a, t);
		SetWindowClassesDirty(WindowClass::ForkShops);
		InvalidateWindowData(WindowClass::ForkBusiness, 0);
	}
	return cost;
}

/**
 * Sell shops in a town.
 * @param flags Type of operation.
 * @param town_id Town to sell in.
 * @param count Number of shops.
 * @return The cost (negative: proceeds) of this operation or an error.
 */
CommandCost CmdSellForkShops(DoCommandFlags flags, TownID town_id, uint8_t count)
{
	if (!Company::IsValidID(_current_company) || count == 0 || count > FORK_SHOP_MAX_PER_TOWN) return CMD_ERROR;
	Town *t = Town::GetIfValid(town_id);
	if (t == nullptr) return CMD_ERROR;

	if (GetForkCompanyShops(_current_company, town_id) < count) return CommandCost(STR_ERROR_FORK_NO_SHOPS);

	CommandCost cost(ExpensesType::Other, -(GetForkShopSellValue(t) * count));

	if (flags.Test(DoCommandFlag::Execute)) {
		uint remaining = count;
		for (ForkAsset *a : ForkAsset::Iterate()) {
			if (remaining == 0) break;
			if (!IsShopOf(a, _current_company, town_id)) continue;
			uint shops = AssetShops(a);
			if (shops <= remaining) {
				remaining -= shops;
				delete a;
			} else {
				a->purchase_price = a->purchase_price * (shops - remaining) / shops;
				a->data[2] = shops - remaining;
				remaining = 0;
				RefreshForkShops(a, t);
			}
		}
		SetWindowClassesDirty(WindowClass::ForkShops);
		InvalidateWindowData(WindowClass::ForkBusiness, 0);
	}
	return cost;
}

/**
 * Change the price level of the shops of a company in a town.
 * @param flags Type of operation.
 * @param town_id Town of the shops.
 * @param percent New price level in percent of the normal prices.
 * @return The cost of this operation (none) or an error.
 */
CommandCost CmdSetForkShopPrice(DoCommandFlags flags, TownID town_id, uint16_t percent)
{
	if (!Company::IsValidID(_current_company)) return CMD_ERROR;
	Town *t = Town::GetIfValid(town_id);
	if (t == nullptr) return CMD_ERROR;
	if (percent < FORK_SHOP_PRICE_MIN || percent > FORK_SHOP_PRICE_MAX || percent % FORK_SHOP_PRICE_STEP != 0) return CMD_ERROR;

	if (GetForkCompanyShops(_current_company, town_id) == 0) return CommandCost(STR_ERROR_FORK_NO_SHOPS);

	if (flags.Test(DoCommandFlag::Execute)) {
		for (ForkAsset *a : ForkAsset::Iterate()) {
			if (!IsShopOf(a, _current_company, town_id)) continue;
			a->data[0] = percent;
			RefreshForkShops(a, t);
		}
		SetWindowClassesDirty(WindowClass::ForkShops);
	}
	return CommandCost();
}

/** Every month the shops are revalued and the owner receives the earnings. */
static void ForkShopMonthly(ForkAsset *a)
{
	const Town *t = Town::GetIfValid(static_cast<TownID>(a->location));
	if (t == nullptr) {
		a->value = 0;
		a->monthly_income = 0;
		return;
	}

	const ForkShopResult r = GetForkShopResult(t, a->owner, AssetShops(a), AssetPrice(a));
	RefreshForkShops(a, t);
	ForkLedgerBook(a->owner, ForkLedgerCategory::Trade, r.gross);
	ForkLedgerBook(a->owner, ForkLedgerCategory::Other, -r.fixed);
	SetWindowClassesDirty(WindowClass::ForkShops);
}

/** Make the monthly update of shops known to the extended economy. */
void RegisterForkShops()
{
	RegisterForkAssetHandler(ForkAssetType::Shop, ForkShopMonthly);
}
