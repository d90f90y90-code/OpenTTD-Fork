/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_economy.h Foundation of the fork's extended economy: market state, business ledger and the asset registry. */

#ifndef FORK_ECONOMY_H
#define FORK_ECONOMY_H

#include "company_type.h"
#include "core/pool_type.hpp"
#include "economy_type.h"
#include "town_type.h"

/**
 * Categories of the business ledger. Every extended-economy feature books its income and costs in one of these,
 * so the finance code never has to change when a feature is added. Some entries are reserved for later features.
 */
enum class ForkLedgerCategory : uint8_t {
	Tax, ///< Local taxes and fees.
	Rent, ///< Rent income and rent costs.
	Salaries, ///< Salaries of staff.
	Dividends, ///< Dividends from shares.
	Advertising, ///< Income from advertising and sponsoring.
	Trade, ///< Trade, customs and shops.
	Leasing, ///< Leasing contracts.
	Other, ///< Anything else.
	End, ///< End marker.
};

/** Things the market state (boom/recession) can influence. Every factor is a percentage, 100 means neutral. */
enum class ForkMarketFactor : uint8_t {
	CargoPrice, ///< Payment for transported cargo.
	Rent, ///< Rents of property.
	StockPrice, ///< Prices of shares.
	Interest, ///< Interest rates.
	End, ///< End marker.
};

/** Kinds of assets a company can own in the extended economy. */
enum class ForkAssetType : uint8_t {
	None, ///< Unused.
	Property, ///< Land and buildings in a town.
	Shop, ///< Retail shops.
	Lease, ///< Leasing contract.
	MiningRight, ///< Right to the output of an industry.
	MediaChannel, ///< Radio or TV channel.
	Shares, ///< Shares of an NPC company.
	End, ///< End marker.
};

/** What the #ForkAsset::location of an asset refers to. */
enum class ForkAssetLocation : uint8_t {
	None, ///< Not tied to a place.
	Town, ///< A #TownID.
	Industry, ///< An #IndustryID.
	Tile, ///< A #TileIndex.
};

static constexpr size_t FORK_LEDGER_CATEGORIES = static_cast<size_t>(ForkLedgerCategory::End);
static constexpr size_t FORK_LEDGER_SIZE = MAX_COMPANIES * FORK_LEDGER_CATEGORIES; ///< One amount per company and category.
static constexpr uint32_t FORK_ECONOMY_DATA_VERSION = 1; ///< Version of the data layout inside the fork economy savegame chunks.

/** Global state of the extended economy. Saved in the 'FKEC' chunk. */
struct ForkEconomyState {
	uint32_t data_version = FORK_ECONOMY_DATA_VERSION; ///< Version of this data, for conversions after the layout changed.
	uint8_t market_phase = 0; ///< Current phase of the economic cycle (0 = normal).
	int32_t market_index = 1000; ///< Strength of the market in permille; 1000 is neutral.
	uint16_t market_months_left = 0; ///< Months until the market phase changes.
	std::array<Money, FORK_LEDGER_SIZE> ledger_month{}; ///< Ledger amounts of the current month.
	std::array<Money, FORK_LEDGER_SIZE> ledger_last{}; ///< Ledger amounts of the previous month.
};

extern ForkEconomyState _fork_economy;

using ForkAssetID = PoolID<uint32_t, struct ForkAssetIDTag, 0xFF0000, 0xFFFFFF>;
struct ForkAsset;
using ForkAssetPool = Pool<ForkAsset, ForkAssetID, 64>;
extern ForkAssetPool _fork_asset_pool;

/** Something a company owns in the extended economy. Features use the generic fields and keep specials in #data. */
struct ForkAsset : ForkAssetPool::PoolItem<&_fork_asset_pool> {
	CompanyID owner = CompanyID::Invalid(); ///< Company that owns the asset.
	uint8_t type = 0; ///< Kind of asset, a #ForkAssetType.
	uint8_t location_kind = 0; ///< What #location refers to, a #ForkAssetLocation.
	uint32_t location = 0; ///< Town, industry or tile the asset is tied to.
	Money purchase_price = 0; ///< What the owner paid.
	Money value = 0; ///< Current value.
	Money monthly_income = 0; ///< Income per month.
	Money monthly_cost = 0; ///< Costs per month.
	uint32_t age_months = 0; ///< Age of the asset.
	std::array<int64_t, 4> data{}; ///< Free for the feature that owns this asset type.

	/**
	 * Construct an asset.
	 * @param index Index within the pool.
	 */
	ForkAsset(ForkAssetID index = ForkAssetID::Invalid()) : ForkAssetPool::PoolItem<&_fork_asset_pool>(index) {}

	/** @return The type of this asset. */
	inline ForkAssetType GetType() const { return static_cast<ForkAssetType>(this->type); }
};

/** Function that a feature registers to run for each of its assets, every month. */
using ForkAssetMonthlyHandler = void (*)(ForkAsset *asset);

void ResetForkEconomy();
void RegisterForkAssetHandler(ForkAssetType type, ForkAssetMonthlyHandler handler);

uint GetForkMarketFactor(ForkMarketFactor factor);
void ForkLedgerBook(CompanyID company, ForkLedgerCategory category, Money amount);
Money GetForkLedger(CompanyID company, ForkLedgerCategory category, bool last_month);
uint GetForkTownSizeLevel(const Town *t);
uint GetForkAssetCount(CompanyID company);

void ShowForkBusinessWindow();

#endif /* FORK_ECONOMY_H */
