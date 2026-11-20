/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_valuation.h Deterministic, integer-only valuation of companies and firms in the fork economy. */

#ifndef FORK_VALUATION_H
#define FORK_VALUATION_H

#include "economy_type.h"

struct ForkFirm;
struct Town;

/** Everything the sale-price formula needs about a business and the town it operates in. */
struct ForkCompanyValueInput {
	Money assets; ///< Book value of the assets; negative values are treated as zero.
	int64_t monthly_production; ///< Units made last month.
	Money monthly_income; ///< Revenue of last month; negative values give no bonus.
	uint32_t city_population; ///< Population of the town.
	uint8_t city_rating; ///< Standing in the town, 0 .. 100.
};

/**
 * Calculate a sale price from assets, activity and the town.
 *
 * This replaces the old flat sale price, which never reacted to how a business was doing: a busy, well-liked
 * firm in a large town used to fetch exactly the same as an idle one in a hamlet. The assets set the base and
 * two bounded fixed-point factors lift it: activity adds at most 50%, the town at most 15% (10% population,
 * 5% standing), giving a multiplier between 1.00 and 1.725. All of the maths is integer fixed point so that
 * the valuation is identical on every client, and the result is never negative.
 * @param input Assets, monthly activity and town details.
 * @return Sale price, zero or positive.
 */
Money CalculateForkCompanySalePrice(const ForkCompanyValueInput &input);

/**
 * Valuation helpers for a firm of a town.
 * @param firm Firm to value, or nullptr.
 * @param town Town of the firm, or nullptr (then the city bonus is zero).
 * @return Sale value of the firm, zero or positive.
 */
Money GetForkFirmSaleValue(const ForkFirm *firm, const Town *town);

/** @return Population of the town, or zero for a nullptr town. */
uint32_t ForkValueCityPopulation(const Town *town);

/** @return Best company standing in the town on a 0 .. 100 scale, or zero for a nullptr town. */
uint8_t ForkValueCityRating(const Town *town);

#endif /* FORK_VALUATION_H */
