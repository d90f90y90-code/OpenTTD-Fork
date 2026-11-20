/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/**
 * @file fork_products.cpp User-created products.
 *
 * A company is no longer locked to one "technology" product. It picks an industry and then makes any
 * product of that industry whose invention year has been reached. Prices, quality and discounts decide
 * how popular a product is; the popularity decides the monthly sales. The catalogue deliberately keeps
 * products closed until their real invention year.
 */

#include "stdafx.h"

#include <algorithm>

#include "command_func.h"
#include "company_base.h"
#include "company_func.h"
#include "core/pool_func.hpp"
#include "economy_func.h"
#include "fork_economy.h"
#include "fork_firm.h"
#include "fork_products.h"
#include "fork_products_cmd.h"
#include "fork_valuation.h"
#include "string_func.h"
#include "timer/timer_game_calendar.h"
#include "town.h"
#include "window_func.h"

#include "table/strings.h"

#include "safeguards.h"

ForkUserProductPool _fork_user_product_pool("ForkUserProduct");
INSTANTIATE_POOL_METHODS(ForkUserProduct)

/** Shorthand for a catalogue entry. */
#define FP(en, ar, ty, yr, price, demand) { en, ar, ForkIndustryType::ty, yr, price, demand },

const ForkCatalogEntry _fork_catalog[] = {
	/* Household appliances. */
	FP("Microwave", "مايكرويف", Household, 1970, 320, 45)
	FP("Electric oven", "فرن كهربائي", Household, 1913, 280, 40)
	FP("Lawn mower", "قصاصة أعشاب", Household, 1830, 180, 50)
	FP("Home appliances", "معدات منزلية", Household, 1900, 240, 42)
	FP("Dishwasher", "غسالة صحون", Household, 1924, 420, 38)
	FP("Washing machine", "غسالة ملابس", Household, 1908, 480, 40)
	FP("Vacuum cleaner", "مكنسة كهربائية", Household, 1901, 210, 44)
	FP("Refrigerator", "ثلاجة", Household, 1913, 520, 46)
	FP("Freezer", "مجمّد", Household, 1918, 390, 36)
	FP("Blender", "خلاط", Household, 1922, 120, 55)
	FP("Electric kettle", "غلاية كهربائية", Household, 1893, 80, 60)
	FP("Coffee machine", "ماكينة قهوة", Household, 1940, 260, 42)
	FP("Iron", "مكواة", Household, 1882, 95, 58)
	FP("Air conditioner", "مكيّف هواء", Household, 1902, 680, 34)

	/* Consumer electronics. */
	FP("Telephone", "هاتف", LifeTechnology, 1876, 160, 55)
	FP("Tablet", "جهاز لوحي", LifeTechnology, 1990, 520, 48)
	FP("Assembled PC", "PC مجمع", LifeTechnology, 1975, 760, 50)
	FP("Laptop", "لابتوب", LifeTechnology, 1981, 890, 48)
	FP("Television", "شاشة / تلفاز", LifeTechnology, 1927, 650, 40)
	FP("Printer", "طابعة", LifeTechnology, 1953, 310, 42)
	FP("Game console", "كونسل", LifeTechnology, 1972, 430, 44)
	FP("Smart watch", "ساعة ذكية", LifeTechnology, 2010, 610, 38)
	FP("Camera", "كاميرا", LifeTechnology, 1880, 340, 44)
	FP("Headphones", "سماعات رأس", LifeTechnology, 1906, 140, 56)
	FP("Earbuds", "هيدفونات", LifeTechnology, 2001, 120, 60)

	/* Communications. */
	FP("SIM card", "شريحة SIM", Communications, 1991, 45, 70)
	FP("Portable router", "راوتر محمول", Communications, 2004, 290, 48)
	FP("Fixed router", "راوتر ثابت", Communications, 1990, 220, 50)
	FP("Wi-Fi booster", "مقوّي إشارة WiFi", Communications, 1997, 130, 55)
	FP("Signal antenna", "هوائي إشارة", Communications, 1895, 180, 52)
	FP("Landline phone", "هاتف أرضي", Communications, 1876, 150, 58)

	/* Drinks. */
	FP("Juice", "عصائر", Beverages, 1900, 35, 75)
	FP("Soda", "مشروب غازي", Beverages, 1886, 28, 82)
	FP("Diet soda", "مشروبات غازية دايت", Beverages, 1962, 34, 70)
	FP("Energy drink", "مشروب طاقة", Beverages, 1987, 48, 64)
	FP("Flavoured drink", "مشروب منكهة", Beverages, 1920, 30, 76)

	/* Food. */
	FP("Candy", "حلويات", Food, 1837, 25, 80)
	FP("Cheese", "أجبان", Food, 1850, 55, 70)
	FP("Canned food", "معلبات", Food, 1810, 42, 72)
	FP("Ready meals", "وجبات جاهزة", Food, 1950, 85, 66)
	FP("Frozen food", "مأكولات مجمّدة", Food, 1925, 72, 68)

	/* Clothes and fabrics. */
	FP("Shirts", "قمصان", Clothing, 1820, 45, 74)
	FP("Trousers", "بناطيل", Clothing, 1825, 52, 72)
	FP("Shorts", "سراويل", Clothing, 1890, 38, 68)
	FP("Underwear", "ملابس داخلية", Clothing, 1850, 34, 76)
	FP("Abaya", "عبائات", Clothing, 1900, 120, 55)
	FP("Hijab", "حجابات", Clothing, 1900, 60, 62)
	FP("Shoes", "أحذية", Clothing, 1800, 68, 70)
	FP("Bags", "حقائب", Clothing, 1840, 90, 60)
	FP("Fabric", "أقمشة", Clothing, 1800, 30, 78)
	FP("Accessories", "إكسسوارات", Clothing, 1880, 75, 58)

	/* Medicine. */
	FP("Medicines", "أدوية", Medical, 1850, 140, 68)
	FP("Medical tools", "أدوات طبية", Medical, 1810, 110, 64)
	FP("Masks", "كمامات", Medical, 1897, 22, 80)
	FP("Blood pressure monitor", "جهاز ضغط الدم", Medical, 1901, 180, 52)
	FP("Glucose meter", "جهاز سكري", Medical, 1970, 240, 50)
	FP("Thermometer", "جهاز فحص الحرارة", Medical, 1866, 65, 70)
	FP("Healthy toothpaste", "معجون أسنان صحي", Medical, 1900, 28, 82)
	FP("Healthy soap", "صابون صحي", Medical, 1800, 18, 88)
	FP("Healthy shampoo", "شامبو صحي", Medical, 1927, 32, 78)
	FP("Sanitiser", "معقمات", Medical, 2002, 26, 84)

	/* Game development. */
	FP("Game industry", "صناعة الألعاب", GameDevelopment, 1972, 180, 58)
	FP("App industry", "صناعة التطبيقات", GameDevelopment, 2000, 220, 56)
	FP("Action game", "لعبة أكشن", GameDevelopment, 1974, 120, 66)
	FP("Strategy game", "لعبة استراتيجية", GameDevelopment, 1980, 135, 62)
	FP("Horror game", "لعبة رعب", GameDevelopment, 1993, 130, 60)
	FP("Simulation game", "لعبة محاكاة", GameDevelopment, 1982, 145, 60)
	FP("Science fiction game", "لعبة خيال علمي", GameDevelopment, 1985, 150, 58)
	FP("Nineties classic", "لعبة كلاسيكية تسعينات", GameDevelopment, 1990, 110, 64)
	FP("Odd game", "لعبة غريبة", GameDevelopment, 1996, 95, 54)
	FP("Sports game", "لعبة رياضة", GameDevelopment, 1983, 125, 66)
	FP("Puzzle game", "لعبة ألغاز", GameDevelopment, 1981, 90, 70)

	/* Software. */
	FP("Short social app", "برنامج شورتات اجتماعي", AppDevelopment, 2018, 95, 68)
	FP("Messaging app", "برنامج مراسلة اجتماعي", AppDevelopment, 1996, 105, 72)
	FP("Marketplace app", "برنامج بيع وشراء منتجات", AppDevelopment, 1999, 160, 66)
	FP("Social network", "برنامج تواصل اجتماعي عام", AppDevelopment, 2003, 140, 74)
	FP("Random call app", "برنامج مكالمات عشوائية", AppDevelopment, 2011, 80, 60)
	FP("Student teaching app", "برنامج تدريس طلابي", AppDevelopment, 2005, 150, 62)
	FP("Video design app", "برنامج تصميم فيديو", AppDevelopment, 2006, 210, 58)
	FP("Job search app", "برنامج بحث عن وظائف", AppDevelopment, 2004, 130, 64)
	FP("File manager", "برنامج ملفات", AppDevelopment, 1981, 70, 76)
	FP("Programming tool", "برنامج برمجة", AppDevelopment, 1955, 190, 55)
	FP("Game engine", "محرك ألعاب", AppDevelopment, 1993, 280, 50)
	FP("Image editor", "برنامج تصميم صور وتعديلها", AppDevelopment, 1990, 170, 60)
	FP("Music app", "برنامج موسيقى", AppDevelopment, 2001, 120, 68)
	FP("Maps app", "برنامج خرائط", AppDevelopment, 2005, 165, 64)
	FP("Translation app", "برنامج ترجمة", AppDevelopment, 2006, 155, 62)
	FP("Notes app", "برنامج تدوين ملاحظات", AppDevelopment, 1995, 60, 78)

	/* Building materials. */
	FP("Cement", "إسمنت", BuildingMaterials, 1824, 48, 72)
	FP("Bricks", "طابوق", BuildingMaterials, 1800, 26, 80)
	FP("Aerated blocks", "أيبلوك", BuildingMaterials, 1923, 40, 68)
	FP("Glass", "زجاج", BuildingMaterials, 1800, 36, 76)
	FP("Paints", "دهانات", BuildingMaterials, 1860, 52, 70)
	FP("Steel", "شيلمان", BuildingMaterials, 1856, 88, 66)
	FP("Construction equipment", "معدات بناء", BuildingMaterials, 1900, 360, 42)

	/* Energy. */
	FP("Solar panels", "ألواح شمسية", Energy, 1954, 420, 48)
	FP("Batteries", "بطاريات", Energy, 1800, 62, 74)
	FP("Electric generator", "مولدات كهربائية", Energy, 1831, 540, 40)
	FP("Chargers", "شواحن", Energy, 1901, 85, 70)
};

