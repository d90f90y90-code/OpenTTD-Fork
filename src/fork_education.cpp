/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_education.cpp Private education: companies run schools and universities in towns and earn tuition from the students. */

#include "stdafx.h"

#include <bit>

#include "command_func.h"
#include "company_base.h"
#include "company_func.h"
#include "fork_economy.h"
#include "fork_education_cmd.h"
#include "strings_func.h"
#include "timer/timer_game_economy.h"
#include "town.h"
#include "window_func.h"

#include "table/strings.h"

#include "safeguards.h"

static constexpr uint FORK_FACULTY_SEATS = 200; ///< Seats of one faculty.
static constexpr uint FORK_FACULTY_FEE_MIN = 100; ///< Lowest yearly tuition of a faculty.
static constexpr uint FORK_FACULTY_FEE_MAX = 1000; ///< Highest yearly tuition of a faculty.
static constexpr uint FORK_FACULTY_FEE_STEP = 50; ///< Tuition is changed in steps of this much.
static constexpr uint FORK_FACULTY_FEE_DEFAULT_INDEX = 8; ///< Tuition step a new faculty starts with (500).
static constexpr uint FORK_SELL_PERCENT = 70; ///< An institution is sold for this part of its value.
static constexpr uint FORK_FACULTY_REFUND_PERCENT = 50; ///< Closing a faculty gives back this part of its cost.
static constexpr uint FORK_UPKEEP_PERMILLE = 3; ///< Monthly upkeep of the buildings, in permille of what was invested.

/** Fixed properties of the kinds of institutions. */
struct ForkSchoolSpec {
	Money build_cost; ///< Price of building it.
	Money upgrade_cost; ///< Price of enlarging it (schools only).
	uint seats; ///< Seats of a school.
	uint seats_upgraded; ///< Seats of a school after it was enlarged.
	uint fee_max; ///< Highest fee per seat and year; the lowest is 1.
	uint fee_default; ///< Fee a new institution starts with.
	uint cost_per_seat; ///< Yearly cost of teaching one student.
	uint pool_percent; ///< Part of the inhabitants that may want to go to a private institution of this kind.
	uint min_population; ///< Inhabitants a town needs before it can be built.
	uint tax_min; ///< Lowest yearly tax in percent of the income.
	uint tax_max; ///< Highest yearly tax in percent of the income.
	uint content_base; ///< What is taught, in percent, for a basic school.
};

static constexpr ForkSchoolSpec FORK_SCHOOL_SPECS[] = {
	/* Primary   */ {10000, 5000, 250, 400, 50, 25, 25, 7, 300, 5, 10, 60},
	/* Secondary */ {25000, 12000, 300, 500, 100, 50, 45, 5, 650, 5, 10, 55},
	/* University */ {60000, 0, 0, 0, FORK_FACULTY_FEE_MAX, 0, 400, 4, 1300, 8, 20, 35},
};

/** Fixed properties of the faculties of a university. */
struct ForkFacultySpec {
	Money cost; ///< Price of opening the faculty.
	uint desire_min; ///< Lowest interest of people in the faculty, in percent.
	uint desire_max; ///< Highest interest of people in the faculty, in percent.
};

static constexpr ForkFacultySpec FORK_FACULTY_SPECS[FORK_FACULTIES] = {
	{20000, 85, 95}, // Medicine
	{20000, 80, 90}, // Pharmacy
	{35000, 90, 100}, // Engineering
	{25000, 80, 95}, // Engineering technologies
	{15000, 80, 85}, // Teaching sciences
	{1000, 45, 50}, // Education
	{3000, 55, 66}, // Arts
	{10000, 70, 80}, // Law
	{5000, 60, 70}, // Management and economics
	{2500, 50, 60}, // Vocational education
};

/** @return The specification of a kind of institution. */
static const ForkSchoolSpec &Spec(ForkSchoolKind kind)
{
	return FORK_SCHOOL_SPECS[static_cast<size_t>(kind)];
}

/** @return The kind of an institution. */
static ForkSchoolKind KindOf(const ForkAsset *a)
{
	return static_cast<ForkSchoolKind>(std::min<int64_t>(a->data[0], static_cast<int64_t>(ForkSchoolKind::University)));
}

