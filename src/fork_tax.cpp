/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_tax.cpp Local taxes of towns on the stations and assets of companies. */

#include "stdafx.h"

#include "company_func.h"
#include "economy_func.h"
#include "fork_economy.h"
#include "station_base.h"
#include "town.h"

#include "safeguards.h"

/**
 * Tax rate a town charges, in permille. Bigger towns charge more, from 2.0% in a hamlet to 12.0% in a metropolis.
 * @param t Town that charges.
 * @return Rate in permille.
 */
uint GetForkTownTaxPermille(const Town *t)
{
	return 20 + GetForkTownSizeLevel(t);
}

/**
 * Weight of a station for the tax: the more and the bigger the facilities, the more tax is due.
 * @param st Station to look at.
 * @return Weight; 0 when the station has nothing that can be taxed.
 */
static uint GetStationTaxWeight(const Station *st)
{
	uint weight = 0;
	if (st->facilities.Test(StationFacility::Train)) weight += 3;
	if (st->facilities.Test(StationFacility::TruckStop)) weight += 1;
	if (st->facilities.Test(StationFacility::BusStop)) weight += 1;
	if (st->facilities.Test(StationFacility::Airport)) weight += 6;
	if (st->facilities.Test(StationFacility::Dock)) weight += 2;
	return weight;
}

/**
 * Charge the monthly local taxes: every town taxes the stations in it, and (later) the assets companies own in it.
 * A station with a train station in a town with a 6% rate pays about as much as a basic company running fee;
 * in the biggest towns the tax is twice that.
 */
void ChargeForkMunicipalTaxes()
{
	for (const Station *st : Station::Iterate()) {
		if (st->town == nullptr || !Company::IsValidID(st->owner)) continue;

		uint weight = GetStationTaxWeight(st);
		if (weight == 0) continue;

		Money tax = _price[Price::StationValue] * weight * GetForkTownTaxPermille(st->town) / 360;
		ForkLedgerBook(st->owner, ForkLedgerCategory::Tax, -tax);
	}

	for (const ForkAsset *a : ForkAsset::Iterate()) {
		if (a->location_kind != static_cast<uint8_t>(ForkAssetLocation::Town) || !Company::IsValidID(a->owner) || a->value <= 0) continue;

		const Town *t = Town::GetIfValid(static_cast<TownID>(a->location));
		if (t == nullptr) continue;

		/* The rate is a yearly rate on the value of the asset. */
		Money tax = a->value * GetForkTownTaxPermille(t) / 12000;
		ForkLedgerBook(a->owner, ForkLedgerCategory::Tax, -tax);
	}
}