#undef FP

static_assert(lengthof(_fork_catalog) == FORK_CATALOG_COUNT, "Fork catalogue size must match FORK_CATALOG_COUNT");

const ForkCatalogEntry *GetForkCatalogEntry(uint16_t id)
{
	return id < FORK_CATALOG_COUNT ? &_fork_catalog[id] : nullptr;
}

uint32_t ForkProductCurrentYear()
{
	return static_cast<uint32_t>(TimerGameCalendar::year.base());
}

bool IsForkProductUnlocked(uint16_t id, uint32_t year)
{
	const ForkCatalogEntry *entry = GetForkCatalogEntry(id);
	return entry != nullptr && year >= entry->unlock_year;
}

ForkIndustryType ForkUserProduct::GetIndustryType() const
{
	const ForkCatalogEntry *entry = GetForkCatalogEntry(this->catalog_id);
	return entry != nullptr ? entry->type : ForkIndustryType::End;
}

Money GetForkUserProductValue(const ForkUserProduct *p)
{
	if (p == nullptr) return 0;
	const ForkCatalogEntry *entry = GetForkCatalogEntry(p->catalog_id);
	if (entry == nullptr) return 0;

	/* A product is worth its stock, its popularity and its yearly profit. */
	Money value = static_cast<Money>(entry->base_price) * std::max<uint>(p->quantity, 1) / 2;
	value += p->income_last * 6;
	value = value * (50 + p->popularity) / 100;
	return std::max<Money>(0, value);
}

