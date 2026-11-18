/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_quality.cpp Cargo quality of the fork's extended economy: food spoils, metal rusts, raw materials last forever. */

#include "stdafx.h"

#include "cargotype.h"
#include "fork_economy.h"

#include "safeguards.h"

/** How fast a cargo loses quality while it is on its way. */
enum class ForkPerishClass : uint8_t {
	Immortal, ///< Never loses quality (oil, coal, wood, diamonds, ...).
	Fast, ///< Fresh food and living animals: spoils within weeks.
	Medium, ///< Grain, sweets and other processed food: spoils within months.
	Slow, ///< Manufactured goods and metal: loses value (rust, wear) but never becomes worthless.
};

/** Parameters of a #ForkPerishClass. Times are in cargo aging periods (one period is 185 ticks, about 2.5 days). */
struct ForkPerishParams {
	uint16_t grace; ///< Periods in transit without any loss of quality.
	uint16_t end; ///< Periods in transit after which the quality has dropped to the minimum.
	uint8_t minimum; ///< Lowest quality in percent. Zero means the cargo turns into garbage.
};

static constexpr ForkPerishParams FORK_PERISH_PARAMS[] = {
	{0, 0, 100}, // Immortal
	{6, 24, 0}, // Fast: fresh for about 15 days, garbage after about 60 days.
	{20, 90, 0}, // Medium: fresh for about 50 days, garbage after about 225 days.
	{60, 300, 40}, // Slow: no loss for about 150 days, then down to 40% after about 750 days.
};

/** Cargo labels of the default cargoes (and the common ones of cargo sets) that have a fixed perish class. */
struct ForkPerishLabel {
	CargoLabel label; ///< Label of the cargo.
	ForkPerishClass perish_class; ///< Perish class of the cargo.
};

static constexpr ForkPerishLabel FORK_PERISH_LABELS[] = {
	/* Fresh food and animals. */
	{CT_LIVESTOCK, ForkPerishClass::Fast},
	{CT_FOOD, ForkPerishClass::Fast},
	{CT_FRUIT, ForkPerishClass::Fast},
	{CargoLabel{"MILK"}, ForkPerishClass::Fast},
	{CargoLabel{"FISH"}, ForkPerishClass::Fast},
	{CargoLabel{"MEAT"}, ForkPerishClass::Fast},
	/* Grain and other processed food. */
	{CT_GRAIN, ForkPerishClass::Medium},
	{CT_WHEAT, ForkPerishClass::Medium},
	{CT_MAIZE, ForkPerishClass::Medium},
	{CT_SUGAR, ForkPerishClass::Medium},
	{CT_TOFFEE, ForkPerishClass::Medium},
	{CT_CANDY, ForkPerishClass::Medium},
	{CT_COLA, ForkPerishClass::Medium},
	{CT_COTTON_CANDY, ForkPerishClass::Medium},
	{CT_BUBBLES, ForkPerishClass::Medium},
	{CT_FIZZY_DRINKS, ForkPerishClass::Medium},
	/* Things that rust or wear. */
	{CT_STEEL, ForkPerishClass::Slow},
	{CT_GOODS, ForkPerishClass::Slow},
	{CT_PAPER, ForkPerishClass::Slow},
	{CT_RUBBER, ForkPerishClass::Slow},
	{CT_BATTERIES, ForkPerishClass::Slow},
	{CT_PLASTIC, ForkPerishClass::Slow},
	{CT_TOYS, ForkPerishClass::Slow},
	/* Explicitly timeless. */
	{CT_PASSENGERS, ForkPerishClass::Immortal},
	{CT_MAIL, ForkPerishClass::Immortal},
	{CT_COAL, ForkPerishClass::Immortal},
	{CT_OIL, ForkPerishClass::Immortal},
	{CT_WOOD, ForkPerishClass::Immortal},
	{CT_IRON_ORE, ForkPerishClass::Immortal},
	{CT_COPPER_ORE, ForkPerishClass::Immortal},
	{CT_WATER, ForkPerishClass::Immortal},
	{CT_DIAMONDS, ForkPerishClass::Immortal},
	{CT_GOLD, ForkPerishClass::Immortal},
	{CT_VALUABLES, ForkPerishClass::Immortal},
};

/**
 * Get how fast a cargo loses quality.
 * @param cargo Type of the cargo.
 * @return The perish class of the cargo.
 */
static ForkPerishClass GetPerishClass(CargoType cargo)
{
	const CargoSpec *cs = CargoSpec::Get(cargo);
	if (!cs->IsValid()) return ForkPerishClass::Immortal;

	for (const ForkPerishLabel &entry : FORK_PERISH_LABELS) {
		if (entry.label == cs->label) return entry.perish_class;
	}

	/* Unknown cargo, for example from a NewGRF: decide by the cargo classes. */
	if (cs->classes.Test(CargoClass::Refrigerated)) return ForkPerishClass::Fast;
	if (cs->classes.Test(CargoClass::PieceGoods)) return ForkPerishClass::Slow;
	return ForkPerishClass::Immortal;
}

/**
 * Get the quality of cargo that has been on its way for some time.
 * @param cargo Type of the cargo.
 * @param periods_in_transit Number of cargo aging periods the cargo was in transit.
 * @return Quality in percent: 100 is as good as new, 0 is garbage.
 */
uint GetForkCargoQuality(CargoType cargo, uint periods_in_transit)
{
	const ForkPerishParams &params = FORK_PERISH_PARAMS[static_cast<size_t>(GetPerishClass(cargo))];
	if (params.minimum == 100 || periods_in_transit <= params.grace) return 100;
	if (periods_in_transit >= params.end) return params.minimum;

	const uint span = params.end - params.grace;
	const uint left = params.end - periods_in_transit;
	return params.minimum + (100 - params.minimum) * left / span;
}
