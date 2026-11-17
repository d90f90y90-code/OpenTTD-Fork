/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/**
 * @file fork_firm.cpp Technology firms in towns.
 *
 * A firm makes one product. Every month it makes units (limited by what the firm can make and by its staff), then all firms
 * with the same product share the demand of the inhabitants, together with the imports that are always there. How much a firm
 * sells depends on its price (also after a discount) and on its quality: cheap and good products win customers. The market
 * price of a product moves with what is on offer: too much stock pushes it down. Firms of a company get the benefit of the
 * retail shops of that company in the same town, and need staff from the labor market.
 */

#include "stdafx.h"

#include "command_func.h"
#include "company_base.h"
#include "company_func.h"
#include "core/pool_func.hpp"
#include "core/random_func.hpp"
#include "economy_func.h"
#include "fork_economy.h"
#include "fork_firm.h"
#include "fork_firm_cmd.h"
#include "news_func.h"
#include "string_func.h"
#include "strings_func.h"
#include "town.h"
#include "window_func.h"

#include "table/strings.h"

#include "safeguards.h"

ForkFirmPool _fork_firm_pool("ForkFirm");
INSTANTIATE_POOL_METHODS(ForkFirm)

/** What is known about a product. */
struct ForkProductInfo {
	uint dollars; ///< Price of one unit at the start of a game, in dollars.
	uint capacity; ///< Units a firm can make per month in a small town.
	uint demand; ///< Units the inhabitants buy per month, per 100000 inhabitants.
};

static constexpr ForkProductInfo FORK_PRODUCTS[FORK_PRODUCT_COUNT] = {
	{420, 40, 45}, // Dishwasher
	{480, 36, 40}, // Washing machine
	{360, 48, 50}, // Lawn mower
	{12, 1500, 1500}, // Lamps
	{650, 28, 35}, // Television
	{110, 150, 170}, // Radio
};

static constexpr uint FORK_QUALITY_APPEAL[FORK_FIRM_MAX_QUALITY] = {70, 85, 100, 120, 140}; ///< How much customers like each quality level.
static constexpr uint FORK_QUALITY_COST[FORK_FIRM_MAX_QUALITY] = {70, 85, 100, 130, 170}; ///< Production cost per quality level, in percent.
static constexpr uint FORK_PRODUCTION_COST_PERCENT = 45; ///< Cost of making a unit of average quality, in percent of the price.
static constexpr uint FORK_NPC_FIRM_LIMIT = 10; ///< Never more firms of the towns than this.

static const char *const FORK_NPC_NAMES[] = {
	"Nova Tech", "Tigris Appliances", "Babylon Electric", "Delta Devices", "Orbit Home",
	"Zagros Tech", "Al-Noor Electronics", "Euphrates Home", "Sunrise Tech", "Atlas Appliances",
	"Crescent Electric", "Mesopotamia Tech",
};

/** @return Money that a rail station costs, the unit in which all prices of the fork economy are measured. */
static Money Unit()
{
	return _price[Price::BuildStationRail];
}

/**
 * Price of one unit of a product, before the market and discounts.
 * @param product The product, a #ForkProduct.
 * @return Base price.
 */
Money GetForkProductPrice(uint product)
{
	if (product >= FORK_PRODUCT_COUNT) return 0;
	/* A rail station costs about 400 dollars at the start of a game; prices are relative to that. */
	return std::max<Money>(Money(1), Unit() * FORK_PRODUCTS[product].dollars / 400);
}

/**
 * What it costs to make one unit.
 * @param product The product.
 * @param quality Quality level, 1 to 5.
 * @return Production cost of a unit.
 */
Money GetForkProductCost(uint product, uint quality)
{
	quality = std::clamp<uint>(quality, 1, FORK_FIRM_MAX_QUALITY);
	return std::max<Money>(Money(1), GetForkProductPrice(product) * FORK_PRODUCTION_COST_PERCENT / 100 * FORK_QUALITY_COST[quality - 1] / 100);
}

/** @return What it costs to found a firm. */
Money GetForkFirmFoundingCost()
{
	return Unit() * 250;
}

/**
 * How many units a firm can make per month when it works at full rate. A bigger town gives more workers, and the staff of the
 * company that owns the firm decides how well they work.
 * @param f The firm.
 * @return Units per month.
 */