/** @return The largest town population in the world; used as a fallback city input of the valuation. */
static uint32_t LargestTownPopulation()
{
	uint32_t best = 1000;
	for (const Town *t : Town::Iterate()) best = std::max<uint32_t>(best, t->cache.population);
	return best;
}

/** @return The town of the company's first firm, or nullptr when it has none. */
static const Town *CompanyTown(CompanyID company)
{
	for (const ForkFirm *f : ForkFirm::Iterate()) {
		if (f->owner == company) return Town::GetIfValid(f->GetTown());
	}
	return nullptr;
}

Money GetForkCompanySalePrice(CompanyID company)
{
	if (!Company::IsValidID(company)) return 0;
	const Company *c = Company::Get(company);

	Money assets = std::max<Money>(0, c->money);
	int64_t monthly_production = 0;
	Money monthly_income = 0;

	for (const ForkFirm *f : ForkFirm::Iterate()) {
		if (f->owner != company) continue;
		assets += std::max<Money>(0, f->value);
		monthly_production += f->made_last;
		monthly_income += f->profit_last;
	}
	for (const ForkUserProduct *p : ForkUserProduct::Iterate()) {
		if (p->owner != company) continue;
		assets += GetForkUserProductValue(p);
		monthly_production += p->quantity;
		monthly_income += p->income_last - p->cost_last;
	}

	const Town *town = CompanyTown(company);
	ForkCompanyValueInput input{};
	input.assets = assets;
	input.monthly_production = monthly_production;
	input.monthly_income = monthly_income;
	input.city_population = ForkValueCityPopulation(town);
	input.city_rating = ForkValueCityRating(town);
	if (input.city_population == 0) {
		input.city_population = LargestTownPopulation();
		input.city_rating = static_cast<uint8_t>(std::min<uint32_t>(100, input.city_population / 100));
	}

	return CalculateForkCompanySalePrice(input);
}

