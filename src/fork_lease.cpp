/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/**
 * @file fork_lease.cpp Leasing of vehicles.
 *
 * Installment contracts (financial lease): about a quarter is paid at once and the rest in monthly installments over
 * 10 to 24 months. The longer the contract, the more interest. After the last installment the vehicle belongs to the
 * company; repairs are the company's own business all the time.
 *
 * Rental contracts (operating lease): a dealer offers a vehicle for a number of months. The company can ask for a longer
 * contract before signing, for a surcharge on the rent. No down payment. When the contract ends the company can renew it
 * for a rent that is 40% higher, otherwise the vehicle is taken back.
 *
 * A contract is stored as an asset of type Lease: location is the vehicle, data[0] the kind, data[1] the months that are
 * left, data[2] the payment per month and data[3] the duration that a renewal lasts.
 */

#include "stdafx.h"

#include "command_func.h"
#include "company_base.h"
#include "company_func.h"
#include "core/backup_type.hpp"
#include "engine_base.h"
#include "fork_economy.h"
#include "fork_lease.h"
#include "fork_lease_cmd.h"
#include "fork_usedmarket.h"
#include "news_func.h"
#include "order_type.h"
#include "strings_func.h"
#include "timer/timer_game_calendar.h"
#include "vehicle_base.h"
#include "vehicle_cmd.h"
#include "window_func.h"

#include "table/strings.h"

#include "safeguards.h"

static constexpr int64_t LEASE_INSTALLMENT = 1; ///< Installment contract; the vehicle will be owned at the end.
static constexpr int64_t LEASE_RENTAL = 2; ///< Rental contract; the vehicle is not owned.
static constexpr int64_t LEASE_RETURNING = 3; ///< Rental contract that ended; the vehicle is on its way to a depot to be taken back.

/**
 * Terms of an installment contract. The interest grows with the duration, and follows the economic cycle.
 * @param price Price of the vehicle.
 * @param months Duration, limited to 10 to 24 months.
 * @return The terms.
 */
ForkLeaseTerms GetForkInstallmentTerms(Money price, uint months)
{
	ForkLeaseTerms t;
	t.months = std::clamp<uint>(months, FORK_LEASE_MIN_MONTHS, FORK_LEASE_MAX_MONTHS);
	t.price = price;
	t.down = price * FORK_LEASE_DOWN_PERCENT / 100;

	const Money financed = price - t.down;
	t.interest_percent = (3 + t.months / 2) * GetForkMarketFactor(ForkMarketFactor::Interest) / 100;
	t.total = financed + financed * t.interest_percent / 100;
	t.payment = (t.total + (t.months - 1)) / t.months;
	return t;
}

/**
 * Terms of a rental contract. A longer duration than the dealer offers costs a surcharge of 10% to 20% on the rent.
 * @param price Price of the vehicle.
 * @param base_months Duration the dealer offers.
 * @param months Duration the company wants, at least the offered one and at most 24 months.
 * @return The terms.
 */
ForkLeaseTerms GetForkRentalTerms(Money price, uint base_months, uint months)
{
	ForkLeaseTerms t;
	t.price = price;
	t.base_months = base_months;
	t.months = std::clamp<uint>(months, base_months, FORK_LEASE_MAX_MONTHS);

	const uint extra = t.months - base_months;
	t.surcharge_percent = extra == 0 ? 0 : 10 + 10 * std::min<uint>(extra, 12) / 12;

	const Money rent = price * FORK_RENT_MONTHLY_PERMILLE / 1000;
	t.payment = rent * (100 + t.surcharge_percent) / 100;
	t.total = t.payment * t.months;
	return t;
}

/** @return The asset of the contract of a vehicle, or nullptr. */
static ForkAsset *FindLease(CompanyID company, VehicleID vehicle)
{
	for (ForkAsset *a : ForkAsset::Iterate()) {
		if (a->owner == company && a->type == static_cast<uint8_t>(ForkAssetType::Lease) && a->location == vehicle.base()) return a;
	}
	return nullptr;
}

/**
 * Count the contracts of a company.
 * @param company Company to look at.
 * @param[out] installments Number of installment contracts.
 * @param[out] rentals Number of rental contracts.
 * @param[out] monthly What is paid per month for all contracts together.
 */
void GetForkLeaseSummary(CompanyID company, uint &installments, uint &rentals, Money &monthly)
{
	installments = rentals = 0;
	monthly = 0;
	for (const ForkAsset *a : ForkAsset::Iterate()) {
		if (a->owner != company || a->type != static_cast<uint8_t>(ForkAssetType::Lease)) continue;
		if (a->data[0] == LEASE_INSTALLMENT) {
			installments++;
			monthly += a->data[2];
		} else if (a->data[0] == LEASE_RENTAL) {
			rentals++;
			if (a->data[1] > 0) monthly += a->data[2];
		}
	}
}

