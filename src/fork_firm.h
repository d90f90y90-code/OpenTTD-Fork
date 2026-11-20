/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_firm.h Companies of the town economy: technology firms that make products and compete on a market. */

#ifndef FORK_FIRM_H
#define FORK_FIRM_H

#include "company_type.h"
#include "core/pool_type.hpp"
#include "economy_type.h"
#include "town_type.h"

/** Products a technology firm can make. */
enum class ForkProduct : uint8_t {
	Dishwasher, ///< Dishwashers.
	WashingMachine, ///< Washing machines.
	LawnMower, ///< Lawn mowers.
	Lamps, ///< Light bulbs and lamps.
	Television, ///< Television sets.
	Radio, ///< Radios.
	End, ///< End marker.
};

static constexpr size_t FORK_PRODUCT_COUNT = static_cast<size_t>(ForkProduct::End);
static constexpr uint FORK_FIRM_MIN_POPULATION = 1500; ///< A town needs this many inhabitants before a firm can be founded.
static constexpr uint FORK_FIRM_MAX_PER_TOWN = 4; ///< Never more firms than this in one town, all owners together.
static constexpr uint FORK_FIRM_MAX_QUALITY = 5; ///< Highest quality level.

using ForkFirmID = PoolID<uint32_t, struct ForkFirmIDTag, 0xFF0000, 0xFFFFFF>;
struct ForkFirm;
using ForkFirmPool = Pool<ForkFirm, ForkFirmID, 32>;
extern ForkFirmPool _fork_firm_pool;

/** A firm in a town. It belongs to a company of the player, or to someone in the town (then #owner is invalid). */
struct ForkFirm : ForkFirmPool::PoolItem<&_fork_firm_pool> {
	std::string name; ///< Name of the firm.
	CompanyID owner = CompanyID::Invalid(); ///< Company that owns the firm; invalid for firms of the town.
	uint32_t town = 0; ///< Town the firm is in.
	uint8_t product = 0; ///< What the firm makes, a #ForkProduct.
	uint8_t quality = 3; ///< Quality of the product, 1 to 5.
	uint8_t rate = 60; ///< How much is made, in percent of what the firm can make.
	uint8_t price_percent = 100; ///< Price of the product, in percent of the base price.
	uint8_t discount = 0; ///< Discount in percent, on top of the price.
	int8_t personality = 0; ///< For firms of the town: how much more or less than the market they ask.
	uint32_t stock = 0; ///< Number of units in the warehouse.
	uint32_t made_last = 0; ///< Units made last month.
	uint32_t sold_last = 0; ///< Units sold last month.
	uint32_t age_months = 0; ///< How long the firm exists.
	uint32_t hq_tile = 0; ///< Northern tile of the 2x2 headquarters block on the map, 0 if it is not built.
	uint32_t lab_tile = 0; ///< Northern tile of the 2x2 laboratory block on the map, 0 if it is not built.
	Money revenue_last = 0; ///< What the sales brought last month.
	Money profit_last = 0; ///< Profit of last month.
	Money profit_average = 0; ///< Average profit per month, with recent months counting more.
	Money value = 0; ///< What the firm is worth.

	/**
	 * Construct a firm.
	 * @param index Index within the pool.
	 */
	ForkFirm(ForkFirmID index = ForkFirmID::Invalid()) : ForkFirmPool::PoolItem<&_fork_firm_pool>(index) {}

	/** @return The town of this firm. */
	inline TownID GetTown() const { return static_cast<TownID>(this->town); }
};

Money GetForkProductPrice(uint product);
Money GetForkProductCost(uint product, uint quality);
Money GetForkFirmFoundingCost();
uint GetForkFirmCapacity(const ForkFirm *f);
uint GetForkFirmsInTown(TownID town);
uint GetForkCompanyFirms(CompanyID company, TownID town);
Money GetForkFirmSellingPrice(const ForkFirm *f);

void ForkFirmsMonthly();
void ShowForkFirmsWindow(uint town);
void ShowForkFirmWindow(uint firm);
void ShowForkFirmCreateWindow(uint town);

#endif /* FORK_FIRM_H */
