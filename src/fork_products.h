/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_products.h User-created products and the industries that can make them. */

#ifndef FORK_PRODUCTS_H
#define FORK_PRODUCTS_H

#include "company_type.h"
#include "core/pool_type.hpp"
#include "economy_type.h"

#include <string>

/** The industries a company can choose. A company is no longer locked to "technology". */
enum class ForkIndustryType : uint8_t {
	Household,          ///< Household appliances.
	LifeTechnology,     ///< Consumer electronics.
	Communications,     ///< Phones, routers and antennas.
	Beverages,          ///< Drinks.
	Food,               ///< Food industry.
	Clothing,           ///< Clothes and fabrics.
	Medical,            ///< Medical products.
	GameDevelopment,    ///< Video games.
	AppDevelopment,     ///< Software and applications.
	BuildingMaterials,  ///< Cement, bricks, glass and tools.
	Energy,             ///< Solar panels, batteries and generators.
	End,                ///< End marker.
};

static constexpr uint FORK_INDUSTRY_TYPE_COUNT = static_cast<uint>(ForkIndustryType::End);

/** One entry of the product catalogue. */
struct ForkCatalogEntry {
	const char *english;    ///< English name, for documentation only.
	const char *arabic;     ///< Arabic name shown in the game.
	ForkIndustryType type;  ///< Industry that can make it.
	uint16_t unlock_year;   ///< First year in which the product exists.
	uint32_t base_price;    ///< Price of one unit at the start of the game.
	uint32_t demand;        ///< Relative demand, per 100000 inhabitants.
};

extern const ForkCatalogEntry _fork_catalog[];
static constexpr size_t FORK_CATALOG_COUNT = 99; ///< Number of catalogue entries; kept in sync in fork_products.cpp.

/** @return The catalogue entry, or nullptr when the index is invalid. */
const ForkCatalogEntry *GetForkCatalogEntry(uint16_t id);

/** @return Whether the product already existed in the given year. */
bool IsForkProductUnlocked(uint16_t id, uint32_t year);

/** @return The current calendar year as an integer. */
uint32_t ForkProductCurrentYear();

using ForkUserProductID = PoolID<uint32_t, struct ForkUserProductIDTag, 0xFF0000, 0xFFFFFF>;
struct ForkUserProduct;
using ForkUserProductPool = Pool<ForkUserProduct, ForkUserProductID, 128>;
extern ForkUserProductPool _fork_user_product_pool;

/** A product that a player company makes and sells. */
struct ForkUserProduct : ForkUserProductPool::PoolItem<&_fork_user_product_pool> {
	CompanyID owner = CompanyID::Invalid(); ///< Company that owns the product.
	uint16_t catalog_id = 0;                ///< Catalogue entry it is based on.
	std::string name;                       ///< Name chosen by the player.
	uint32_t quantity = 10;                 ///< Units made per month.
	uint32_t price = 100;                   ///< Selling price of one unit.
	uint8_t quality = 3;                    ///< Quality level, 1 to 5.
	uint8_t discount = 0;                   ///< Temporary discount in percent, 0 to 50.
	uint16_t popularity = 50;               ///< Popularity in percent, 0 to 100.
	uint32_t age_months = 0;                ///< Age of the product.
	Money income_last = 0;                  ///< Income of the last month.
	Money cost_last = 0;                    ///< Production cost of the last month.

	ForkUserProduct(ForkUserProductID index = ForkUserProductID::Invalid()) : ForkUserProductPool::PoolItem<&_fork_user_product_pool>(index) {}

	/** @return The industry type of this product. */
	ForkIndustryType GetIndustryType() const;
};

/** @return What one unit of the product is worth when the company is sold. */
Money GetForkUserProductValue(const ForkUserProduct *p);

/** @return The sale price of a whole company, including its products and firms. */
Money GetForkCompanySalePrice(CompanyID company);

/** Monthly update of popularity, income and costs of all player products. */
void ForkProductsMonthly();

/** Remove products when a company disappears. */
void TransferForkUserProducts(Owner old_owner, Owner new_owner);

/** Windows. */
void ShowForkCompanyWindow(CompanyID company);
void ShowForkProductsWindow();
void ShowForkProductStatsWindow();

#endif /* FORK_PRODUCTS_H */