/** Mix some numbers into a pseudo random number that is the same on every computer. */
static uint32_t Mix(uint32_t a, uint32_t b, uint32_t c)
{
	uint32_t h = a * 2654435761U ^ (b + 0x9E37U) * 40503U ^ (c + 7U) * 2246822519U;
	h ^= h >> 15;
	h *= 2246822519U;
	h ^= h >> 13;
	return h;
}

/** @return The current year, for things that change every year. */
static uint32_t CurrentYear()
{
	return static_cast<uint32_t>(TimerGameEconomy::year.base());
}

/** @return Tax rate in percent of the income that a town asks of this kind of institution this year. */
static uint TaxPercent(const Town *t, ForkSchoolKind kind)
{
	const ForkSchoolSpec &s = Spec(kind);
	return s.tax_min + Mix(t->index.base(), 100 + static_cast<uint>(kind), CurrentYear()) % (s.tax_max - s.tax_min + 1);
}

uint GetForkFacultyDesire(const Town *t, uint faculty)
{
	const ForkFacultySpec &f = FORK_FACULTY_SPECS[faculty % FORK_FACULTIES];
	return f.desire_min + Mix(t->index.base(), faculty, CurrentYear()) % (f.desire_max - f.desire_min + 1);
}

Money GetForkFacultyCost(uint faculty)
{
	return FORK_FACULTY_SPECS[faculty % FORK_FACULTIES].cost;
}

StringID GetForkFacultyName(uint faculty)
{
	return STR_FORK_FACULTY_MEDICINE + (faculty % FORK_FACULTIES);
}

Money GetForkSchoolBuildCost(ForkSchoolKind kind) { return Spec(kind).build_cost; }
Money GetForkSchoolUpgradeCost(ForkSchoolKind kind) { return Spec(kind).upgrade_cost; }
uint GetForkSchoolMinPopulation(ForkSchoolKind kind) { return Spec(kind).min_population; }
uint GetForkSchoolFeeMax(ForkSchoolKind kind) { return Spec(kind).fee_max; }

bool IsForkSchoolUpgraded(const ForkAsset *a) { return (a->data[1] & 2) != 0; }
bool IsForkSchoolDoubleShift(const ForkAsset *a) { return (a->data[1] & 1) != 0; }

uint GetForkSchoolFee(const ForkAsset *a)
{
	return std::clamp<uint>(static_cast<uint>(std::max<int64_t>(a->data[2], 0)), 1, Spec(KindOf(a)).fee_max);
}

uint GetForkFacultyOpenMask(const ForkAsset *a)
{
	return static_cast<uint>(a->data[3]) & ((1U << FORK_FACULTIES) - 1);
}

uint GetForkFacultyFee(const ForkAsset *a, uint faculty)
{
	const uint index = static_cast<uint>(a->data2[0] >> (5 * (faculty % FORK_FACULTIES))) & 31;
	return std::min(FORK_FACULTY_FEE_MIN + index * FORK_FACULTY_FEE_STEP, FORK_FACULTY_FEE_MAX);
}

/** Set the tuition of a faculty (a multiple of the step, within the limits). */
static void SetFacultyFee(ForkAsset *a, uint faculty, uint fee)
{
	const int64_t index = (fee - FORK_FACULTY_FEE_MIN) / FORK_FACULTY_FEE_STEP;
	const int shift = 5 * static_cast<int>(faculty);
	a->data2[0] = (a->data2[0] & ~(static_cast<int64_t>(31) << shift)) | (index << shift);
}

std::string GetForkSchoolName(const ForkAsset *a)
{
	if (!a->name.empty()) return a->name;
	return GetString(STR_FORK_EDU_NAME_PRIMARY + static_cast<uint>(KindOf(a)));
}

/** @return Number of faculties that are open. */
static uint OpenFaculties(const ForkAsset *a)
{
	return static_cast<uint>(std::popcount(GetForkFacultyOpenMask(a)));
}

/** @return Whether this asset is an institution of the company in the town. */
static bool IsSchoolOf(const ForkAsset *a, CompanyID company, TownID town, ForkSchoolKind kind)
{
	return a->owner == company && a->type == static_cast<uint8_t>(ForkAssetType::Education) &&
			a->location == town.base() && KindOf(a) == kind;
}

