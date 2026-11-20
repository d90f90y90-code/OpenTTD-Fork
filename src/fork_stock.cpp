/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/**
 * @file fork_stock.cpp Stock exchange.
 *
 * Twelve companies are listed. Their prices follow the economic cycle (boom lifts them, recession pushes them down) with some
 * random movement every month, and they pay dividends. The firms of the towns can be invested in too: up to half of a firm can be
 * bought, and with half of it a company can take the firm over by paying 25 times the price of 1%, which brings the share to 51%
 * and makes the company the owner.
 *
 * Holdings are assets of type Shares: data[0] is 1 for a listed company or 2 for a firm of a town, data[1] the company or the
 * firm, data[2] the number of shares or the percentage.
 */

#include "stdafx.h"

#include "command_func.h"
#include "company_base.h"
#include "company_func.h"
#include "core/random_func.hpp"
#include "economy_func.h"
#include "fork_economy.h"
#include "fork_firm.h"
#include "fork_stock.h"
#include "fork_stock_cmd.h"
#include "fork_valuation.h"
#include "news_func.h"
#include "strings_func.h"
#include "town.h"
#include "window_func.h"

#include "table/strings.h"

#include "safeguards.h"

static constexpr int64_t HOLDING_LISTED = 1; ///< Holding in a listed company.
static constexpr int64_t HOLDING_FIRM = 2; ///< Holding in a firm of a town.
static constexpr uint STOCK_FEE_PERCENT = 1; ///< Brokerage on listed shares.
static constexpr uint FIRM_FEE_PERCENT = 5; ///< Fee on shares of firms of the towns.
static constexpr uint TAKEOVER_FACTOR = 25; ///< A takeover costs this many times the price of 1%.
static constexpr uint FIRM_PAYOUT_PERCENT = 50; ///< Part of its profit that a firm pays to its shareholders.

/** What is known about a listed company. */
struct ForkListedInfo {
	uint dollars; ///< Price of a share at the start of a game, in dollars.
	uint volatility; ///< How much the price moves randomly per month, in permille.
	uint yield_tenths; ///< Dividend per year, in tenths of a percent of the price.
};

static constexpr ForkListedInfo FORK_LISTED[FORK_LISTED_COUNT] = {
	{180, 40, 5}, // Pears
	{320, 30, 10}, // WenDoor
	{140, 50, 0}, // Zookal
	{60, 40, 25}, // Inter Mains
	{90, 40, 20}, // Samsong
	{70, 40, 15}, // Sunny
	{250, 80, 0}, // Tezla
	{130, 50, 0}, // Amazoon
	{400, 60, 0}, // Netflux
	{300, 60, 0}, // Faceboot
	{100, 30, 12}, // Nikee
	{55, 20, 30}, // Cola-Koala
};

/** @return The price a share would have without any market movement. */
static Money BasePrice(uint index)
{
	return std::max<Money>(Money(1), _price[Price::BuildStationRail] * FORK_LISTED[index].dollars / 400);
}

/**
 * Price of a share of a listed company.
 * @param index Number of the company.
 * @return Price of one share.
 */
Money GetForkStockPrice(uint index)
{
	if (index >= FORK_LISTED_COUNT) return 0;
	const int64_t stored = _fork_economy.stock_price[index];
	return stored > 0 ? Money(stored) : BasePrice(index);
}

/**
 * How much the price of a company changed last month.
 * @param index Number of the company.
 * @return Change in permille.
 */
int GetForkStockChangePermille(uint index)
{
	if (index >= FORK_LISTED_COUNT) return 0;
	const int64_t previous = _fork_economy.stock_previous[index];
	const int64_t now = _fork_economy.stock_price[index];
	if (previous <= 0 || now <= 0) return 0;
	return static_cast<int>((now - previous) * 1000 / previous);
}

/**
 * Dividend of a listed company.
 * @param index Number of the company.
 * @return Dividend per year in tenths of a percent of the price.
 */
uint GetForkStockYieldTenths(uint index)
{
	return index < FORK_LISTED_COUNT ? FORK_LISTED[index].yield_tenths : 0;
}

/** @return The asset with a holding, or nullptr. */
static ForkAsset *FindHolding(CompanyID company, int64_t kind, uint64_t index)
{
	for (ForkAsset *a : ForkAsset::Iterate()) {
		if (a->owner == company && a->type == static_cast<uint8_t>(ForkAssetType::Shares) && a->data[0] == kind && a->data[1] == static_cast<int64_t>(index)) return a;
	}
	return nullptr;
}

/**
 * Number of shares of a listed company that a company holds.
 * @param company Owner of the shares.
 * @param index Number of the listed company.
 * @return Number of shares.
 */
