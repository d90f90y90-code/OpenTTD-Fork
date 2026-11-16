/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_rights.cpp Rights and licenses: a town only lets a company transport the cargo that comes from the town when the company owns the right for it, or has an annual license. */

#include "stdafx.h"

#include "cargotype.h"
#include "command_func.h"
#include "company_base.h"
#include "company_func.h"
#include "economy_func.h"
#include "fork_economy.h"
#include "fork_rights_cmd.h"
#include "landscape_type.h"
#include "map_func.h"
#include "news_func.h"
#include "settings_type.h"
#include "strings_func.h"
#include "town.h"
#include "window_func.h"

#include "table/strings.h"

#include "safeguards.h"

/** Price range of the rights, in dollars at the start of a game; the prices of the towns lie in between. */
struct ForkRightRange {
	uint lowest; ///< Lowest price.
	uint highest; ///< Highest price.
};

static constexpr ForkRightRange FORK_RIGHT_RANGES[] = {
	{2500, 4500}, // Mining: coal, iron ore, copper ore.
	{2000, 3000}, // Logging: wood.
	{10000, 25000}, // Oil.
	{400, 800}, // Water.
	{2000, 3000}, // Food and goods.
	{10000, 25000}, // Valuables.
	{300, 2500}, // People and mail.
};
static_assert(lengthof(FORK_RIGHT_RANGES) == static_cast<size_t>(ForkRightCategory::End));

static constexpr uint FORK_LICENSE_YEARLY_PERCENT = 25; ///< A license costs this part of the price of the right per year.
static constexpr uint FORK_LICENSE_MAX_YEARS = 3; ///< A license runs at most this many years.

/** Rights and licenses are stored in assets: data[0] is the category, data[1] the kind, data[2] the months a license has left. */
static constexpr int64_t FORK_RIGHT_OWNED = 1; ///< The company owns the right for good.
static constexpr int64_t FORK_RIGHT_LICENSE = 2; ///< The company has a license for a limited time.

static std::array<uint, MAX_COMPANIES> _fork_confiscated{}; ///< Deliveries confiscated this month, per company. Only used for the monthly report.

/** @return Whether this asset is a right or license of the company in the town for the category. */
static bool IsRightOf(const ForkAsset *a, CompanyID company, TownID town, ForkRightCategory category)
{
	return a->owner == company && a->type == static_cast<uint8_t>(ForkAssetType::MiningRight) && a->location == town.base() && a->data[0] == static_cast<int64_t>(category);
}

/** @return The asset holding the right or license, or nullptr. */
static ForkAsset *FindRight(CompanyID company, TownID town, ForkRightCategory category)
{
	for (ForkAsset *a : ForkAsset::Iterate()) {
		if (IsRightOf(a, company, town, category)) return a;
	}
	return nullptr;
}

/**
 * Which right is needed to transport a cargo.
 * @param cargo Type of the cargo.
 * @return The category of the right.
 */
ForkRightCategory GetForkRightCategory(CargoType cargo)
{
	const CargoSpec *cs = CargoSpec::Get(cargo);
	if (!cs->IsValid()) return ForkRightCategory::FoodGoods;
	if (cs->classes.Test(CargoClass::Passengers) || cs->classes.Test(CargoClass::Mail)) return ForkRightCategory::PeopleMail;

	if (cs->label == CT_COAL || cs->label == CT_IRON_ORE || cs->label == CT_COPPER_ORE) return ForkRightCategory::Mining;
	if (cs->label == CT_WOOD) return ForkRightCategory::Logging;
	if (cs->label == CT_OIL) return ForkRightCategory::Oil;
	if (cs->label == CT_WATER) return ForkRightCategory::Water;
	if (cs->label == CT_VALUABLES || cs->label == CT_GOLD || cs->label == CT_DIAMONDS) return ForkRightCategory::Valuables;

	/* Everything else is food or goods of some kind. */
	return ForkRightCategory::FoodGoods;
}

/**
 * Whether a town sells a kind of right at all. Water rights only exist in the sub-tropical climate, where there are deserts.
 * @param category Category of the right.
 * @return True if the right is for sale.
 */
bool IsForkRightAvailable([[maybe_unused]] const Town *t, ForkRightCategory category)
{
	if (category >= ForkRightCategory::End) return false;
	if (category == ForkRightCategory::Water) return _settings_game.game_creation.landscape == LandscapeType::Tropic;
	return true;
}

/**
 * Price of a right in a town. Every town has its own price within the range of the category;
 * it follows inflation and the construction cost setting like all prices.
 * @param t Town to look at.
 * @param category Category of the right.
 * @return Price of the right.
 */
