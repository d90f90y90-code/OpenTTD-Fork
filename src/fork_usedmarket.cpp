/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/**
 * @file fork_usedmarket.cpp Market of used vehicles.
 *
 * Three dealers sell vehicles that have been in use for a while: TTD MAX sells trains, TAJCAR road vehicles
 * and LIMANO vehicles of every kind. Each month every dealer shows a few offers. The price depends on the age
 * of the vehicle and on how old the model is, and every offer has a bit of luck in it: some are bargains and
 * some are a little too expensive. The offers follow from the date, so nothing but the number of vehicles
 * that were bought has to be saved.
 */

#include "stdafx.h"

#include "command_func.h"
#include "company_base.h"
#include "company_func.h"
#include "engine_base.h"
#include "engine_func.h"
#include "fork_economy.h"
#include "fork_usedmarket.h"
#include "fork_usedmarket_cmd.h"
#include "timer/timer_game_calendar.h"
#include "timer/timer_game_economy.h"
#include "vehicle_base.h"
#include "vehicle_cmd.h"
#include "window_func.h"

#include "table/strings.h"

#include "safeguards.h"

static constexpr uint8_t _dealer_vehicle_types[FORK_DEALER_COUNT] = {0b0001, 0b0010, 0b1111}; ///< Per dealer the vehicle types it sells, one bit per #VehicleType.
static constexpr int _dealer_bias[FORK_DEALER_COUNT] = {-1, 0, 2}; ///< Per dealer a shift of the luck: TTD MAX is a bit cheaper and LIMANO a bit dearer.

static uint32_t Mix(uint32_t h)
{
	h ^= h >> 16;
	h *= 0x7FEB352DU;
	h ^= h >> 15;
	h *= 0x846CA68BU;
	h ^= h >> 16;
	return h;
}

/** @return Number of the current month, counted from year 0. */
static uint32_t GetMonthIndex()
{
	return static_cast<uint32_t>(TimerGameEconomy::year.base()) * 12 + TimerGameEconomy::month;
}

/** @return Index in the list of bought vehicles for an offer. */
static size_t GetSoldIndex(CompanyID company, uint dealer, uint slot)
{
	return static_cast<size_t>(company.base()) * FORK_USED_SLOTS + dealer * FORK_OFFERS_PER_DEALER + slot;
}

/**
 * Make the offers of a dealer for a company. The offers are the same all month and depend only on the date,
 * the dealer and the models the company can build.
 * @param dealer Number of the dealer: 0 TTD MAX, 1 TAJCAR, 2 LIMANO.
 * @param company Company that looks at the offers.
 * @return The offers; empty when the dealer has nothing to offer.
 */
std::vector<ForkUsedOffer> GetForkUsedOffers(uint dealer, CompanyID company)
{
	std::vector<ForkUsedOffer> offers;
	if (dealer >= FORK_DEALER_COUNT || !Company::IsValidID(company)) return offers;

	std::vector<const Engine *> candidates;
	for (uint vt = 0; vt < 4; vt++) {
		if (((_dealer_vehicle_types[dealer] >> vt) & 1) == 0) continue;
		const VehicleType type = static_cast<VehicleType>(vt);
		for (const Engine *e : Engine::IterateType(type)) {
			/* Only models that have been on the market for at least a year can be sold second hand. */
			if (e->age < 12 || !IsEngineBuildable(e->index, type, company)) continue;
			candidates.push_back(e);
		}
	}
	if (candidates.empty()) return offers;

	const uint32_t month = GetMonthIndex();
	for (uint slot = 0; slot < FORK_OFFERS_PER_DEALER; slot++) {
		const uint32_t h = Mix(Mix(month * 31 + dealer) * 131 + slot * 7919 + company.base());
		const Engine *e = candidates[Mix(h) % candidates.size()];
		const int model_age = e->age;

		/* The vehicle is between half a year and six years old, and never older than the model. */
		const int age = std::min<int>(6 + static_cast<int>(Mix(h + 1) % 67), model_age - 3);
		/* Luck: from a bargain to a little too dear. */
		const int noise = static_cast<int>(Mix(h + 2) % 15) - 9 + _dealer_bias[dealer];
		/* An old model sells for less. */
		const int obsolete = std::clamp((model_age - 120) / 12 * 3 / 2, 0, 15);

		ForkUsedOffer offer;
		offer.slot = static_cast<uint8_t>(slot);
		offer.engine = e->index;
		offer.age_months = static_cast<uint>(age);
		offer.price_percent = std::clamp(100 - age * 7 / 12 - obsolete + noise, 35, 100);
		offer.deal = noise <= -5 ? -1 : (noise >= 3 ? 1 : 0);
		offer.stock = 1 + Mix(h + 3) % 3;
		offer.sold = _fork_economy.used_month == static_cast<int32_t>(month) ? _fork_economy.used_sold[GetSoldIndex(company, dealer, slot)] : 0;
		offer.new_price = e->GetCost();
		offer.price = offer.new_price * offer.price_percent / 100;
		offers.push_back(offer);
	}
	return offers;
}

