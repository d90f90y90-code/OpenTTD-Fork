/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_lease.h Leasing of vehicles: installment contracts (financial lease) and rental contracts (operating lease). */

#ifndef FORK_LEASE_H
#define FORK_LEASE_H

#include "company_type.h"
#include "economy_type.h"
#include "vehicle_type.h"

static constexpr uint FORK_LEASE_MIN_MONTHS = 10; ///< Shortest installment contract.
static constexpr uint FORK_LEASE_MAX_MONTHS = 24; ///< Longest contract of any kind.
static constexpr uint FORK_LEASE_DOWN_PERCENT = 25; ///< Down payment of an installment contract, in percent of the price.
static constexpr uint FORK_RENT_MONTHLY_PERMILLE = 45; ///< Monthly rent in permille of the price of the vehicle.
static constexpr uint FORK_RENT_RENEWAL_PERCENT = 140; ///< The rent after a renewal, in percent of the old rent.

/** The terms of a lease contract. */
struct ForkLeaseTerms {
	Money price = 0; ///< Price of the vehicle.
	Money down = 0; ///< Down payment, paid when the contract is signed.
	Money payment = 0; ///< Payment per month.
	Money total = 0; ///< Everything that is paid after the down payment.
	uint months = 0; ///< Duration of the contract in months.
	uint interest_percent = 0; ///< Interest on the financed part of an installment contract.
	uint base_months = 0; ///< Duration the dealer offers for a rental contract.
	uint surcharge_percent = 0; ///< Surcharge on the rent for a longer duration than offered.
};

ForkLeaseTerms GetForkInstallmentTerms(Money price, uint months);
ForkLeaseTerms GetForkRentalTerms(Money price, uint base_months, uint months);
uint GetForkRentalBaseMonths(uint dealer, uint slot, CompanyID company);
void GetForkLeaseSummary(CompanyID company, uint &installments, uint &rentals, Money &monthly);
void RegisterForkLease();
void ShowForkLeaseRenewWindow(uint vehicle);
Money GetForkLeaseRenewRent(uint vehicle, uint &months, Money &old_rent);

#endif /* FORK_LEASE_H */