Money GetForkRightPrice(const Town *t, ForkRightCategory category)
{
	const ForkRightRange &range = FORK_RIGHT_RANGES[static_cast<size_t>(category)];

	uint32_t h = TileX(t->xy) * 2654435761U ^ TileY(t->xy) * 40503U ^ (static_cast<uint32_t>(category) + 1) * 0x9E3779B1U;
	h ^= h >> 15;
	h *= 0x2c1b3c6dU;
	h ^= h >> 12;

	uint dollars = range.lowest + (range.highest - range.lowest) * (h % 101) / 100;
	dollars = dollars / 10 * 10;

	/* A rail station costs about 400 dollars at the start of a game; prices are relative to that. */
	return _price[Price::BuildStationRail] * dollars / 400;
}

/**
 * Monthly rent of an annual license: a quarter of the price of the right per year, paid monthly.
 * @param t Town to look at.
 * @param category Category of the right.
 * @return Rent per month.
 */
Money GetForkLicenseMonthlyRent(const Town *t, ForkRightCategory category)
{
	return GetForkRightPrice(t, category) * FORK_LICENSE_YEARLY_PERCENT / 100 / 12;
}

/**
 * Get what a company holds of a right.
 * @param company Company to look at.
 * @param town Town of the right.
 * @param category Category of the right.
 * @return Whether the right is owned, and how many months a license has left.
 */
ForkRightStatus GetForkRightStatus(CompanyID company, TownID town, ForkRightCategory category)
{
	ForkRightStatus status;
	const ForkAsset *a = FindRight(company, town, category);
	if (a == nullptr) return status;

	if (a->data[1] == FORK_RIGHT_OWNED) {
		status.owned = true;
	} else {
		status.license_months = static_cast<uint>(std::max<int64_t>(a->data[2], 0));
	}
	return status;
}

/**
 * Check whether a company may transport cargo from where it came. The town the cargo came from decides:
 * the company needs the right or a valid license for that kind of cargo in that town.
 * @param company Company that delivers the cargo.
 * @param cargo Type of the cargo.
 * @param source Where the cargo came from.
 * @return True if the transport is allowed.
 */
bool IsForkTransportAllowed(CompanyID company, CargoType cargo, const Source &source)
{
	const Town *t = GetForkSourceTown(source);
	if (t == nullptr || !Company::IsValidID(company)) return true;

	const ForkRightCategory category = GetForkRightCategory(cargo);
	if (!IsForkRightAvailable(t, category)) return true;

	const ForkRightStatus status = GetForkRightStatus(company, t->index, category);
	return status.owned || status.license_months > 0;
}

/**
 * Remember that a delivery was confiscated, for the report at the end of the month.
 * @param company Company that lost the delivery.
 */
void ForkNoteConfiscation(CompanyID company)
{
	if (company.base() < MAX_COMPANIES) _fork_confiscated[company.base()]++;
}

/**
 * Buy a right for good.
 * @param flags Type of operation.
 * @param town_id Town to buy in.
 * @param category Category of the right.
 * @return The cost of this operation or an error.
 */
CommandCost CmdBuyForkRight(DoCommandFlags flags, TownID town_id, uint8_t category)
{
	if (!Company::IsValidID(_current_company) || category >= static_cast<uint8_t>(ForkRightCategory::End)) return CMD_ERROR;
	Town *t = Town::GetIfValid(town_id);
	if (t == nullptr) return CMD_ERROR;

	const ForkRightCategory cat = static_cast<ForkRightCategory>(category);
	if (!IsForkRightAvailable(t, cat)) return CommandCost(STR_ERROR_FORK_RIGHT_NOT_AVAILABLE);

	ForkAsset *existing = FindRight(_current_company, town_id, cat);
	if (existing != nullptr && existing->data[1] == FORK_RIGHT_OWNED) return CommandCost(STR_ERROR_FORK_RIGHT_ALREADY_OWNED);
	if (existing == nullptr && !ForkAsset::CanAllocateItem()) return CommandCost(STR_ERROR_FORK_TOO_MANY_ASSETS);

	const Money price = GetForkRightPrice(t, cat);
	CommandCost cost(ExpensesType::Other, price);

	if (flags.Test(DoCommandFlag::Execute)) {
		ForkAsset *a = existing;
		if (a == nullptr) {
			a = ForkAsset::Create();
			a->owner = _current_company;
			a->type = static_cast<uint8_t>(ForkAssetType::MiningRight);
			a->location_kind = static_cast<uint8_t>(ForkAssetLocation::Town);
			a->location = town_id.base();
			a->data[0] = category;
		}
		a->data[1] = FORK_RIGHT_OWNED; // A license that was still running turns into ownership.
		a->data[2] = 0;
		a->purchase_price = price;
		a->value = 0; // Rights do not cost property tax.
		a->monthly_cost = 0;
		SetWindowClassesDirty(WindowClass::ForkRights);
		SetWindowClassesDirty(WindowClass::ForkLicense);
		InvalidateWindowData(WindowClass::ForkBusiness, 0);
	}
	return cost;
}