/** A new month: the dealers have new offers. */
void ForkUsedMarketMonthly()
{
	_fork_economy.used_month = static_cast<int32_t>(GetMonthIndex());
	_fork_economy.used_sold.fill(0);
	SetWindowClassesDirty(WindowClass::ForkUsedMarket);
}

/**
 * Buy a used vehicle of a dealer. The vehicle is delivered to a depot.
 * @param flags Type of operation.
 * @param tile Depot of the company where the vehicle is delivered.
 * @param dealer Number of the dealer.
 * @param slot Number of the offer at that dealer.
 * @return The cost of this operation or an error.
 */
CommandCost CmdBuyUsedVehicle(DoCommandFlags flags, TileIndex tile, uint8_t dealer, uint8_t slot)
{
	if (!Company::IsValidID(_current_company) || dealer >= FORK_DEALER_COUNT || slot >= FORK_OFFERS_PER_DEALER) return CMD_ERROR;

	const std::vector<ForkUsedOffer> offers = GetForkUsedOffers(dealer, _current_company);
	const ForkUsedOffer *offer = nullptr;
	for (const ForkUsedOffer &o : offers) {
		if (o.slot == slot) offer = &o;
	}
	if (offer == nullptr) return CommandCost(STR_ERROR_FORK_UM_NO_OFFER);
	if (offer->sold >= offer->stock) return CommandCost(STR_ERROR_FORK_UM_SOLD_OUT);

	/* Build the vehicle like a new one, then change what makes it a used one. */
	CommandCost built;
	VehicleID veh_id{};
	std::tie(built, veh_id, std::ignore, std::ignore, std::ignore) = Command<Commands::BuildVehicle>::Do(flags, tile, offer->engine, false, INVALID_CARGO, ClientID::Invalid);
	if (built.Failed()) return built;

	CommandCost cost(ExpensesType::NewVehicles, built.GetCost() * offer->price_percent / 100);

	if (flags.Test(DoCommandFlag::Execute)) {
		Vehicle *v = Vehicle::GetIfValid(veh_id);
		if (v != nullptr) {
			const TimerGameCalendar::Date age = TimerGameCalendar::Date(static_cast<int32_t>(offer->age_months) * 365 / 12);
			for (Vehicle *u = v; u != nullptr; u = u->Next()) {
				u->age = age;
				/* It is worth what was paid, so it can't be sold for more right away. */
				u->value = u->value * offer->price_percent / 100;
			}
		}
		_fork_economy.used_sold[GetSoldIndex(_current_company, dealer, slot)]++;
		SetWindowClassesDirty(WindowClass::ForkUsedMarket);
		InvalidateWindowData(WindowClass::ForkBusiness, 0);
	}
	return cost;
}
