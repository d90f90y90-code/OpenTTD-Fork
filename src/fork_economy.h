/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_economy.h Foundation of the fork's extended economy: market state, business ledger and the asset registry. */

#ifndef FORK_ECONOMY_H
#define FORK_ECONOMY_H

#include "company_type.h"
#include "core/pool_type.hpp"
#include "cargo_type.h"
#include "economy_type.h"
#include "fork_firm.h"
#include "fork_stock.h"
#include "source_type.h"
#include "station_type.h"
#include "strings_type.h"
#include "town_type.h"

/**
 * Categories of the business ledger. Every extended-economy feature books its income and costs in one of these,
 * so the finance code never has to change when a feature is added. Some entries are reserved for later features.
 */
enum class ForkLedgerCategory : uint8_t {
	Tax, ///< Local taxes and fees.
	Rent, ///< Rent income and rent costs.
	Salaries, ///< Salaries of staff.
	Dividends, ///< Dividends from shares.
	Advertising, ///< Income from advertising and sponsoring.
	Trade, ///< Trade, customs and shops.
	Leasing, ///< Leasing contracts.
	Other, ///< Anything else.
	End, ///< End marker.
};

/** Phases of the economic cycle. */
enum class ForkMarketPhase : uint8_t {
	Normal, ///< Stable economy.
	Boom, ///< Rising prices, rents and share prices.
	Recession, ///< Falling prices, rents and share prices.
	End, ///< End marker.
};

/** Kinds of rights a town sells. Each one allows the transport of a group of cargoes that come from the town. */
enum class ForkRightCategory : uint8_t {
	Mining, ///< Coal, iron ore and copper ore.
	Logging, ///< Wood.
	Oil, ///< Oil.
	Water, ///< Water.
	FoodGoods, ///< Food, goods and all other cargo.
	Valuables, ///< Valuables, gold and diamonds.
	PeopleMail, ///< Passengers and mail.
	End, ///< End marker.
};

/** What a company holds of a right in a town. */
struct ForkRightStatus {
	bool owned = false; ///< The company owns the right for good.
	uint license_months = 0; ///< Months a license has left, zero if there is none.
};

/** Things the market state (boom/recession) can influence. Every factor is a percentage, 100 means neutral. */
enum class ForkMarketFactor : uint8_t {
	CargoPrice, ///< Payment for transported cargo.
	Rent, ///< Rents of property.
	StockPrice, ///< Prices of shares.
	Interest, ///< Interest rates.
	Consumption, ///< What inhabitants spend in shops.
	Wages, ///< Salaries of staff.
	End, ///< End marker.
};

/** Kinds of assets a company can own in the extended economy. */
enum class ForkAssetType : uint8_t {
	None, ///< Unused.
	Property, ///< Land and buildings in a town.
	Shop, ///< Retail shops.
	Lease, ///< Leasing contract.
	MiningRight, ///< Right to the output of an industry.
	MediaChannel, ///< Radio or TV channel.
	Shares, ///< Shares of an NPC company.
	Staff, ///< Managers and administrative staff in a town.
	Education, ///< Private school or university in a town.
	End, ///< End marker.
};

/** What the #ForkAsset::location of an asset refers to. */
enum class ForkAssetLocation : uint8_t {
	None, ///< Not tied to a place.
	Town, ///< A #TownID.
	Industry, ///< An #IndustryID.
	Tile, ///< A #TileIndex.
};

static constexpr size_t FORK_LEDGER_CATEGORIES = static_cast<size_t>(ForkLedgerCategory::End);
static constexpr size_t FORK_LEDGER_SIZE = MAX_COMPANIES * FORK_LEDGER_CATEGORIES; ///< One amount per company and category.
static constexpr uint32_t FORK_ECONOMY_DATA_VERSION = 1; ///< Version of the data layout inside the fork economy savegame chunks.

/** Global state of the extended economy. Saved in the 'FKEC' chunk. */
static constexpr size_t FORK_DEALER_COUNT = 5; ///< Number of dealers of used vehicles: TTD MAX, TAJCAR, LIMANO, SEAMAR, AIRIX.
static constexpr size_t FORK_OFFERS_PER_DEALER = 6; ///< The most offers one dealer makes.
static constexpr size_t FORK_USED_SLOTS = 24; ///< Number of offers per company, all dealers together. Part of the savegame layout.

struct ForkEconomyState {
	ForkEconomyState() { this->firm_ref_price.fill(100); }