/**
 * Sign an annual license. The rent is paid monthly, so nothing is paid when signing.
 * @param flags Type of operation.
 * @param town_id Town to sign in.
 * @param category Category of the license.
 * @param years Duration in years, 1 to 3.
 * @return The cost of this operation (none) or an error.
 */
CommandCost CmdSignForkLicense(DoCommandFlags flags, TownID town_id, uint8_t category, uint8_t years)
{
	if (!Company::IsValidID(_current_company) || category >= static_cast<uint8_t>(ForkRightCategory::End)) return CMD_ERROR;
	if (years < 1 || years > FORK_LICENSE_MAX_YEARS) return CMD_ERROR;
	Town *t = Town::GetIfValid(town_id);
	if (t == nullptr) return CMD_ERROR;

	const ForkRightCategory cat = static_cast<ForkRightCategory>(category);
	if (!IsForkRightAvailable(t, cat)) return CommandCost(STR_ERROR_FORK_RIGHT_NOT_AVAILABLE);

	ForkAsset *existing = FindRight(_current_company, town_id, cat);
	if (existing != nullptr && existing->data[1] == FORK_RIGHT_OWNED) return CommandCost(STR_ERROR_FORK_RIGHT_ALREADY_OWNED);
	if (existing == nullptr && !ForkAsset::CanAllocateItem()) return CommandCost(STR_ERROR_FORK_TOO_MANY_ASSETS);

	if (flags.Test(DoCommandFlag::Execute)) {
		ForkAsset *a = existing;
		if (a == nullptr) {
			a = ForkAsset::Create();
			a->owner = _current_company;
			a->type = static_cast<uint8_t>(ForkAssetType::MiningRight);
			a->location_kind = static_cast<uint8_t>(ForkAssetLocation::Town);
			a->location = town_id.base();
			a->data[0] = category;
		}
		a->data[1] = FORK_RIGHT_LICENSE;
		a->data[2] = static_cast<int64_t>(years) * 12;
		a->value = 0;
		a->monthly_cost = GetForkLicenseMonthlyRent(t, cat);
		SetWindowClassesDirty(WindowClass::ForkRights);
		SetWindowClassesDirty(WindowClass::ForkLicense);
		InvalidateWindowData(WindowClass::ForkBusiness, 0);
	}
	return CommandCost();
}

/** Every month a license is paid, and it ends when its time is over. */
static void ForkRightMonthly(ForkAsset *a)
{
	if (a->data[1] != FORK_RIGHT_LICENSE) return;

	const Town *t = Town::GetIfValid(static_cast<TownID>(a->location));
	if (t == nullptr || a->data[0] < 0 || a->data[0] >= static_cast<int64_t>(ForkRightCategory::End)) {
		delete a;
		return;
	}

	const ForkRightCategory cat = static_cast<ForkRightCategory>(a->data[0]);
	a->monthly_cost = GetForkLicenseMonthlyRent(t, cat);
	ForkLedgerBook(a->owner, ForkLedgerCategory::Tax, -a->monthly_cost);

	a->data[2]--;
	if (a->data[2] <= 0) {
		if (a->owner == _local_company) {
			AddNewsItem(GetEncodedString(STR_FORK_NEWS_LICENSE_EXPIRED, STR_FORK_RIGHT_NAME_MINING + static_cast<uint>(cat), t->index), NewsType::Economy, NewsStyle::Normal, {});
		}
		delete a;
	}
	SetWindowClassesDirty(WindowClass::ForkRights);
	SetWindowClassesDirty(WindowClass::ForkLicense);
}

/** Report the deliveries that were confiscated last month, and start counting again. */
void ForkRightsOnNewMonth()
{
	for (uint c = 0; c < MAX_COMPANIES; c++) {
		if (_fork_confiscated[c] > 0 && CompanyID(static_cast<uint8_t>(c)) == _local_company) {
			AddNewsItem(GetEncodedString(STR_FORK_NEWS_CONFISCATED, _fork_confiscated[c]), NewsType::Economy, NewsStyle::Normal, {});
		}
		_fork_confiscated[c] = 0;
	}
}

/** Make the monthly update of rights known to the extended economy. */
void RegisterForkRights()
{
	RegisterForkAssetHandler(ForkAssetType::MiningRight, ForkRightMonthly);
}