uint GetForkFirmCapacity(const ForkFirm *f)
{
	if (f->product >= FORK_PRODUCT_COUNT) return 0;
	const Town *t = Town::GetIfValid(f->GetTown());
	const uint level = t != nullptr ? GetForkTownSizeLevel(t) : 0;

	uint capacity = FORK_PRODUCTS[f->product].capacity * (60 + level) / 100;
	if (Company::IsValidID(f->owner)) capacity = capacity * GetForkStaffEfficiency(f->owner, f->GetTown()) / 100;
	return std::max<uint>(capacity, 1);
}

/**
 * Count the firms in a town.
 * @param town Town to look in.
 * @return Number of firms of all owners.
 */
uint GetForkFirmsInTown(TownID town)
{
	uint count = 0;
	for (const ForkFirm *f : ForkFirm::Iterate()) {
		if (f->GetTown() == town) count++;
	}
	return count;
}

/**
 * Count the firms of a company in a town.
 * @param company Owner.
 * @param town Town to look in.
 * @return Number of firms.
 */
uint GetForkCompanyFirms(CompanyID company, TownID town)
{
	uint count = 0;
	for (const ForkFirm *f : ForkFirm::Iterate()) {
		if (f->owner == company && f->GetTown() == town) count++;
	}
	return count;
}

/** @return The price in percent of the base price that customers pay after the discount. */
static uint EffectivePercent(const ForkFirm *f)
{
	return std::max<uint>(10, f->price_percent * (100 - f->discount) / 100);
}

/**
 * What a customer pays for one unit.
 * @param f The firm.
 * @return Price after the discount.
 */
Money GetForkFirmSellingPrice(const ForkFirm *f)
{
	return GetForkProductPrice(f->product) * EffectivePercent(f) / 100;
}

/** Make a new firm of the town, with its own ideas about prices, and tell the newspaper. */
static void SpawnFirmOfTown(const Town *t)
{
	if (!ForkFirm::CanAllocateItem()) return;

	ForkFirm *f = ForkFirm::Create();
	f->name = FORK_NPC_NAMES[RandomRange(lengthof(FORK_NPC_NAMES))];
	f->owner = CompanyID::Invalid();
	f->town = t->index.base();
	f->product = static_cast<uint8_t>(RandomRange(FORK_PRODUCT_COUNT));
	f->quality = static_cast<uint8_t>(2 + RandomRange(3));
	f->rate = 90;
	f->personality = static_cast<int8_t>(static_cast<int>(RandomRange(21)) - 10);
	f->price_percent = static_cast<uint8_t>(std::clamp<int>(_fork_economy.firm_ref_price[f->product] + f->personality, 50, 200));
	f->value = GetForkFirmFoundingCost() / 2;

	AddNewsItem(GetEncodedString(STR_FORK_NEWS_FIRM_NPC, f->name, t->index, STR_FORK_PRODUCT_DISHWASHER + f->product), NewsType::Economy, NewsStyle::Normal, {});
}

/**
 * Let the firms with one product make their goods, divide the demand of the inhabitants and settle the money.
 * @param product The product.
 * @param total_population Inhabitants of all towns together.
 */
