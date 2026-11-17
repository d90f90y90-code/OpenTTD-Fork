/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/**
 * @file fork_media.cpp Television and radio channels.
 *
 * A company can build one channel of each kind (entertainment, radio, news, sport) in a big town. It takes
 * a few months to build. Once on air the channel has an audience that depends on the size of the town, the
 * kind of channel, the reputation of the channel and how well the town is staffed. A free channel reaches
 * more people and so becomes more popular; a channel with a paid package earns from subscriptions, but the
 * higher the price of the package the fewer people subscribe. Both earn from advertising. Popularity raises
 * the value of the building and the reputation of the company in the town.
 * A channel is a #ForkAsset of the type #ForkAssetType::MediaChannel with these values in #ForkAsset::data:
 *   0 = kind, 1 = price of the package (0 = free), 2 = popularity (0-1000), 3 = reputation (0-1000).
 */

#include "stdafx.h"

#include "command_func.h"
#include "core/backup_type.hpp"
#include "company_base.h"
#include "company_func.h"
#include "economy_func.h"
#include "fork_economy.h"
#include "fork_media.h"
#include "fork_media_cmd.h"
#include "town.h"
#include "window_func.h"

#include "table/strings.h"

#include "safeguards.h"

static constexpr int64_t _channel_price[FORK_CHANNEL_KINDS] = {30000, 10000, 25000, 20000}; ///< Price to build a channel at the start of the game.
static constexpr int64_t _channel_running_cost[FORK_CHANNEL_KINDS] = {1200, 300, 900, 700}; ///< Monthly running costs at the start of the game.
static constexpr uint _channel_reach_percent[FORK_CHANNEL_KINDS] = {60, 70, 50, 35}; ///< Share of the inhabitants that can be reached.

/** Make an amount of the start of the game worth what it is now, following inflation of construction costs. */
static Money Scaled(Money base)
{
	return base * static_cast<int64_t>(_price[Price::BuildStationRail]) / 150;
}

/** @return Whether the town is big enough for channels. */
bool IsForkMediaTown(const Town *t)
{
	return t->cache.population >= FORK_MEDIA_MIN_POPULATION;
}

/** @return Price to build a channel of a kind. */
Money GetForkChannelPrice(uint kind)
{
	return Scaled(_channel_price[std::min<uint>(kind, FORK_CHANNEL_KINDS - 1)]);
}

/** @return The channel of a kind in a town, of whoever owns it, or nullptr. */
const ForkAsset *FindForkChannel(TownID town, uint kind)
{
	for (const ForkAsset *a : ForkAsset::Iterate()) {
		if (a->type == static_cast<uint8_t>(ForkAssetType::MediaChannel) && a->location == town.base() && a->data[0] == static_cast<int64_t>(kind)) return a;
	}
	return nullptr;
}

/** @return Whether the channel has been built and is on air. */
bool IsForkChannelOperating(const ForkAsset *a)
{
	return a->age_months >= FORK_CHANNEL_BUILD_MONTHS;
}

/** @return Which picture of the building to show: 0 foundation, 1 under construction, 2 finished. */
uint GetForkChannelStage(const ForkAsset *a)
{
	if (a->age_months <= 1) return 0;
	return a->age_months < FORK_CHANNEL_BUILD_MONTHS ? 1 : 2;
}

/** Efficiency of the channels of a company in a town: the same staff does the work of the houses, shops and channels. */
static uint GetChannelEfficiency(CompanyID company, TownID town)
{
	uint channels = 0;
	for (const ForkAsset *c : ForkAsset::Iterate()) {
		if (c->owner == company && c->type == static_cast<uint8_t>(ForkAssetType::MediaChannel) && c->location == town.base() && IsForkChannelOperating(c)) channels++;
	}

	const uint required = GetForkRequiredStaff(GetForkCompanyHouses(company, town), GetForkCompanyShops(company, town)) + FORK_CHANNEL_STAFF * channels;
	if (required == 0) return 100;

	const uint ratio = GetForkCompanyStaff(company, town) * 100 / required;
	if (ratio < 100) return 50 + ratio / 2;
	return 100 + std::min<uint>(10, (ratio - 100) / 5);
}

/**
 * Work out what a channel does in a month.
 * @param a The channel.
 * @return Audience, income and costs; all zero while the channel is being built.
 */
ForkChannelStats GetForkChannelStats(const ForkAsset *a)
{
	ForkChannelStats s;
	const Town *t = Town::GetIfValid(static_cast<TownID>(a->location));
	if (t == nullptr || !IsForkChannelOperating(a)) return s;

	const uint kind = static_cast<uint>(std::clamp<int64_t>(a->data[0], 0, FORK_CHANNEL_KINDS - 1));
	const uint package = static_cast<uint>(std::clamp<int64_t>(a->data[1], 0, FORK_CHANNEL_PACKAGE_MAX));
	const uint popularity = static_cast<uint>(std::clamp<int64_t>(a->data[2], 0, 1000));
	const uint reputation = static_cast<uint>(std::clamp<int64_t>(a->data[3], 0, 1000));

	s.efficiency = GetChannelEfficiency(a->owner, t->index);

	/* The people who could watch, a good reputation brings more of them. */
	uint potential = t->cache.population * _channel_reach_percent[kind] / 100;
	potential = potential * (700 + reputation * 3 / 10) / 1000;

	if (package == 0) {
		s.audience = potential * s.efficiency / 100;
	} else {
		/* The higher the price of the package, the fewer people pay for it. */
		const uint share = std::clamp<uint>(100 - package / 10, 10, 100);
		s.subscribers = potential * share / 100 * s.efficiency / 100;
		s.audience = s.subscribers;
	}

	s.subscription_income = Scaled(Money(s.subscribers) * package / 300);
	s.ad_income = Scaled(Money(s.audience) * 3 * (500 + popularity) / 10000);
	s.running_cost = Scaled(_channel_running_cost[kind]);
	s.net = s.subscription_income + s.ad_income - s.running_cost;
	return s;
}