	uint32_t data_version = FORK_ECONOMY_DATA_VERSION; ///< Version of this data, for conversions after the layout changed.
	uint8_t market_phase = 0; ///< Current phase of the economic cycle (0 = normal).
	int32_t market_index = 1000; ///< Strength of the market in permille; 1000 is neutral.
	int32_t market_target = 1000; ///< Value the market index moves towards, set when a phase starts.
	uint16_t market_months_left = 24; ///< Months until the market phase changes.
	std::array<int64_t, FORK_LISTED_COUNT> stock_price{}; ///< Price of a share of every listed company; zero until the first month.
	std::array<int64_t, FORK_LISTED_COUNT> stock_previous{}; ///< The prices of a month ago.
	std::array<int16_t, FORK_PRODUCT_COUNT> firm_ref_price{}; ///< Market price of every firm product, in percent of the base price.
	std::array<Money, FORK_LEDGER_SIZE> ledger_month{}; ///< Ledger amounts of the current month.
	std::array<Money, FORK_LEDGER_SIZE> ledger_last{}; ///< Ledger amounts of the previous month.
	int32_t used_month = -1; ///< Month of the used vehicle offers that #used_sold is about.
	std::array<uint8_t, MAX_COMPANIES * FORK_USED_SLOTS> used_sold{}; ///< Per company and offer, how many vehicles were bought this month.
};

extern ForkEconomyState _fork_economy;

using ForkAssetID = PoolID<uint32_t, struct ForkAssetIDTag, 0xFF0000, 0xFFFFFF>;
struct ForkAsset;
using ForkAssetPool = Pool<ForkAsset, ForkAssetID, 64>;
extern ForkAssetPool _fork_asset_pool;

/** Something a company owns in the extended economy. Features use the generic fields and keep specials in #data. */
struct ForkAsset : ForkAssetPool::PoolItem<&_fork_asset_pool> {
	CompanyID owner = CompanyID::Invalid(); ///< Company that owns the asset.
	uint8_t type = 0; ///< Kind of asset, a #ForkAssetType.
	uint8_t location_kind = 0; ///< What #location refers to, a #ForkAssetLocation.
	uint32_t location = 0; ///< Town, industry or tile the asset is tied to.
	Money purchase_price = 0; ///< What the owner paid.
	Money value = 0; ///< Current value.
	Money monthly_income = 0; ///< Income per month.
	Money monthly_cost = 0; ///< Costs per month.
	uint32_t age_months = 0; ///< Age of the asset.
	std::array<int64_t, 4> data{}; ///< Free for the feature that owns this asset type.
	std::array<int64_t, 4> data2{}; ///< More free data, for features that need more room.
	std::string name; ///< Name given by the owner, for the assets that have one (channels).

	/**
	 * Construct an asset.
	 * @param index Index within the pool.
	 */
	ForkAsset(ForkAssetID index = ForkAssetID::Invalid()) : ForkAssetPool::PoolItem<&_fork_asset_pool>(index) {}

	/** @return The type of this asset. */
	inline ForkAssetType GetType() const { return static_cast<ForkAssetType>(this->type); }
};

/** Function that a feature registers to run for each of its assets, every month. */
using ForkAssetMonthlyHandler = void (*)(ForkAsset *asset);

void ResetForkEconomy();
void RegisterForkAssetHandler(ForkAssetType type, ForkAssetMonthlyHandler handler);

uint GetForkMarketFactor(ForkMarketFactor factor);
void ForkLedgerBook(CompanyID company, ForkLedgerCategory category, Money amount);
Money GetForkLedger(CompanyID company, ForkLedgerCategory category, bool last_month);
uint GetForkTownSizeLevel(const Town *t);
uint GetForkTownTaxPermille(const Town *t);
void ChargeForkMunicipalTaxes();
uint GetForkAssetCount(CompanyID company);
void TransferForkAssets(Owner old_owner, Owner new_owner);

int GetForkPropertySwing();
Money GetForkHouseValue(const Town *t);
Money GetForkHouseBuyPrice(const Town *t);
Money GetForkHouseSellValue(const Town *t);
Money GetForkHouseRent(const Town *t, uint rent_percent);
uint GetForkHouseLimit(const Town *t);
uint GetForkCompanyHouses(CompanyID company, TownID town);
uint GetForkRentPercent(CompanyID company, TownID town);
uint GetForkOccupancy(uint rent_percent);
void RegisterForkRealEstate();

/** Result of the retail shops of a company in a town, per month. */
struct ForkShopResult {
	Money sales = 0; ///< Money taken at the cash registers.
	Money gross = 0; ///< Sales minus the cost of the goods.
	Money fixed = 0; ///< Fixed costs of running the shops.
	Money profit = 0; ///< Gross profit minus fixed costs.
	uint share_permille = 0; ///< Share of the spending in the town that goes to the shops, in permille.
};

Money GetForkTownRetailSpending(const Town *t);
Money GetForkShopValue(const Town *t);
Money GetForkShopBuyPrice(const Town *t);
Money GetForkShopSellValue(const Town *t);
uint GetForkShopLimit(const Town *t);
uint GetForkCompanyShops(CompanyID company, TownID town);
uint GetForkShopPrice(CompanyID company, TownID town);
ForkShopResult GetForkShopResult(const Town *t, CompanyID company, uint shops, uint price_percent);
void RegisterForkShops();
void ShowForkShopsWindow(uint town);