uint GetForkStockHeld(CompanyID company, uint index)
{
	const ForkAsset *a = FindHolding(company, HOLDING_LISTED, index);
	return a != nullptr ? static_cast<uint>(a->data[2]) : 0;
}

/**
 * What a company paid for the shares of a listed company that it holds.
 * @param company Owner of the shares.
 * @param index Number of the listed company.
 * @return Money paid.
 */
Money GetForkStockCost(CompanyID company, uint index)
{
	const ForkAsset *a = FindHolding(company, HOLDING_LISTED, index);
	return a != nullptr ? a->purchase_price : Money(0);
}

/**
 * Price of 1% of a firm.
 * @param f The firm.
 * @return Price of 1%.
 */
Money GetForkFirmShareValue(const ForkFirm *f)
{
	return std::max<Money>(Money(1), GetForkFirmSaleValue(f, Town::GetIfValid(f->GetTown())) / 100);
}

/**
 * What a takeover of a firm costs: 25 times the price of 1%.
 * @param f The firm.
 * @return Price of the takeover.
 */
Money GetForkFirmTakeoverPrice(const ForkFirm *f)
{
	return GetForkFirmShareValue(f) * TAKEOVER_FACTOR;
}

/**
 * Percentage of a firm that a company holds.
 * @param company Owner of the shares.
 * @param firm Index of the firm.
 * @return Percentage.
 */
uint GetForkFirmStake(CompanyID company, uint32_t firm)
{
	const ForkAsset *a = FindHolding(company, HOLDING_FIRM, firm);
	return a != nullptr ? static_cast<uint>(a->data[2]) : 0;
}

/**
 * Percentage of a firm that all companies together hold.
 * @param firm Index of the firm.
 * @return Percentage.
 */
uint GetForkFirmStakeTotal(uint32_t firm)
{
	uint total = 0;
	for (const ForkAsset *a : ForkAsset::Iterate()) {
		if (a->type == static_cast<uint8_t>(ForkAssetType::Shares) && a->data[0] == HOLDING_FIRM && a->data[1] == static_cast<int64_t>(firm)) total += static_cast<uint>(a->data[2]);
	}
	return total;
}

/** Every month the prices move and, as the economy changes, so does what the shares are worth. */
void ForkStocksMonthly()
{
	const uint factor = GetForkMarketFactor(ForkMarketFactor::StockPrice);
	for (uint i = 0; i < FORK_LISTED_COUNT; i++) {
		const Money base = BasePrice(i);
		Money price = _fork_economy.stock_price[i] > 0 ? Money(_fork_economy.stock_price[i]) : base;
		_fork_economy.stock_previous[i] = static_cast<int64_t>(price);

		/* The price is pulled towards what the economy says it should be, and moves a bit at random. */
		const Money target = base * factor / 100;
		const int noise = static_cast<int>(RandomRange(2 * FORK_LISTED[i].volatility + 1)) - static_cast<int>(FORK_LISTED[i].volatility);
		price += (target - price) * 12 / 100 + price * noise / 1000;
		_fork_economy.stock_price[i] = static_cast<int64_t>(std::max<Money>(Money(1), price));
	}
	SetWindowClassesDirty(WindowClass::ForkExchange);
}

/**
 * Buy shares of a listed company.
 * @param flags Type of operation.
 * @param listed Number of the company.
 * @param count Number of shares, 1 to 100.
 * @return The cost of this operation or an error.
 */
CommandCost CmdBuyForkStock(DoCommandFlags flags, uint8_t listed, uint8_t count)
{
	if (!Company::IsValidID(_current_company) || listed >= FORK_LISTED_COUNT || count == 0 || count > 100) return CMD_ERROR;

	ForkAsset *existing = FindHolding(_current_company, HOLDING_LISTED, listed);
	if (existing == nullptr && !ForkAsset::CanAllocateItem()) return CommandCost(STR_ERROR_FORK_TOO_MANY_ASSETS);

	const Money price = GetForkStockPrice(listed) * count;
	const Money cost_money = price + price * STOCK_FEE_PERCENT / 100;

	if (flags.Test(DoCommandFlag::Execute)) {
		ForkAsset *a = existing;
		if (a == nullptr) {
			a = ForkAsset::Create();
			a->owner = _current_company;
			a->type = static_cast<uint8_t>(ForkAssetType::Shares);
			a->location_kind = static_cast<uint8_t>(ForkAssetLocation::None);
			a->data[0] = HOLDING_LISTED;
			a->data[1] = listed;
			a->data[2] = 0;
		}
		a->data[2] += count;
		a->purchase_price += cost_money;
		a->value = GetForkStockPrice(listed) * static_cast<int>(a->data[2]);
		SetWindowClassesDirty(WindowClass::ForkExchange);
		InvalidateWindowData(WindowClass::ForkBusiness, 0);
	}
	return CommandCost(ExpensesType::Other, cost_money);
}

