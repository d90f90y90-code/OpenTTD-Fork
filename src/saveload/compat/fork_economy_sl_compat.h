/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_economy_sl_compat.h Loading of fork economy chunks from savegames of older fork versions. */

#ifndef SAVELOAD_COMPAT_FORK_ECONOMY_H
#define SAVELOAD_COMPAT_FORK_ECONOMY_H

#include "../saveload.h"

/** Original field order for _fork_economy_desc. */
const SaveLoadCompat _fork_economy_sl_compat[] = {
	SLC_VAR("data_version"),
	SLC_VAR("market_phase"),
	SLC_VAR("market_index"),
	SLC_VAR("market_target"),
	SLC_VAR("firm_ref_price"),
	SLC_VAR("market_months_left"),
	SLC_VAR("ledger_month"),
	SLC_VAR("ledger_last"),
};

/** Original field order for _fork_asset_desc. */
const SaveLoadCompat _fork_asset_sl_compat[] = {
	SLC_VAR("owner"),
	SLC_VAR("type"),
	SLC_VAR("location_kind"),
	SLC_VAR("location"),
	SLC_VAR("purchase_price"),
	SLC_VAR("value"),
	SLC_VAR("monthly_income"),
	SLC_VAR("monthly_cost"),
	SLC_VAR("age_months"),
	SLC_VAR("data"),
};

/** Original field order for _fork_firm_desc. */
const SaveLoadCompat _fork_firm_sl_compat[] = {
	SLC_VAR("name"),
	SLC_VAR("owner"),
	SLC_VAR("town"),
	SLC_VAR("product"),
	SLC_VAR("quality"),
	SLC_VAR("rate"),
	SLC_VAR("price_percent"),
	SLC_VAR("discount"),
	SLC_VAR("personality"),
	SLC_VAR("stock"),
	SLC_VAR("made_last"),
	SLC_VAR("sold_last"),
	SLC_VAR("age_months"),
	SLC_VAR("revenue_last"),
	SLC_VAR("profit_last"),
	SLC_VAR("profit_average"),
	SLC_VAR("value"),
};

#endif /* SAVELOAD_COMPAT_FORK_ECONOMY_H */