void TransferForkUserProducts(Owner old_owner, Owner new_owner)
{
	for (ForkUserProduct *p : ForkUserProduct::Iterate()) {
		if (p->owner != old_owner) continue;
		if (Company::IsValidID(new_owner) && new_owner != old_owner) {
			p->owner = new_owner;
		} else {
			delete p;
		}
	}
}

void ForkProductsMonthly()
{
	for (ForkUserProduct *p : ForkUserProduct::Iterate()) {
		if (!Company::IsValidID(p->owner)) continue;

		const ForkCatalogEntry *entry = GetForkCatalogEntry(p->catalog_id);
		if (entry == nullptr) continue;

		p->age_months++;

		/* A fair price is close to the base price. Expensive products are less popular, discounts help. */
		const uint32_t fair = std::max<uint32_t>(1, entry->base_price);
		int price_ratio = static_cast<int>(p->price) * 100 / static_cast<int>(fair);
		long long appeal = 50;
		appeal += static_cast<int>(p->quality) * 8;
		appeal += p->discount / 2;
		appeal -= (price_ratio - 100) / 4;
		appeal += static_cast<int>(std::min<uint32_t>(100, LargestTownPopulation() / 100)) / 5;
		p->popularity = static_cast<uint16_t>(std::clamp<long long>(appeal, 1, 100));

		const uint32_t sold = std::max<uint32_t>(1, static_cast<uint32_t>(static_cast<uint64_t>(p->quantity) * p->popularity / 100));
		const Money price = static_cast<Money>(std::max<uint32_t>(1, p->price)) * (100 - p->discount) / 100;
		p->income_last = price * static_cast<Money>(sold);
		p->cost_last = static_cast<Money>(entry->base_price) * static_cast<Money>(p->quantity) / 2;

		ForkLedgerBook(p->owner, ForkLedgerCategory::Trade, p->income_last - p->cost_last);
	}

	SetWindowClassesDirty(WindowClass::ForkProducts);
	SetWindowClassesDirty(WindowClass::ForkProductStats);
	InvalidateWindowData(WindowClass::ForkBusiness, 0);
}