const ForkAsset *GetForkSchool(CompanyID company, TownID town, ForkSchoolKind kind)
{
	for (const ForkAsset *a : ForkAsset::Iterate()) {
		if (IsSchoolOf(a, company, town, kind)) return a;
	}
	return nullptr;
}

/**
 * Quality of an institution: age of the buildings, what is taught, and how pleased the town is with the company.
 * @return Quality in percent.
 */
static uint SchoolQuality(const Town *t, const ForkAsset *a)
{
	const ForkSchoolKind kind = KindOf(a);
	const uint age = std::max<int>(40, 100 - static_cast<int>(a->age_months / 6));
	uint content = Spec(kind).content_base;
	if (kind == ForkSchoolKind::University) {
		content = std::min<uint>(100, content + 8 * OpenFaculties(a));
	} else if (IsForkSchoolUpgraded(a)) {
		content = std::min<uint>(100, content + 25);
	}
	const int rating = std::clamp<int>(t->ratings[a->owner], -1000, 1000);
	const uint satisfaction = static_cast<uint>((rating + 1000) / 20);
	return (age * 3 + content * 4 + satisfaction * 3) / 10;
}

/**
 * Popularity: the quality weighed against what is asked for. Asking more than the quality is worth drives people away.
 * @param quality Quality in percent.
 * @param fee Fee that is asked.
 * @param fee_max Highest fee possible.
 * @param fair_base Fair fee for the lowest quality, in percent of the highest fee.
 * @param fair_span Rise of the fair fee for the highest quality, in percent of the highest fee.
 * @return Popularity in percent.
 */
static uint Popularity(uint quality, uint fee, uint fee_max, uint fair_base, uint fair_span)
{
	const uint fair = std::max<uint>(1, fee_max * (fair_base + fair_span * quality / 100) / 100);
	const int ratio = static_cast<int>(fee * 100 / fair);
	const int multiplier = std::clamp(100 + (100 - ratio) * 12 / 10, 5, 160);
	return std::min<uint>(100, quality * multiplier / 100);
}

/** @return Popularity of a school (primary or secondary). */
static uint SchoolPopularity(const Town *t, const ForkAsset *a)
{
	return Popularity(SchoolQuality(t, a), GetForkSchoolFee(a), Spec(KindOf(a)).fee_max, 30, 50);
}

/** @return Popularity of one faculty of a university. */
static uint FacultyPopularity(const Town *t, const ForkAsset *a, uint faculty)
{
	return Popularity(SchoolQuality(t, a), GetForkFacultyFee(a, faculty), FORK_FACULTY_FEE_MAX, 25, 45);
}

