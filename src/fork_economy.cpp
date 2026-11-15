/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_economy.cpp Foundation of the fork's extended economy: market state, business ledger and the asset registry. */

#include "stdafx.h"

#include "company_base.h"
#include "company_func.h"
#include "core/pool_func.hpp"
#include "economy_func.h"
#include "core/random_func.hpp"
#include "fork_economy.h"
#include "fork_usedmarket.h"
#include "news_func.h"
#include "strings_func.h"
#include "timer/timer.h"
#include "timer/timer_game_economy.h"
#include "town.h"
#include "window_func.h"

#include "table/strings.h"

#include "safeguards.h"

ForkEconomyState _fork_economy;
ForkAssetPool _fork_asset_pool("ForkAsset");
INSTANTIATE_POOL_METHODS(ForkAsset)

/** Monthly handlers of the asset types, set by the features. */
static std::array<ForkAssetMonthlyHandler, static_cast<size_t>(ForkAssetType::End)> _fork_asset_handlers{};

/** Reset the extended economy to its initial state, for a new game or before loading a savegame. */
void ResetForkEconomy()
{
	_fork_economy = {};
	RegisterForkRealEstate();
	RegisterForkShops();
	RegisterForkLabor();
}

/**
 * Register what happens to assets of a type every month.
 * @param type Type of asset.
 * @param handler Function to call for each asset of the type.
 */
void RegisterForkAssetHandler(ForkAssetType type, ForkAssetMonthlyHandler handler)
{
	_fork_asset_handlers[static_cast<size_t>(type)] = handler;
}

/**
 * Get the effect of the market (boom/recession) on something.
 * @param factor What to look at.
 * @return Percentage, where 100 is neutral.
 */
uint GetForkMarketFactor(ForkMarketFactor factor)
{
	/* Distance of the market from neutral, in permille. Each factor reacts with its own strength. */
	const int32_t delta = _fork_economy.market_index - 1000;
	int32_t percent = 100;
	switch (factor) {
		case ForkMarketFactor::CargoPrice: percent += delta / 20; break; // +-12% in the extremes.
		case ForkMarketFactor::Rent: percent += delta / 10; break; // +-25%
		case ForkMarketFactor::StockPrice: percent += delta / 5; break; // +-50%
		case ForkMarketFactor::Interest: percent += delta / 10; break; // +-25%
		case ForkMarketFactor::Consumption: percent += delta / 10; break; // +-25%
		case ForkMarketFactor::Wages: percent += delta / 15; break; // +-17%
		default: break;
	}
	return static_cast<uint>(std::max<int32_t>(10, percent));
}

/** @return Index into the ledger arrays, or SIZE_MAX if the company or category is invalid. */
static size_t LedgerIndex(CompanyID company, ForkLedgerCategory category)
{
	if (!Company::IsValidID(company) || category >= ForkLedgerCategory::End) return SIZE_MAX;
	return static_cast<size_t>(company.base()) * FORK_LEDGER_CATEGORIES + static_cast<size_t>(category);
}

/**
 * Book income or costs of the extended economy: the money moves to or from the company and the ledger remembers it.
 * @param company Company that gets or pays the money.
 * @param category Ledger category to book in.
 * @param amount Positive for income, negative for costs.
 */
void ForkLedgerBook(CompanyID company, ForkLedgerCategory category, Money amount)
{
	size_t index = LedgerIndex(company, category);
	if (index == SIZE_MAX || amount == 0) return;

	_fork_economy.ledger_month[index] += amount;
	SubtractMoneyFromCompany(company, CommandCost(ExpensesType::Other, -amount));
	InvalidateWindowData(WindowClass::ForkBusiness, 0);
}

/**
 * Get what a company booked in a ledger category.
 * @param company Company to look at.
 * @param category Ledger category.
 * @param last_month Use the previous month instead of the current one.
 * @return The booked amount, positive for income.
 */
Money GetForkLedger(CompanyID company, ForkLedgerCategory category, bool last_month)
{
	size_t index = LedgerIndex(company, category);
	if (index == SIZE_MAX) return 0;
	return last_month ? _fork_economy.ledger_last[index] : _fork_economy.ledger_month[index];
}

/**
 * How big a town is, as input for features that depend on the size of a town (taxes, rents, shops).
 * @param t Town to look at.
 * @return Level between 0 (hamlet) and 100 (metropolis), derived from the population.
 */