CommandCost CmdCreateForkProduct(DoCommandFlags flags, uint16_t catalog_id, uint32_t quantity, uint32_t price, const std::string &name)
{
	if (!Company::IsValidID(_current_company)) return CMD_ERROR;
	const ForkCatalogEntry *entry = GetForkCatalogEntry(catalog_id);
	if (entry == nullptr || !IsForkProductUnlocked(catalog_id, ForkProductCurrentYear())) return CMD_ERROR;
	if (quantity == 0 || quantity > 10000 || price == 0 || price > 1000000) return CMD_ERROR;
	if (name.empty() || Utf8StringLength(name) >= MAX_LENGTH_COMPANY_NAME_CHARS) return CMD_ERROR;
	if (!ForkUserProduct::CanAllocateItem()) return CommandCost(STR_ERROR_FORK_TOO_MANY_ASSETS);

	CommandCost cost(ExpensesType::Other, GetForkFirmFoundingCost());

	if (flags.Test(DoCommandFlag::Execute)) {
		ForkUserProduct *p = ForkUserProduct::Create();
		p->owner = _current_company;
		p->catalog_id = catalog_id;
		p->name = name;
		p->price = price;
		p->quantity = quantity;
		p->quality = 3;
		p->discount = 0;
		p->popularity = 50;
		SetWindowClassesDirty(WindowClass::ForkProducts);
	}

	return cost;
}

CommandCost CmdSetForkProduct(DoCommandFlags flags, uint32_t product_index, uint8_t what, uint32_t value)
{
	ForkUserProduct *p = ForkUserProduct::GetIfValid(static_cast<ForkUserProductID>(product_index));
	if (p == nullptr || p->owner != _current_company) return CMD_ERROR;

	switch (what) {
		case 0: if (value == 0 || value > 10000) return CMD_ERROR; break;
		case 1: if (value == 0 || value > 1000000) return CMD_ERROR; break;
		case 2: if (value < 1 || value > 5) return CMD_ERROR; break;
		case 3: if (value > 50 || value % 5 != 0) return CMD_ERROR; break;
		default: return CMD_ERROR;
	}

	if (flags.Test(DoCommandFlag::Execute)) {
		switch (what) {
			case 0: p->quantity = value; break;
			case 1: p->price = value; break;
			case 2: p->quality = static_cast<uint8_t>(value); break;
			case 3: p->discount = static_cast<uint8_t>(value); break;
		}
		SetWindowClassesDirty(WindowClass::ForkProducts);
	}

	return CommandCost();
}

CommandCost CmdRenameForkProduct(DoCommandFlags flags, uint32_t product_index, const std::string &name)
{
	ForkUserProduct *p = ForkUserProduct::GetIfValid(static_cast<ForkUserProductID>(product_index));
	if (p == nullptr || p->owner != _current_company || name.empty()) return CMD_ERROR;
	if (Utf8StringLength(name) >= MAX_LENGTH_COMPANY_NAME_CHARS) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		p->name = name;
		SetWindowClassesDirty(WindowClass::ForkProducts);
	}

	return CommandCost();
}

/**
 * Sell the whole company: all firms and all products are valued with the new valuation and the money is
 * paid to the company. The company stays alive as a vehicle so the savegame and other systems remain valid.
 */
CommandCost CmdSellForkCompany(DoCommandFlags flags, CompanyID company)
{
	if (!Company::IsValidID(company) || company != _current_company) return CMD_ERROR;

	const Money price = GetForkCompanySalePrice(company);
	if (price <= 0) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		SubtractMoneyFromCompany(company, CommandCost(ExpensesType::Other, -price));
		TransferForkAssets(company, CompanyID::Invalid());
		for (ForkFirm *f : ForkFirm::Iterate()) {
			if (f->owner != company) continue;
			f->owner = CompanyID::Invalid();
			f->value = 0;
		}
		SetWindowClassesDirty(WindowClass::ForkFirms);
		SetWindowClassesDirty(WindowClass::ForkProducts);
		InvalidateWindowData(WindowClass::ForkBusiness, 0);
	}

	return CommandCost();
}
