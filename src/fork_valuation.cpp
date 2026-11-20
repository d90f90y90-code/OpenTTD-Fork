/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_valuation.cpp Integer fixed-point valuation of companies and firms in the fork economy. */

#include "stdafx.h"

#include <algorithm>
#include <limits>

#include "company_base.h"
#include "core/math_func.hpp"
#include "fork_firm.h"
#include "fork_valuation.h"
#include "town.h"
#include "town_type.h"

#include "safeguards.h"

static constexpr int64_t FORK_VALUE_SCALE = 10000; ///< Ten-thousandths of a multiplier; 10000 is 1.0.
static constexpr uint32_t FORK_VALUE_BASE_POPULATION = 1000; ///< Population at which the population bonus starts.
static constexpr int64_t FORK_LN10_BASIS = 23026; ///< ln(10) in ten-thousandths, for log10 -> log conversion.

/* log10(1 .. 10) in ten-thousandths of a decade; the extra element is the end of the last interval. */
static constexpr int64_t FORK_LOG10_LEADING[] = {0, 3010, 4771, 6020, 6990, 7781, 8451, 9030, 9542, 10000, 10000};

/**
 * Approximate log10(value) as ten-thousandths of a decade with integer maths only.
 * Two significant digits are kept, with linear interpolation inside the leading digit's interval.
 * @param value Positive number to take the logarithm of.
 * @return log10(value) * 10000, rounded down; zero for zero.
 */
static int64_t ForkLog10Basis(uint64_t value)
{
	if (value == 0) return 0;

	int64_t decade = 2; // Three significant digits (100 .. 999) give log10 2.xxx.
	while (value < 100) {
		value *= 10;
		decade--;
	}
	while (value >= 1000) {
		value /= 10;
		decade++;
	}

	const int64_t digit = static_cast<int64_t>(value / 100); // 1 .. 9.
	const int64_t fraction = FORK_LOG10_LEADING[digit] +
			(FORK_LOG10_LEADING[digit + 1] - FORK_LOG10_LEADING[digit]) * static_cast<int64_t>(value % 100) / 100;
	return decade * FORK_VALUE_SCALE + fraction;
}

/**
 * Approximate log(1 + value) in ten-thousandths, using log10 and ln(10).
 * @param value Activity of a month; non-positive values give zero.
 * @return log(1 + value) * 10000, zero for non-positive input.
 */
static int64_t ForkLog1pBasis(int64_t value)
{
	if (value <= 0) return 0;
	const uint64_t shifted = static_cast<uint64_t>(value) + 1; // Safe even at INT64_MAX.
	return ForkLog10Basis(shifted) * FORK_LN10_BASIS / FORK_VALUE_SCALE;
}

/**
 * Multiply a non-negative amount by a fixed-point factor without overflowing.
 * @param value Amount to scale.
 * @param factor Factor in ten-thousandths (10000 == 1.0).
 * @return Scaled amount, capped at the largest representable value.
 */
static int64_t ForkScaleByBasis(int64_t value, int64_t factor)
{
	if (value <= 0 || factor <= 0) return 0;

	const int64_t maximum = std::numeric_limits<int64_t>::max();
	const int64_t whole = value / FORK_VALUE_SCALE;
	const int64_t extra = (value % FORK_VALUE_SCALE) * factor / FORK_VALUE_SCALE; // Small: factor is at most 15000.
	if (whole > (maximum - extra) / factor) return maximum;
	return whole * factor + extra;
}

Money CalculateForkCompanySalePrice(const ForkCompanyValueInput &input)
{
	const int64_t assets = std::max<int64_t>(input.assets.base(), 0);
	if (assets == 0) return Money(0);

	/* log10(population / 1000), in ten-thousandths of a decade, at most +10%. */
	const uint64_t population = std::max<uint32_t>(input.city_population, FORK_VALUE_BASE_POPULATION);
	const int64_t population_log = ForkLog10Basis(population) - 3 * FORK_VALUE_SCALE;
	const int64_t population_bonus = Clamp<int64_t>(population_log * 400 / FORK_VALUE_SCALE, 0, 1000);
	const int64_t rating_bonus = Clamp<int64_t>(static_cast<int64_t>(input.city_rating) * 5, 0, 500); // At most +5%.
	const int64_t city_factor = FORK_VALUE_SCALE + population_bonus + rating_bonus; // 1.000 .. 1.150.

	/* log(1 + production) / 10 and log(1 + income) / 20, in ten-thousandths, at most +50% together. */
	const int64_t production_bonus = ForkLog1pBasis(input.monthly_production) / 10;
	const int64_t income_bonus = ForkLog1pBasis(input.monthly_income.base()) / 20;
	const int64_t activity_factor = FORK_VALUE_SCALE + Clamp<int64_t>(production_bonus + income_bonus, 0, 5000);

	int64_t price = ForkScaleByBasis(assets, activity_factor);
	price = ForkScaleByBasis(price, city_factor);
	return Money(price);
}

uint32_t ForkValueCityPopulation(const Town *town)
{
	return town != nullptr ? town->cache.population : 0;
}

uint8_t ForkValueCityRating(const Town *town)
{
	if (town == nullptr) return 0;

	int best = 0;
	for (const Company *c : Company::Iterate()) {
		if (!town->have_ratings.Test(c->index)) continue;
		const int rating = Clamp<int>(town->ratings[c->index], RATING_MINIMUM, RATING_MAXIMUM);
		best = std::max(best, (rating - RATING_MINIMUM) / 20); // -1000 .. 1000 becomes 0 .. 100.
	}
	return static_cast<uint8_t>(best);
}

Money GetForkFirmSaleValue(const ForkFirm *firm, const Town *town)
{
	if (firm == nullptr) return Money(0);

	ForkCompanyValueInput input;
	input.assets = std::max<Money>(Money(0), firm->value);
	input.monthly_production = static_cast<int64_t>(firm->made_last);
	input.monthly_income = std::max<Money>(Money(0), firm->revenue_last);
	input.city_population = ForkValueCityPopulation(town);
	input.city_rating = ForkValueCityRating(town);
	return CalculateForkCompanySalePrice(input);
}
