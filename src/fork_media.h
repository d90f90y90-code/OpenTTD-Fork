/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_media.h Television and radio channels that companies build in big towns. */

#ifndef FORK_MEDIA_H
#define FORK_MEDIA_H

#include "company_type.h"
#include "economy_type.h"
#include "town_type.h"

struct ForkAsset;
struct Town;

/** The kinds of channel. The order is the order of the building pictures. */
enum class ForkChannelKind : uint8_t {
	Entertainment, ///< Entertainment channel.
	Radio, ///< Radio station.
	News, ///< News channel.
	Sport, ///< Sport channel.
	End, ///< End marker.
};

static constexpr uint FORK_CHANNEL_KINDS = static_cast<uint>(ForkChannelKind::End); ///< Number of kinds of channel.
static constexpr uint FORK_CHANNEL_BUILD_MONTHS = 4; ///< Months it takes to build a channel before it goes on air.
static constexpr uint FORK_MEDIA_MIN_POPULATION = 1500; ///< Channels can only be built in towns with this many inhabitants.
static constexpr uint FORK_CHANNEL_PACKAGE_MAX = 800; ///< Highest price of a paid package.
static constexpr uint FORK_CHANNEL_STAFF = 5; ///< Staff a channel needs to work at full efficiency.

/** Numbers about the work of a channel in the last month. */
struct ForkChannelStats {
	uint efficiency = 100; ///< Efficiency from how well the town is staffed, in percent.
	uint audience = 0; ///< Number of people who watch or listen.
	uint subscribers = 0; ///< Number of people who pay for the package.
	Money subscription_income = 0; ///< Income from paid packages per month.
	Money ad_income = 0; ///< Income from advertising per month.
	Money running_cost = 0; ///< Costs of running the channel per month.
	Money net = 0; ///< Result per month.
};

bool IsForkMediaTown(const Town *t);
Money GetForkChannelPrice(uint kind);
const ForkAsset *FindForkChannel(TownID town, uint kind);
bool IsForkChannelOperating(const ForkAsset *a);
uint GetForkChannelStage(const ForkAsset *a);
ForkChannelStats GetForkChannelStats(const ForkAsset *a);
void RegisterForkMedia();

void ShowForkMediaWindow(uint town);
void ShowForkChannelCreateWindow(uint town, uint kind);
void ShowForkChannelWindow(uint asset);

#endif /* FORK_MEDIA_H */