/**
 * What a renewal of a rental contract would cost.
 * @param vehicle Vehicle of the contract.
 * @param[out] months Duration the renewal lasts.
 * @param[out] old_rent The rent of the contract that ended.
 * @return The new rent per month, zero if the vehicle has no contract that waits for a decision.
 */
Money GetForkLeaseRenewRent(uint vehicle, uint &months, Money &old_rent)
{
	months = 0;
	old_rent = 0;
	const ForkAsset *a = FindLease(_local_company, static_cast<VehicleID>(vehicle));
	if (a == nullptr || a->data[0] != LEASE_RENTAL || a->data[1] > 0) return 0;

	months = static_cast<uint>(a->data[3] & 0xFFFF);
	old_rent = a->data[2];
	return Money(a->data[2]) * FORK_RENT_RENEWAL_PERCENT / 100;
}

/**
 * Take a vehicle on installments or rent.
 * @param flags Type of operation.
 * @param tile Depot of the company where the vehicle is delivered.
 * @param dealer Number of the dealer.
 * @param slot Number of the offer at that dealer.
 * @param kind 1 for installments, 2 for rent.
 * @param months Duration of the contract in months.
 * @return The cost of this operation (the down payment) or an error.
 */
CommandCost CmdLeaseForkVehicle(DoCommandFlags flags, TileIndex tile, uint8_t dealer, uint8_t slot, uint8_t kind, uint8_t months)
{
	if (!Company::IsValidID(_current_company) || dealer >= FORK_DEALER_COUNT || slot >= FORK_OFFERS_PER_DEALER) return CMD_ERROR;
	if (kind != LEASE_INSTALLMENT && kind != LEASE_RENTAL) return CMD_ERROR;

	const std::vector<ForkUsedOffer> offers = GetForkUsedOffers(dealer, _current_company);
	const ForkUsedOffer *offer = nullptr;
	for (const ForkUsedOffer &o : offers) {
		if (o.slot == slot) offer = &o;
	}
	if (offer == nullptr) return CommandCost(STR_ERROR_FORK_UM_NO_OFFER);
	if (offer->sold >= offer->stock) return CommandCost(STR_ERROR_FORK_UM_SOLD_OUT);
	if (!ForkAsset::CanAllocateItem()) return CommandCost(STR_ERROR_FORK_TOO_MANY_ASSETS);

	const uint base_months = kind == LEASE_RENTAL ? GetForkRentalBaseMonths(dealer, slot, _current_company) : 0;
	if (kind == LEASE_INSTALLMENT && (months < FORK_LEASE_MIN_MONTHS || months > FORK_LEASE_MAX_MONTHS)) return CommandCost(STR_ERROR_FORK_LEASE_MONTHS);
	if (kind == LEASE_RENTAL && (months < base_months || months > FORK_LEASE_MAX_MONTHS)) return CommandCost(STR_ERROR_FORK_LEASE_MONTHS);

	/* Build the vehicle like a new one, then change what makes it a used one. */
	CommandCost built;
	VehicleID veh_id{};
	std::tie(built, veh_id, std::ignore, std::ignore, std::ignore) = Command<Commands::BuildVehicle>::Do(flags, tile, offer->engine, false, INVALID_CARGO, ClientID::Invalid);
	if (built.Failed()) return built;

	const Money price = built.GetCost() * offer->price_percent / 100;
	const ForkLeaseTerms terms = kind == LEASE_INSTALLMENT ? GetForkInstallmentTerms(price, months) : GetForkRentalTerms(price, base_months, months);

	/* Only the down payment is paid now, and nothing for a rental contract. */
	CommandCost cost(ExpensesType::NewVehicles, kind == LEASE_INSTALLMENT ? terms.down : Money(0));

	if (flags.Test(DoCommandFlag::Execute)) {
		Vehicle *v = Vehicle::GetIfValid(veh_id);
		if (v != nullptr) {
			const TimerGameCalendar::Date age = TimerGameCalendar::Date(static_cast<int32_t>(offer->age_months) * 365 / 12);
			for (Vehicle *u = v; u != nullptr; u = u->Next()) {
				u->age = age;
				/* A vehicle that is only rented is worth nothing to the company, so selling it earns nothing. */
				u->value = kind == LEASE_INSTALLMENT ? u->value * offer->price_percent / 100 : Money(0);
			}

			ForkAsset *a = ForkAsset::Create();
			a->owner = _current_company;
			a->type = static_cast<uint8_t>(ForkAssetType::Lease);
			a->location_kind = static_cast<uint8_t>(ForkAssetLocation::None);
			a->location = veh_id.base();
			a->data[0] = kind;
			a->data[1] = terms.months;
			a->data[2] = static_cast<int64_t>(terms.payment);
			a->data[3] = kind == LEASE_RENTAL ? terms.months : 0;
			a->purchase_price = terms.down;
			a->value = 0;
			a->monthly_cost = terms.payment;
		}
		ForkUsedMarketNoteSold(_current_company, dealer, slot);
		SetWindowClassesDirty(WindowClass::ForkUsedMarket);
		InvalidateWindowData(WindowClass::ForkBusiness, 0);
	}
	return cost;
}