uint GetForkTownSizeLevel(const Town *t)
{
	return std::min<uint>(100, t->cache.population / 100);
}

/**
 * Count the assets of a company.
 * @param company Owner to count for.
 * @return Number of assets.
 */
uint GetForkAssetCount(CompanyID company)
{
	uint count = 0;
	for (const ForkAsset *a : ForkAsset::Iterate()) {
		if (a->owner == company) count++;
	}
	return count;
}

/** Move the economic cycle one month forward: pick a new phase when the old one ran out, and drift towards its level. */
static void UpdateMarketCycle()
{
	ForkEconomyState &s = _fork_economy;
	const ForkMarketPhase old_phase = static_cast<ForkMarketPhase>(s.market_phase);

	if (s.market_months_left > 0) s.market_months_left--;
	if (s.market_months_left == 0) {
		const uint r = RandomRange(100);
		ForkMarketPhase phase;
		switch (old_phase) {
			case ForkMarketPhase::Normal: phase = r < 35 ? ForkMarketPhase::Boom : (r < 70 ? ForkMarketPhase::Recession : ForkMarketPhase::Normal); break;
			case ForkMarketPhase::Boom: phase = r < 60 ? ForkMarketPhase::Normal : ForkMarketPhase::Recession; break;
			default: phase = r < 70 ? ForkMarketPhase::Normal : ForkMarketPhase::Boom; break;
		}

		switch (phase) {
			case ForkMarketPhase::Boom:
				s.market_months_left = 8 + RandomRange(17);
				s.market_target = 1120 + RandomRange(131);
				break;
			case ForkMarketPhase::Recession:
				s.market_months_left = 6 + RandomRange(13);
				s.market_target = 780 + RandomRange(111);
				break;
			default:
				s.market_months_left = 12 + RandomRange(25);
				s.market_target = 980 + RandomRange(41);
				break;
		}
		s.market_phase = static_cast<uint8_t>(phase);

		if (phase != old_phase) {
			StringID news = phase == ForkMarketPhase::Boom ? STR_FORK_NEWS_BOOM : (phase == ForkMarketPhase::Recession ? STR_FORK_NEWS_RECESSION : STR_FORK_NEWS_STABLE);
			AddNewsItem(GetEncodedString(news), NewsType::Economy, NewsStyle::Normal, {});
		}
	}

	/* The market changes gradually, never more than 3% per month. */
	s.market_index += std::clamp<int32_t>(s.market_target - s.market_index, -30, 30);
}

/**
 * A company goes away or is taken over: its assets are passed on to the new owner, or removed when there is none.
 * @param old_owner Company that disappears.
 * @param new_owner Company that takes over, or an invalid owner.
 */
void TransferForkAssets(Owner old_owner, Owner new_owner)
{
	for (ForkAsset *a : ForkAsset::Iterate()) {
		if (a->owner != old_owner) continue;
		if (Company::IsValidID(new_owner) && new_owner != old_owner) {
			a->owner = new_owner;
		} else {
			delete a;
		}
	}

	/* Clear the ledger of the company, so a new company that gets the same number starts fresh. */
	for (size_t c = 0; c < FORK_LEDGER_CATEGORIES; c++) {
		size_t index = static_cast<size_t>(old_owner.base()) * FORK_LEDGER_CATEGORIES + c;
		if (index >= FORK_LEDGER_SIZE) break;
		_fork_economy.ledger_month[index] = 0;
		_fork_economy.ledger_last[index] = 0;
	}
}

/** Everything the extended economy does at the start of a month. */
static void ForkEconomyMonthly()
{
	UpdateMarketCycle();

	_fork_economy.ledger_last = _fork_economy.ledger_month;
	_fork_economy.ledger_month.fill(0);

	ChargeForkMunicipalTaxes();
	ForkUsedMarketMonthly();

	for (ForkAsset *a : ForkAsset::Iterate()) {
		a->age_months++;
		ForkAssetMonthlyHandler handler = _fork_asset_handlers[a->type < static_cast<uint8_t>(ForkAssetType::End) ? a->type : 0];
		if (handler != nullptr) handler(a);
	}

	InvalidateWindowData(WindowClass::ForkBusiness, 0);
}

static const IntervalTimer<TimerGameEconomy> _fork_economy_monthly({TimerGameEconomy::Trigger::Month, TimerGameEconomy::Priority::Fork}, [](auto)
{
	ForkEconomyMonthly();
});
