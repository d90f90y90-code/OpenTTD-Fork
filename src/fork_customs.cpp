/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_customs.cpp Customs of the fork's extended economy: goods that cross from one town to another pay customs duty to the destination town. */

#include "stdafx.h"

#include "cargotype.h"
#include "fork_economy.h"
#include "industry.h"
#include "map_func.h"
#include "station_base.h"
#include "town.h"

#include "safeguards.h"

static constexpr uint FORK_CUSTOMS_MIN_PERCENT = 4; ///< Lowest customs duty of a town.
static constexpr uint FORK_CUSTOMS_RATES = 12; ///< Number of different customs rates, from 4% up to and including 15%.

/**
 * Customs duty a town asks for goods that arrive from another town. Every town has its own rate,
 * between 4% and 15%, that stays the same for the whole game.
 * @param t Town to look at.
 * @return Customs duty in percent.
 */
uint GetForkCustomsRate(const Town *t)
{
	uint32_t h = TileX(t->xy) * 73856093U ^ TileY(t->xy) * 19349663U;
	h ^= h >> 13;
	h *= 0x5bd1e995U;
	h ^= h >> 15;
	return FORK_CUSTOMS_MIN_PERCENT + h % FORK_CUSTOMS_RATES;
}

/**
 * Get the town that an industry or a town source of cargo belongs to.
 * @param source Where the cargo came from.
 * @return The town, or nullptr if the cargo does not come from a town or industry.
 */
const Town *GetForkSourceTown(const Source &source)
{
	switch (source.type) {
		case SourceType::Industry: {
			const Industry *i = Industry::GetIfValid(source.ToIndustryID());
			return i != nullptr ? i->town : nullptr;
		}

		case SourceType::Town:
			return Town::GetIfValid(source.ToTownID());

		default:
			return nullptr;
	}
}

/**
 * Work out the customs duty of cargo that was delivered. Goods from the industry or town of one town that are
 * delivered at a station of another town pay customs to the destination town: a percentage of the payment.
 * Passengers and mail are people and letters, not goods, so they cross the border without duty.
 * @param cargo Type of the cargo.
 * @param source Where the cargo came from.
 * @param destination Station the cargo was delivered at.
 * @param payment What the cargo paid.
 * @return The duty, zero if none has to be paid.
 */
Money GetForkCustomsDuty(CargoType cargo, const Source &source, StationID destination, Money payment)
{
	if (payment <= 0) return 0;

	const CargoSpec *cs = CargoSpec::Get(cargo);
	if (!cs->IsValid() || cs->classes.Test(CargoClass::Passengers) || cs->classes.Test(CargoClass::Mail)) return 0;

	const Town *from = GetForkSourceTown(source);
	const Station *st = Station::GetIfValid(destination);
	if (from == nullptr || st == nullptr || st->town == nullptr || st->town == from) return 0;

	return payment * GetForkCustomsRate(st->town) / 100;
}