/** Send a vehicle to a depot, where it stops. */
static void SendBackToDepot(Vehicle *v)
{
	Backup<CompanyID> cur_company(_current_company, v->owner);
	Vehicle *front = v->First();
	if (!front->current_order.IsType(OT_GOTO_DEPOT)) front->SendToDepot({DoCommandFlag::Execute}, {});
	cur_company.Restore();
}

/** Renew a rental contract, or let the vehicle go. */
CommandCost CmdRenewForkLease(DoCommandFlags flags, VehicleID vehicle_id, bool renew)
{
	if (!Company::IsValidID(_current_company)) return CMD_ERROR;

	ForkAsset *a = FindLease(_current_company, vehicle_id);
	if (a == nullptr || a->data[0] != LEASE_RENTAL || a->data[1] > 0) return CommandCost(STR_ERROR_FORK_LEASE_NOT_PENDING);

	if (flags.Test(DoCommandFlag::Execute)) {
		if (renew) {
			a->data[1] = a->data[3] & 0xFFFF;
			a->data[2] = static_cast<int64_t>(Money(a->data[2]) * FORK_RENT_RENEWAL_PERCENT / 100);
			a->monthly_cost = a->data[2];
		} else {
			a->data[0] = LEASE_RETURNING;
			Vehicle *v = Vehicle::GetIfValid(vehicle_id);
			if (v != nullptr) SendBackToDepot(v);
		}
		SetWindowClassesDirty(WindowClass::ForkUsedMarket);
		InvalidateWindowData(WindowClass::ForkBusiness, 0);
	}
	return CommandCost();
}

/** Every month the payments of a contract are made and contracts that are over are dealt with. */
static void ForkLeaseMonthly(ForkAsset *a)
{
	Vehicle *v = Vehicle::GetIfValid(static_cast<VehicleID>(a->location));
	const int64_t kind = a->data[0];
	const Money payment = a->data[2];

	if (v == nullptr || v->owner != a->owner) {
		/* The vehicle is gone, so the contract is settled: installments are paid at once, a rental costs half of what is left. */
		if (kind == LEASE_INSTALLMENT) ForkLedgerBook(a->owner, ForkLedgerCategory::Leasing, -(payment * a->data[1]));
		if (kind == LEASE_RENTAL) ForkLedgerBook(a->owner, ForkLedgerCategory::Leasing, -(payment * a->data[1] / 2));
		delete a;
		return;
	}

	if (kind == LEASE_INSTALLMENT) {
		ForkLedgerBook(a->owner, ForkLedgerCategory::Leasing, -payment);
		a->monthly_cost = payment;
		a->data[1]--;
		if (a->data[1] <= 0) {
			if (a->owner == _local_company) {
				AddNewsItem(GetEncodedString(STR_FORK_NEWS_LEASE_OWNED, PackEngineNameDParam(v->engine_type, EngineNameContext::Generic)), NewsType::Economy, NewsStyle::Normal, {});
			}
			delete a; // The vehicle belongs to the company now.
		}
	} else if (kind == LEASE_RENTAL) {
		if (a->data[1] <= 0) {
			/* There was no decision about a renewal, so the vehicle is taken back. */
			a->data[0] = LEASE_RETURNING;
			SendBackToDepot(v);
			if (a->owner == _local_company) {
				AddNewsItem(GetEncodedString(STR_FORK_NEWS_LEASE_RETURNED, PackEngineNameDParam(v->engine_type, EngineNameContext::Generic)), NewsType::Economy, NewsStyle::Normal, {});
			}
			return;
		}

		ForkLedgerBook(a->owner, ForkLedgerCategory::Leasing, -payment);
		a->monthly_cost = payment;
		a->data[1]--;
		if (a->data[1] <= 0 && a->owner == _local_company) ShowForkLeaseRenewWindow(v->index.base());
	} else {
		/* Taken back: the vehicle is on its way to a depot, and is gone when it stops there. */
		a->monthly_cost = 0;
		if (v->IsStoppedInDepot()) {
			Backup<CompanyID> cur_company(_current_company, a->owner);
			Command<Commands::SellVehicle>::Do({DoCommandFlag::Execute}, v->index, false, false, ClientID::Invalid);
			cur_company.Restore();
			delete a;
			return;
		}
		SendBackToDepot(v);
	}
	SetWindowClassesDirty(WindowClass::ForkUsedMarket);
}

/** Make the monthly update of contracts known to the extended economy. */
void RegisterForkLease()
{
	RegisterForkAssetHandler(ForkAssetType::Lease, ForkLeaseMonthly);
}