ForkSchoolResult GetForkSchoolResult(const Town *t, const ForkAsset *a)
{
	ForkSchoolResult r;
	const ForkSchoolKind kind = KindOf(a);
	const ForkSchoolSpec &spec = Spec(kind);
	const uint shift = IsForkSchoolDoubleShift(a) ? 2 : 1;
	const uint64_t population = t->cache.population;

	r.quality = SchoolQuality(t, a);
	r.tax_percent = TaxPercent(t, kind);

	Money income = 0;
	uint popularity_sum = 0;
	uint popularity_count = 0;

	if (kind != ForkSchoolKind::University) {
		r.capacity = (IsForkSchoolUpgraded(a) ? spec.seats_upgraded : spec.seats) * shift;
		const uint own = SchoolPopularity(t, a);
		uint64_t others = 0;
		for (const ForkAsset *o : ForkAsset::Iterate()) {
			if (o == a || o->owner == a->owner || o->type != static_cast<uint8_t>(ForkAssetType::Education)) continue;
			if (o->location != a->location || KindOf(o) != kind) continue;
			others += SchoolPopularity(t, o);
		}
		const uint64_t share = own == 0 ? 0 : static_cast<uint64_t>(own) * 1000 / (own + others);
		const uint64_t pool = population * spec.pool_percent / 100;
		r.enrolled = static_cast<uint>(std::min<uint64_t>(r.capacity, pool * share / 1000 * own / 100));
		r.popularity = own;
		income = static_cast<Money>(r.enrolled) * GetForkSchoolFee(a) / 12;
	} else {
		for (uint f = 0; f < FORK_FACULTIES; f++) {
			if (!(GetForkFacultyOpenMask(a) & (1U << f))) continue;
			const uint own = FacultyPopularity(t, a, f);
			uint64_t others = 0;
			for (const ForkAsset *o : ForkAsset::Iterate()) {
				if (o == a || o->owner == a->owner || o->type != static_cast<uint8_t>(ForkAssetType::Education)) continue;
				if (o->location != a->location || KindOf(o) != kind || !(GetForkFacultyOpenMask(o) & (1U << f))) continue;
				others += FacultyPopularity(t, o, f);
			}
			const uint64_t share = own == 0 ? 0 : static_cast<uint64_t>(own) * 1000 / (own + others);
			const uint64_t pool = population * spec.pool_percent / 100 * GetForkFacultyDesire(t, f) / 700;
			const uint seats = FORK_FACULTY_SEATS * shift;
			const uint enrolled = static_cast<uint>(std::min<uint64_t>(seats, pool * share / 1000 * own / 100));

			r.fac_enrolled[f] = enrolled;
			r.fac_popularity[f] = own;
			r.capacity += seats;
			r.enrolled += enrolled;
			popularity_sum += own;
			popularity_count++;
			income += static_cast<Money>(enrolled) * GetForkFacultyFee(a, f) / 12;
		}
		r.popularity = popularity_count == 0 ? 0 : popularity_sum / popularity_count;
	}

	/* Teaching costs depend on the number of students; the buildings cost upkeep. A double shift needs twice the teachers and staff,
	 * so it doubles all running costs, and it takes twice the students (morning and afternoon shift). */
	Money running = static_cast<Money>(r.enrolled) * spec.cost_per_seat / 12 + a->purchase_price * FORK_UPKEEP_PERMILLE / 1000;
	running = running * shift * GetForkMarketFactor(ForkMarketFactor::Wages) / 100;

	r.income = income;
	r.running = running;
	r.tax = income * r.tax_percent / 100;
	return r;
}

Money GetForkSchoolSellValue(const ForkAsset *a)
{
	return a->value * FORK_SELL_PERCENT / 100;
}

/** Bring value and income of an institution up to date. */
static void RefreshSchool(ForkAsset *a, const Town *t)
{
	const ForkSchoolResult r = GetForkSchoolResult(t, a);
	a->value = a->purchase_price * std::max<int>(50, 100 - static_cast<int>(a->age_months / 10)) / 100;
	a->monthly_income = r.income - r.running - r.tax;
	a->monthly_cost = r.running + r.tax;
	a->data2[1] = r.enrolled;
	a->data2[2] = r.popularity;
}

/** Tell the open windows that something changed. */
static void SchoolsChanged()
{
	SetWindowClassesDirty(WindowClass::ForkEducation);
	InvalidateWindowData(WindowClass::ForkBusiness, 0);
}

/** Find an institution of the current company, for a command. */
static ForkAsset *FindSchool(CompanyID company, TownID town, ForkSchoolKind kind)
{
	for (ForkAsset *a : ForkAsset::Iterate()) {
		if (IsSchoolOf(a, company, town, kind)) return a;
	}
	return nullptr;
}

/**
 * Build a private school or university in a town.
 * @param flags Type of operation.
 * @param town_id Town to build in.
 * @param kind Kind of institution, a #ForkSchoolKind.
 * @return The cost of this operation or an error.
 */
CommandCost CmdBuildForkSchool(DoCommandFlags flags, TownID town_id, uint8_t kind)
{
	if (!Company::IsValidID(_current_company) || kind >= static_cast<uint8_t>(ForkSchoolKind::End)) return CMD_ERROR;
	Town *t = Town::GetIfValid(town_id);
	if (t == nullptr) return CMD_ERROR;
	const ForkSchoolKind k = static_cast<ForkSchoolKind>(kind);
	const ForkSchoolSpec &spec = Spec(k);

	if (t->cache.population < spec.min_population) return CommandCost(STR_ERROR_FORK_EDU_TOWN_TOO_SMALL);
	if (FindSchool(_current_company, town_id, k) != nullptr) return CommandCost(STR_ERROR_FORK_EDU_ALREADY_BUILT);
	if (!ForkAsset::CanAllocateItem()) return CommandCost(STR_ERROR_FORK_TOO_MANY_ASSETS);

	CommandCost cost(ExpensesType::Other, spec.build_cost);

	if (flags.Test(DoCommandFlag::Execute)) {
		ForkAsset *a = ForkAsset::Create();
		a->owner = _current_company;
		a->type = static_cast<uint8_t>(ForkAssetType::Education);
		a->location_kind = static_cast<uint8_t>(ForkAssetLocation::Town);
		a->location = town_id.base();
		a->purchase_price = spec.build_cost;
		a->data[0] = kind;
		a->data[1] = 0;
		a->data[2] = spec.fee_default;
		a->data[3] = 0;
		a->data2[0] = 0;
		for (uint f = 0; f < FORK_FACULTIES; f++) SetFacultyFee(a, f, FORK_FACULTY_FEE_MIN + FORK_FACULTY_FEE_DEFAULT_INDEX * FORK_FACULTY_FEE_STEP);
		RefreshSchool(a, t);
		SchoolsChanged();
	}
	return cost;
}