/**
 * Sell shares of a listed company.
 * @param flags Type of operation.
 * @param listed Number of the company.
 * @param count Number of shares, 1 to 100.
 * @return The cost (negative: proceeds) of this operation or an error.
 */
CommandCost CmdSellForkStock(DoCommandFlags flags, uint8_t listed, uint8_t count)
{
	if (!Company::IsValidID(_current_company) || listed >= FORK_LISTED_COUNT || count == 0 || count > 100) return CMD_ERROR;

	ForkAsset *a = FindHolding(_current_company, HOLDING_LISTED, listed);
	if (a == nullptr || a->data[2] < count) return CommandCost(STR_ERROR_FORK_STOCK_NOT_ENOUGH);

	const Money price = GetForkStockPrice(listed) * count;
	const Money proceeds = price - price * STOCK_FEE_PERCENT / 100;

	if (flags.Test(DoCommandFlag::Execute)) {
		const int before = static_cast<int>(a->data[2]);
		a->purchase_price = a->purchase_price * (before - count) / before;
		a->data[2] -= count;
		if (a->data[2] <= 0) {
			delete a;
		} else {
			a->value = GetForkStockPrice(listed) * static_cast<int>(a->data[2]);
		}
		SetWindowClassesDirty(WindowClass::ForkExchange);
		InvalidateWindowData(WindowClass::ForkBusiness, 0);
	}
	return CommandCost(ExpensesType::Other, -proceeds);
}

/** @return The firm if it exists and belongs to someone in the town, so its shares can be traded. */
static ForkFirm *GetIndependentFirm(uint32_t firm_index)
{
	ForkFirm *f = ForkFirm::GetIfValid(static_cast<ForkFirmID>(firm_index));
	return (f != nullptr && !Company::IsValidID(f->owner)) ? f : nullptr;
}

/**
 * Buy a part of a firm of a town.
 * @param flags Type of operation.
 * @param firm_index The firm.
 * @param percent Percentage to buy, 1 to 50.
 * @return The cost of this operation or an error.
 */
CommandCost CmdBuyForkFirmShare(DoCommandFlags flags, uint32_t firm_index, uint8_t percent)
{
	if (!Company::IsValidID(_current_company) || percent == 0 || percent > FORK_FIRM_MAX_STAKE) return CMD_ERROR;
	ForkFirm *f = GetIndependentFirm(firm_index);
	if (f == nullptr) return CommandCost(STR_ERROR_FORK_STOCK_NOT_INDEPENDENT);
	if (GetForkFirmStakeTotal(firm_index) + percent > FORK_FIRM_MAX_STAKE) return CommandCost(STR_ERROR_FORK_STOCK_FIRM_LIMIT);

	ForkAsset *existing = FindHolding(_current_company, HOLDING_FIRM, firm_index);
	if (existing == nullptr && !ForkAsset::CanAllocateItem()) return CommandCost(STR_ERROR_FORK_TOO_MANY_ASSETS);

	const Money price = GetForkFirmShareValue(f) * percent;
	const Money cost_money = price + price * FIRM_FEE_PERCENT / 100;

	if (flags.Test(DoCommandFlag::Execute)) {
		ForkAsset *a = existing;
		if (a == nullptr) {
			a = ForkAsset::Create();
			a->owner = _current_company;
			a->type = static_cast<uint8_t>(ForkAssetType::Shares);
			a->location_kind = static_cast<uint8_t>(ForkAssetLocation::None);
			a->data[0] = HOLDING_FIRM;
			a->data[1] = firm_index;
			a->data[2] = 0;
		}
		a->data[2] += percent;
		a->purchase_price += cost_money;
		a->value = GetForkFirmShareValue(f) * static_cast<int>(a->data[2]);
		SetWindowClassesDirty(WindowClass::ForkExchange);
		InvalidateWindowData(WindowClass::ForkBusiness, 0);
	}
	return CommandCost(ExpensesType::Other, cost_money);
}

/**
 * Sell a part of a firm of a town.
 * @param flags Type of operation.
 * @param firm_index The firm.
 * @param percent Percentage to sell.
 * @return The cost (negative: proceeds) of this operation or an error.
 */