static void RunProductMarket(uint product, uint64_t total_population)
{
	const ForkProductInfo &info = FORK_PRODUCTS[product];
	std::vector<ForkFirm *> firms;
	for (ForkFirm *f : ForkFirm::Iterate()) {
		if (f->product == product) firms.push_back(f);
	}

	const uint ref = std::clamp<int>(_fork_economy.firm_ref_price[product], 60, 140);

	/* Make goods. */
	std::vector<Money> costs(firms.size(), 0);
	for (size_t i = 0; i < firms.size(); i++) {
		ForkFirm *f = firms[i];
		const uint capacity = GetForkFirmCapacity(f);
		const uint room = capacity * 3 > f->stock ? capacity * 3 - f->stock : 0; // The warehouse holds three months of production.
		uint units = std::min<uint>(capacity * f->rate / 100, room);
		if (f->rate > 0 && units == 0 && room > 0) units = 1;
		f->made_last = units;
		f->stock += units;
		costs[i] = GetForkProductCost(product, f->quality) * units;
	}

	/* How attractive every firm is: a good quality and a low price win customers, and a company that has shops in the town sells more. */
	const uint64_t imports = 100ULL * 1000000 / (static_cast<uint64_t>(ref) * ref);
	uint64_t total_attraction = imports;
	uint64_t weighted_price = imports * ref;
	std::vector<uint64_t> attraction(firms.size(), 0);
	for (size_t i = 0; i < firms.size(); i++) {
		const ForkFirm *f = firms[i];
		const uint price = EffectivePercent(f);
		uint shop_bonus = 0;
		if (Company::IsValidID(f->owner)) shop_bonus = std::min<uint>(30, GetForkCompanyShops(f->owner, f->GetTown()) * 3);
		attraction[i] = FORK_QUALITY_APPEAL[std::clamp<uint>(f->quality, 1, FORK_FIRM_MAX_QUALITY) - 1] * 1000000ULL / (static_cast<uint64_t>(price) * price) * (100 + shop_bonus) / 100;
		total_attraction += attraction[i];
		weighted_price += attraction[i] * price;
	}

	/* The inhabitants buy more when things are cheap and when times are good. */
	const uint64_t average_price = std::max<uint64_t>(10, weighted_price / total_attraction);
	const uint elasticity = static_cast<uint>(std::clamp<uint64_t>(ref * 100 / average_price, 50, 200));
	uint64_t demand100 = total_population * info.demand / 1000; // Units times 100.
	demand100 = demand100 * GetForkMarketFactor(ForkMarketFactor::Consumption) / 100;
	demand100 = demand100 * elasticity / 100;

	uint64_t stock_left = 0;
	for (size_t i = 0; i < firms.size(); i++) {
		ForkFirm *f = firms[i];
		const uint64_t want = (demand100 * attraction[i] / total_attraction + 50) / 100;
		const uint sold = static_cast<uint>(std::min<uint64_t>(f->stock, want));
		f->stock -= sold;
		f->sold_last = sold;
		stock_left += f->stock;

		const Money revenue = GetForkFirmSellingPrice(f) * sold;
		const Money holding = GetForkProductCost(product, f->quality) * f->stock / 100;
		f->revenue_last = revenue;
		f->profit_last = revenue - costs[i] - holding;
		f->profit_average = (f->profit_average * 3 + f->profit_last) / 4;
		f->value = std::max<Money>(GetForkFirmFoundingCost() / 2, f->profit_average * 30);
		f->age_months++;

		if (Company::IsValidID(f->owner)) {
			ForkLedgerBook(f->owner, ForkLedgerCategory::Trade, revenue - costs[i]);
			ForkLedgerBook(f->owner, ForkLedgerCategory::Other, -holding);
		} else {
			/* Firms of the town follow the market, and put things on sale when the warehouse is full. */
			f->price_percent = static_cast<uint8_t>(std::clamp<int>(ref + f->personality, 50, 200));
			f->discount = f->stock > GetForkFirmCapacity(f) * 2 ? 15 : 0;
		}
	}

	/* The market price follows what is on offer: stock that does not sell pushes the price down, empty warehouses push it up. */
	const uint64_t demand_units = demand100 / 100;
	int new_ref = ref;
	if (stock_left > demand_units) {
		new_ref -= 1;
	} else if (stock_left * 3 < demand_units) {
		new_ref += 1;
	}
	new_ref += (100 - static_cast<int>(ref)) / 10; // The price slowly moves back to normal when the pressure stops.
	_fork_economy.firm_ref_price[product] = static_cast<int16_t>(std::clamp(new_ref, 60, 140));
}

/** Every month firms of the towns appear, make their goods and sell them. */
void ForkFirmsMonthly()
{
	uint npc_firms = 0;
	for (const ForkFirm *f : ForkFirm::Iterate()) {
		if (!Company::IsValidID(f->owner)) npc_firms++;
	}

	uint64_t total_population = 0;
	for (const Town *t : Town::Iterate()) {
		total_population += t->cache.population;
		if (t->cache.population >= FORK_FIRM_MIN_POPULATION && npc_firms < FORK_NPC_FIRM_LIMIT && GetForkFirmsInTown(t->index) < 2 && RandomRange(36) == 0) {
			SpawnFirmOfTown(t);
			npc_firms++;
		}
	}

	for (uint p = 0; p < FORK_PRODUCT_COUNT; p++) RunProductMarket(p, total_population);

	SetWindowClassesDirty(WindowClass::ForkFirms);
	SetWindowClassesDirty(WindowClass::ForkFirm);
}