/**
 * Sell a school or university.
 * @param flags Type of operation.
 * @param town_id Town of the institution.
 * @param kind Kind of institution.
 * @return The cost (negative: proceeds) of this operation or an error.
 */
CommandCost CmdSellForkSchool(DoCommandFlags flags, TownID town_id, uint8_t kind)
{
	if (!Company::IsValidID(_current_company) || kind >= static_cast<uint8_t>(ForkSchoolKind::End)) return CMD_ERROR;
	if (Town::GetIfValid(town_id) == nullptr) return CMD_ERROR;

	ForkAsset *a = FindSchool(_current_company, town_id, static_cast<ForkSchoolKind>(kind));
	if (a == nullptr) return CommandCost(STR_ERROR_FORK_EDU_NONE);

	CommandCost cost(ExpensesType::Other, -GetForkSchoolSellValue(a));
	if (flags.Test(DoCommandFlag::Execute)) {
		delete a;
		SchoolsChanged();
	}
	return cost;
}

/**
 * Change settings of a school or university, enlarge it, or open and close faculties.
 * @param flags Type of operation.
 * @param town_id Town of the institution.
 * @param kind Kind of institution.
 * @param action What to do, a #ForkSchoolAction.
 * @param argument Argument of the action.
 * @return The cost of this operation or an error.
 */