CommandCost CmdSellForkFirmShare(DoCommandFlags flags, uint32_t firm_index, uint8_t percent)
{
	if (!Company::IsValidID(_current_company) || percent == 0 || percent > FORK_FIRM_MAX_STAKE) return CMD_ERROR;
	ForkFirm *f = GetIndependentFirm(firm_index);
	if (f == nullptr) return CommandCost(STR_ERROR_FORK_STOCK_NOT_INDEPENDENT);

	ForkAsset *a = FindHolding(_current_company, HOLDING_FIRM, firm_index);
	if (a == nullptr || a->data[2] < percent) return CommandCost(STR_ERROR_FORK_STOCK_NOT_ENOUGH);

	const Money price = GetForkFirmShareValue(f) * percent;
	const Money proceeds = price - price * FIRM_FEE_PERCENT / 100;

	if (flags.Test(DoCommandFlag::Execute)) {
		const int before = static_cast<int>(a->data[2]);
		a->purchase_price = a->purchase_price * (before - percent) / before;
		a->data[2] -= percent;
		if (a->data[2] <= 0) {
			delete a;
		} else {
			a->value = GetForkFirmShareValue(f) * static_cast<int>(a->data[2]);
		}
		SetWindowClassesDirty(WindowClass::ForkExchange);
		InvalidateWindowData(WindowClass::ForkBusiness, 0);
	}
	return CommandCost(ExpensesType::Other, -proceeds);
}

/**
 * Take over a firm of a town. This needs half of its shares and costs 25 times the price of 1%; that last percent
 * brings the company to 51%, so it owns the firm from then on.
 * @param flags Type of operation.
 * @param firm_index The firm.
 * @return The cost of this operation or an error.
 */
CommandCost CmdTakeOverForkFirm(DoCommandFlags flags, uint32_t firm_index)
{
	if (!Company::IsValidID(_current_company)) return CMD_ERROR;
	ForkFirm *f = GetIndependentFirm(firm_index);
	if (f == nullptr) return CommandCost(STR_ERROR_FORK_STOCK_NOT_INDEPENDENT);
	if (GetForkFirmStake(_current_company, firm_index) < FORK_FIRM_MAX_STAKE) return CommandCost(STR_ERROR_FORK_STOCK_NEED_HALF);

	CommandCost cost(ExpensesType::Other, GetForkFirmTakeoverPrice(f));

	if (flags.Test(DoCommandFlag::Execute)) {
		f->owner = _current_company;
		f->value = std::max(f->value, GetForkFirmFoundingCost() / 2);

		/* Every holding in the firm is over: the company owns it now. */
		for (ForkAsset *a : ForkAsset::Iterate()) {
			if (a->type == static_cast<uint8_t>(ForkAssetType::Shares) && a->data[0] == HOLDING_FIRM && a->data[1] == static_cast<int64_t>(firm_index)) delete a;
		}

		AddNewsItem(GetEncodedString(STR_FORK_NEWS_TAKEOVER, _current_company, f->name), NewsType::Economy, NewsStyle::Normal, {});
		SetWindowClassesDirty(WindowClass::ForkExchange);
		SetWindowClassesDirty(WindowClass::ForkFirms);
		InvalidateWindowData(WindowClass::ForkBusiness, 0);
	}
	return cost;
}

/** Every month shares are worth what the market says, and pay dividends. */
static void ForkSharesMonthly(ForkAsset *a)
{
	if (a->data[0] == HOLDING_LISTED) {
		const uint index = static_cast<uint>(a->data[1]);
		if (index >= FORK_LISTED_COUNT) {
			delete a;
			return;
		}
		a->value = GetForkStockPrice(index) * static_cast<int>(a->data[2]);
		ForkLedgerBook(a->owner, ForkLedgerCategory::Dividends, a->value * FORK_LISTED[index].yield_tenths / 12000);
	} else if (a->data[0] == HOLDING_FIRM) {
		const ForkFirm *f = ForkFirm::GetIfValid(static_cast<ForkFirmID>(a->data[1]));
		if (f == nullptr) {
			delete a;
			return;
		}
		if (Company::IsValidID(f->owner)) {
			/* Another company took the firm over: the shares are bought back at their value. */
			ForkLedgerBook(a->owner, ForkLedgerCategory::Other, GetForkFirmShareValue(f) * static_cast<int>(a->data[2]));
			delete a;
			return;
		}
		a->value = GetForkFirmShareValue(f) * static_cast<int>(a->data[2]);
		if (f->profit_last > 0) ForkLedgerBook(a->owner, ForkLedgerCategory::Dividends, f->profit_last * FIRM_PAYOUT_PERCENT / 100 * static_cast<int>(a->data[2]) / 100);
	}
	SetWindowClassesDirty(WindowClass::ForkExchange);
}

/** Make the monthly update of shares known to the extended economy. */
void RegisterForkStocks()
{
	RegisterForkAssetHandler(ForkAssetType::Shares, ForkSharesMonthly);
}