/**
 * Found a firm in a town.
 * @param flags Type of operation.
 * @param town_id Town to found the firm in.
 * @param product What the firm will make.
 * @param name Name of the firm.
 * @return The cost of this operation or an error.
 */
CommandCost CmdCreateForkFirm(DoCommandFlags flags, TownID town_id, uint8_t product, const std::string &name)
{
	if (!Company::IsValidID(_current_company) || product >= FORK_PRODUCT_COUNT) return CMD_ERROR;
	Town *t = Town::GetIfValid(town_id);
	if (t == nullptr) return CMD_ERROR;

	if (name.empty() || Utf8StringLength(name) >= MAX_LENGTH_COMPANY_NAME_CHARS) return CommandCost(STR_ERROR_FORK_FIRM_NAME);
	if (t->cache.population < FORK_FIRM_MIN_POPULATION) return CommandCost(STR_ERROR_FORK_FIRM_TOWN_TOO_SMALL);
	if (GetForkFirmsInTown(town_id) >= FORK_FIRM_MAX_PER_TOWN) return CommandCost(STR_ERROR_FORK_FIRM_TOWN_FULL);
	if (!ForkFirm::CanAllocateItem()) return CommandCost(STR_ERROR_FORK_TOO_MANY_ASSETS);

	CommandCost cost(ExpensesType::Other, GetForkFirmFoundingCost());

	if (flags.Test(DoCommandFlag::Execute)) {
		ForkFirm *f = ForkFirm::Create();
		f->name = name;
		f->owner = _current_company;
		f->town = town_id.base();
		f->product = product;
		f->quality = 3;
		f->rate = 60;
		f->price_percent = static_cast<uint8_t>(std::clamp<int>(_fork_economy.firm_ref_price[product], 50, 200));
		f->value = GetForkFirmFoundingCost() / 2;

		AddNewsItem(GetEncodedString(STR_FORK_NEWS_FIRM_OWN, f->name, _current_company, STR_FORK_PRODUCT_DISHWASHER + product), NewsType::Economy, NewsStyle::Normal, {});
		SetWindowClassesDirty(WindowClass::ForkFirms);
		InvalidateWindowData(WindowClass::ForkBusiness, 0);
	}
	return cost;
}

/**
 * Change how a firm of the company works.
 * @param flags Type of operation.
 * @param firm_index The firm.
 * @param what What to change: 0 production rate, 1 quality, 2 price, 3 discount.
 * @param value The new value.
 * @return The cost of this operation (none) or an error.
 */
CommandCost CmdSetForkFirm(DoCommandFlags flags, uint32_t firm_index, uint8_t what, uint8_t value)
{
	ForkFirm *f = ForkFirm::GetIfValid(static_cast<ForkFirmID>(firm_index));
	if (f == nullptr || f->owner != _current_company) return CMD_ERROR;

	switch (what) {
		case 0: if (value > 100 || value % 10 != 0) return CMD_ERROR; break;
		case 1: if (value < 1 || value > FORK_FIRM_MAX_QUALITY) return CMD_ERROR; break;
		case 2: if (value < 50 || value > 200 || value % 5 != 0) return CMD_ERROR; break;
		case 3: if (value > 50 || value % 5 != 0) return CMD_ERROR; break;
		default: return CMD_ERROR;
	}

	if (flags.Test(DoCommandFlag::Execute)) {
		switch (what) {
			case 0: f->rate = value; break;
			case 1: f->quality = value; break;
			case 2: f->price_percent = value; break;
			default: f->discount = value; break;
		}
		SetWindowClassesDirty(WindowClass::ForkFirms);
		SetWindowClassesDirty(WindowClass::ForkFirm);
	}
	return CommandCost();
}

/**
 * Throw away the stock of a firm. What was paid for it is lost.
 * @param flags Type of operation.
 * @param firm_index The firm.
 * @return The cost of this operation (none) or an error.
 */
CommandCost CmdDumpForkFirmStock(DoCommandFlags flags, uint32_t firm_index)
{
	ForkFirm *f = ForkFirm::GetIfValid(static_cast<ForkFirmID>(firm_index));
	if (f == nullptr || f->owner != _current_company) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		f->stock = 0;
		SetWindowClassesDirty(WindowClass::ForkFirms);
		SetWindowClassesDirty(WindowClass::ForkFirm);
	}
	return CommandCost();
}