CommandCost CmdForkSchoolAction(DoCommandFlags flags, TownID town_id, uint8_t kind, uint8_t action, uint16_t argument)
{
	if (!Company::IsValidID(_current_company) || kind >= static_cast<uint8_t>(ForkSchoolKind::End)) return CMD_ERROR;
	Town *t = Town::GetIfValid(town_id);
	if (t == nullptr) return CMD_ERROR;

	const ForkSchoolKind k = static_cast<ForkSchoolKind>(kind);
	ForkAsset *a = FindSchool(_current_company, town_id, k);
	if (a == nullptr) return CommandCost(STR_ERROR_FORK_EDU_NONE);
	const bool university = k == ForkSchoolKind::University;

	Money cost = 0;
	switch (static_cast<ForkSchoolAction>(action)) {
		case ForkSchoolAction::SetFee:
			if (university) return CommandCost(STR_ERROR_FORK_EDU_FACULTY_ONLY);
			if (argument < 1 || argument > Spec(k).fee_max) return CMD_ERROR;
			if (flags.Test(DoCommandFlag::Execute)) a->data[2] = argument;
			break;

		case ForkSchoolAction::ToggleShift:
			if (flags.Test(DoCommandFlag::Execute)) a->data[1] ^= 1;
			break;

		case ForkSchoolAction::Upgrade:
			if (university) return CommandCost(STR_ERROR_FORK_EDU_FACULTY_ONLY);
			if (IsForkSchoolUpgraded(a)) return CommandCost(STR_ERROR_FORK_EDU_ALREADY_UPGRADED);
			cost = Spec(k).upgrade_cost;
			if (flags.Test(DoCommandFlag::Execute)) {
				a->data[1] |= 2;
				a->purchase_price += cost;
				a->age_months /= 2; // New buildings make the whole school look younger.
			}
			break;

		case ForkSchoolAction::OpenFaculty: {
			if (!university) return CommandCost(STR_ERROR_FORK_EDU_UNIVERSITY_ONLY);
			if (argument >= FORK_FACULTIES) return CMD_ERROR;
			if (GetForkFacultyOpenMask(a) & (1U << argument)) return CommandCost(STR_ERROR_FORK_EDU_FACULTY_OPEN);
			cost = GetForkFacultyCost(argument);
			if (flags.Test(DoCommandFlag::Execute)) {
				a->data[3] |= static_cast<int64_t>(1) << argument;
				a->purchase_price += cost;
			}
			break;
		}

		case ForkSchoolAction::CloseFaculty: {
			if (!university) return CommandCost(STR_ERROR_FORK_EDU_UNIVERSITY_ONLY);
			if (argument >= FORK_FACULTIES) return CMD_ERROR;
			if (!(GetForkFacultyOpenMask(a) & (1U << argument))) return CommandCost(STR_ERROR_FORK_EDU_FACULTY_CLOSED);
			cost = -(GetForkFacultyCost(argument) * FORK_FACULTY_REFUND_PERCENT / 100);
			if (flags.Test(DoCommandFlag::Execute)) {
				a->data[3] &= ~(static_cast<int64_t>(1) << argument);
				a->purchase_price = std::max<Money>(Spec(k).build_cost, a->purchase_price - GetForkFacultyCost(argument));
			}
			break;
		}

		case ForkSchoolAction::SetFacultyFee: {
			if (!university) return CommandCost(STR_ERROR_FORK_EDU_UNIVERSITY_ONLY);
			const uint faculty = argument & 15;
			const uint fee = argument >> 4;
			if (faculty >= FORK_FACULTIES || fee < FORK_FACULTY_FEE_MIN || fee > FORK_FACULTY_FEE_MAX || fee % FORK_FACULTY_FEE_STEP != 0) return CMD_ERROR;
			if (!(GetForkFacultyOpenMask(a) & (1U << faculty))) return CommandCost(STR_ERROR_FORK_EDU_FACULTY_CLOSED);
			if (flags.Test(DoCommandFlag::Execute)) SetFacultyFee(a, faculty, fee);
			break;
		}

		default:
			return CMD_ERROR;
	}

	if (flags.Test(DoCommandFlag::Execute)) {
		RefreshSchool(a, t);
		SchoolsChanged();
	}
	return CommandCost(ExpensesType::Other, cost);
}

/**
 * Rename a school or university.
 * @param flags Type of operation.
 * @param town_id Town of the institution.
 * @param kind Kind of institution.
 * @param text New name; empty to go back to the default name.
 * @return The cost of this operation (none) or an error.
 */
CommandCost CmdRenameForkSchool(DoCommandFlags flags, TownID town_id, uint8_t kind, const std::string &text)
{
	if (!Company::IsValidID(_current_company) || kind >= static_cast<uint8_t>(ForkSchoolKind::End)) return CMD_ERROR;
	if (Town::GetIfValid(town_id) == nullptr || text.size() > 80) return CMD_ERROR;

	ForkAsset *a = FindSchool(_current_company, town_id, static_cast<ForkSchoolKind>(kind));
	if (a == nullptr) return CommandCost(STR_ERROR_FORK_EDU_NONE);

	if (flags.Test(DoCommandFlag::Execute)) {
		a->name = text;
		SchoolsChanged();
	}
	return CommandCost();
}

/** Every month the institutions are revalued; the owner receives the tuition and pays the costs and the tax. */
static void ForkEducationMonthly(ForkAsset *a)
{
	const Town *t = Town::GetIfValid(static_cast<TownID>(a->location));
	if (t == nullptr) {
		a->value = 0;
		a->monthly_income = 0;
		a->monthly_cost = 0;
		return;
	}

	const ForkSchoolResult r = GetForkSchoolResult(t, a);
	RefreshSchool(a, t);
	ForkLedgerBook(a->owner, ForkLedgerCategory::Trade, r.income);
	ForkLedgerBook(a->owner, ForkLedgerCategory::Tax, -r.tax);
	ForkLedgerBook(a->owner, ForkLedgerCategory::Other, -r.running);
	SetWindowClassesDirty(WindowClass::ForkEducation);
}

/** Make the monthly update of schools known to the extended economy. */
void RegisterForkEducation()
{
	RegisterForkAssetHandler(ForkAssetType::Education, ForkEducationMonthly);
}