Money GetForkWage(const Town *t);
uint GetForkStaffLimit(const Town *t);
uint GetForkCompanyStaff(CompanyID company, TownID town);
uint GetForkRequiredStaff(uint houses, uint shops, uint firms = 0);
uint GetForkStaffEfficiencyFor(CompanyID company, TownID town, uint houses, uint shops, uint firms = 0);
uint GetForkStaffEfficiency(CompanyID company, TownID town);
void RegisterForkLabor();

ForkRightCategory GetForkRightCategory(CargoType cargo);
bool IsForkRightAvailable(const Town *t, ForkRightCategory category);
Money GetForkRightPrice(const Town *t, ForkRightCategory category);
Money GetForkLicenseMonthlyRent(const Town *t, ForkRightCategory category);
ForkRightStatus GetForkRightStatus(CompanyID company, TownID town, ForkRightCategory category);
const Town *GetForkSourceTown(const Source &source);
bool IsForkTransportAllowed(CompanyID company, CargoType cargo, const Source &source);
void ForkNoteConfiscation(CompanyID company);
void ForkRightsOnNewMonth();
void RegisterForkRights();
void ShowForkRightsWindow(uint town);
void ShowForkLicenseWindow(uint town, uint category);

uint GetForkCustomsRate(const Town *t);
Money GetForkCustomsDuty(CargoType cargo, const Source &source, StationID destination, Money payment);
void ShowForkStaffWindow(uint town);
void ShowForkRealEstateWindow(uint town);

/** Kinds of private education institutions a company can build in a town. */
enum class ForkSchoolKind : uint8_t {
	Primary, ///< Private primary school.
	Secondary, ///< Private secondary school.
	University, ///< Private university with faculties.
	End, ///< End marker.
};

/** Things a company can do with an institution, for #CmdForkSchoolAction. */
enum class ForkSchoolAction : uint8_t {
	SetFee, ///< Set the fee per seat per year (schools); argument is the fee.
	ToggleShift, ///< Switch the double-shift mode on or off.
	Upgrade, ///< Enlarge a school, once.
	OpenFaculty, ///< Open a faculty (universities); argument is the faculty.
	CloseFaculty, ///< Close a faculty (universities); argument is the faculty.
	SetFacultyFee, ///< Set the yearly tuition of a faculty; argument is the faculty in the low byte and the tuition in the rest.
};

static constexpr uint FORK_FACULTIES = 10; ///< Number of faculties a university can have.

/** Result of one institution, per month unless said otherwise. */
struct ForkSchoolResult {
	uint capacity = 0; ///< Number of seats (including double shift).
	uint enrolled = 0; ///< Number of students.
	uint quality = 0; ///< Quality in percent: age of the building, what is taught, and how pleased the town is.
	uint popularity = 0; ///< Popularity in percent: quality weighed against what the institution asks for.
	Money income = 0; ///< Tuition received.
	Money running = 0; ///< Costs of running it.
	Money tax = 0; ///< Tax to the town.
	uint tax_percent = 0; ///< Tax rate of this year.
	std::array<uint, 10> fac_enrolled{}; ///< Students per faculty (universities).
	std::array<uint, 10> fac_popularity{}; ///< Popularity per faculty (universities).
};

const ForkAsset *GetForkSchool(CompanyID company, TownID town, ForkSchoolKind kind);
ForkSchoolResult GetForkSchoolResult(const Town *t, const ForkAsset *a);
std::string GetForkSchoolName(const ForkAsset *a);
Money GetForkSchoolBuildCost(ForkSchoolKind kind);
Money GetForkSchoolSellValue(const ForkAsset *a);
Money GetForkSchoolUpgradeCost(ForkSchoolKind kind);
uint GetForkSchoolMinPopulation(ForkSchoolKind kind);
uint GetForkSchoolFeeMax(ForkSchoolKind kind);
bool IsForkSchoolUpgraded(const ForkAsset *a);
bool IsForkSchoolDoubleShift(const ForkAsset *a);
uint GetForkSchoolFee(const ForkAsset *a);
uint GetForkFacultyOpenMask(const ForkAsset *a);
uint GetForkFacultyFee(const ForkAsset *a, uint faculty);
Money GetForkFacultyCost(uint faculty);
uint GetForkFacultyDesire(const Town *t, uint faculty);
StringID GetForkFacultyName(uint faculty);
void RegisterForkEducation();
void ShowForkEducationWindow(uint town);

uint GetForkCargoQuality(CargoType cargo, uint periods_in_transit);
Money GetForkSpoilageFee(uint num_pieces, uint dist, CargoType cargo);

void ShowForkBusinessWindow();

#endif /* FORK_ECONOMY_H */