/**
 * Build a channel in a town.
 * @param flags Type of operation.
 * @param town_id Town to build in.
 * @param kind Kind of channel, a #ForkChannelKind.
 * @param package Price of the paid package, or 0 for a free channel.
 * @param name Name of the channel.
 * @return The cost of this operation or an error.
 */
CommandCost CmdBuildForkChannel(DoCommandFlags flags, TownID town_id, uint8_t kind, uint16_t package, const std::string &name)
{
	if (!Company::IsValidID(_current_company) || kind >= FORK_CHANNEL_KINDS || package > FORK_CHANNEL_PACKAGE_MAX) return CMD_ERROR;
	if (name.empty() || name.size() > 63) return CMD_ERROR;
	Town *t = Town::GetIfValid(town_id);
	if (t == nullptr) return CMD_ERROR;

	if (!IsForkMediaTown(t)) return CommandCost(STR_ERROR_FORK_MEDIA_TOWN_TOO_SMALL);
	if (FindForkChannel(town_id, kind) != nullptr) return CommandCost(STR_ERROR_FORK_MEDIA_KIND_TAKEN);
	if (!ForkAsset::CanAllocateItem()) return CommandCost(STR_ERROR_FORK_TOO_MANY_ASSETS);

	CommandCost cost(ExpensesType::Construction, GetForkChannelPrice(kind));

	if (flags.Test(DoCommandFlag::Execute)) {
		ForkAsset *a = ForkAsset::Create();
		a->owner = _current_company;
		a->type = static_cast<uint8_t>(ForkAssetType::MediaChannel);
		a->location_kind = static_cast<uint8_t>(ForkAssetLocation::Town);
		a->location = town_id.base();
		a->purchase_price = cost.GetCost();
		a->value = cost.GetCost();
		a->name = name;
		a->data[0] = kind;
		a->data[1] = package;
		a->data[2] = 0;
		a->data[3] = 500;
		SetWindowClassesDirty(WindowClass::ForkMedia);
		InvalidateWindowData(WindowClass::ForkBusiness, 0);
	}
	return cost;
}

/**
 * Change the package of a channel.
 * @param flags Type of operation.
 * @param asset_id The channel.
 * @param package New price of the paid package, or 0 for a free channel.
 * @return The cost of this operation (none) or an error.
 */
CommandCost CmdSetForkChannelPackage(DoCommandFlags flags, ForkAssetID asset_id, uint16_t package)
{
	ForkAsset *a = ForkAsset::GetIfValid(asset_id);
	if (a == nullptr || a->type != static_cast<uint8_t>(ForkAssetType::MediaChannel) || a->owner != _current_company || package > FORK_CHANNEL_PACKAGE_MAX) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		a->data[1] = package;
		SetWindowClassesDirty(WindowClass::ForkChannel);
		SetWindowClassesDirty(WindowClass::ForkMedia);
	}
	return CommandCost();
}

/** Every month a channel is built a bit further or, once on air, earns and spends money and gets more or less popular. */
static void ForkChannelMonthly(ForkAsset *a)
{
	Town *t = Town::GetIfValid(static_cast<TownID>(a->location));
	if (t == nullptr || !IsForkChannelOperating(a)) {
		a->value = a->purchase_price;
		a->monthly_income = 0;
		a->monthly_cost = 0;
		SetWindowClassesDirty(WindowClass::ForkChannel);
		SetWindowClassesDirty(WindowClass::ForkMedia);
		return;
	}

	const ForkChannelStats s = GetForkChannelStats(a);

	/* Popularity moves towards the share of the town that is reached; a free channel reaches more people. */
	const int64_t target = std::min<int64_t>(1000, static_cast<int64_t>(s.audience) * 1600 / std::max<uint>(1, t->cache.population));
	a->data[2] += (target - a->data[2]) / 6;
	a->data[2] = std::clamp<int64_t>(a->data[2], 0, 1000);

	/* A popular channel makes the building worth more. */
	a->value = a->purchase_price * (900 + a->data[2]) / 1000;
	a->monthly_income = s.net;
	a->monthly_cost = s.running_cost;

	ForkLedgerBook(a->owner, ForkLedgerCategory::Trade, s.subscription_income);
	ForkLedgerBook(a->owner, ForkLedgerCategory::Advertising, s.ad_income);
	ForkLedgerBook(a->owner, ForkLedgerCategory::Other, -s.running_cost);

	/* ...and it makes the town think better of the company. */
	if (Company::IsValidID(a->owner)) {
		AutoRestoreBackup cur_company(_current_company, a->owner);
		ChangeTownRating(t, static_cast<int>(a->data[2] / 250), RATING_VERYGOOD, DoCommandFlag::Execute);
	}

	SetWindowClassesDirty(WindowClass::ForkChannel);
	SetWindowClassesDirty(WindowClass::ForkMedia);
}

/** Make the monthly update of channels known to the extended economy. */
void RegisterForkMedia()
{
	RegisterForkAssetHandler(ForkAssetType::MediaChannel, ForkChannelMonthly);
}
