/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_usedmarket.h Market of used vehicles, run by five dealers. */

#ifndef FORK_USEDMARKET_H
#define FORK_USEDMARKET_H

#include "company_type.h"
#include "economy_type.h"
#include "engine_type.h"

/** An offer of a dealer: a used vehicle for a price. */
struct ForkUsedOffer {
	uint8_t slot = 0; ///< Number of the offer at the dealer.
	EngineID engine = EngineID::Invalid(); ///< Model that is offered.
	uint age_months = 0; ///< How long the vehicle has been in use.
	int price_percent = 100; ///< Price in percent of the price of a new vehicle.
	int deal = 0; ///< -1 for a bargain, 1 for a price that is a bit too high, 0 otherwise.
	uint stock = 1; ///< Number of vehicles that are offered.
	uint sold = 0; ///< Number of vehicles of this offer that the company already bought this month.
	Money new_price = 0; ///< Price of a new vehicle of this model.
	Money price = 0; ///< Price asked for the used vehicle.
};

std::vector<ForkUsedOffer> GetForkUsedOffers(uint dealer, CompanyID company);
void ForkUsedMarketMonthly();
void ForkUsedMarketNoteSold(CompanyID company, uint dealer, uint slot);
void ShowForkUsedMarketWindow();

#endif /* FORK_USEDMARKET_H */
