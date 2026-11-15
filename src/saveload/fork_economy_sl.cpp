/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_economy_sl.cpp Code handling saving and loading of the fork's extended economy. */

#include "../stdafx.h"

#include "saveload.h"
#include "saveload_error.hpp"
#include "compat/fork_economy_sl_compat.h"

#include "../fork_economy.h"

#include "../safeguards.h"

/*
 * Both chunks are tables: the field names are stored in the savegame, so fields can be added to them
 * in later versions without a new savegame version; old savegames simply leave the new fields at their default.
 */

static const SaveLoad _fork_economy_desc[] = {
	SaveLoad::Variable<VarFileType::U32>("data_version", SLE_OBJECT_ADDRESS(ForkEconomyState, data_version), SaveLoadVersion::ForkEconomy),
	SaveLoad::Variable<VarFileType::U8>("market_phase", SLE_OBJECT_ADDRESS(ForkEconomyState, market_phase), SaveLoadVersion::ForkEconomy),
	SaveLoad::Variable<VarFileType::I32>("market_index", SLE_OBJECT_ADDRESS(ForkEconomyState, market_index), SaveLoadVersion::ForkEconomy),
	SaveLoad::Variable<VarFileType::I32>("market_target", SLE_OBJECT_ADDRESS(ForkEconomyState, market_target), SaveLoadVersion::ForkEconomy),
	SaveLoad::Variable<VarFileType::U16>("market_months_left", SLE_OBJECT_ADDRESS(ForkEconomyState, market_months_left), SaveLoadVersion::ForkEconomy),
	SaveLoad::Array<VarFileType::I64, FORK_LEDGER_SIZE>("ledger_month", SLE_OBJECT_ADDRESS(ForkEconomyState, ledger_month), SaveLoadVersion::ForkEconomy),
	SaveLoad::Array<VarFileType::I64, FORK_LEDGER_SIZE>("ledger_last", SLE_OBJECT_ADDRESS(ForkEconomyState, ledger_last), SaveLoadVersion::ForkEconomy),
	SaveLoad::Variable<VarFileType::I32>("used_month", SLE_OBJECT_ADDRESS(ForkEconomyState, used_month), SaveLoadVersion::ForkEconomy),
	SaveLoad::Array<VarFileType::U8, MAX_COMPANIES * FORK_USED_SLOTS>("used_sold", SLE_OBJECT_ADDRESS(ForkEconomyState, used_sold), SaveLoadVersion::ForkEconomy),
};

struct FKECChunkHandler : ChunkHandler {
	FKECChunkHandler() : ChunkHandler("FKEC", ChunkType::Table) {}

	void Save() const override
	{
		SlTableHeader(_fork_economy_desc);

		SlSetArrayIndex(0);
		SlObject(&_fork_economy, _fork_economy_desc);
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(_fork_economy_desc, _fork_economy_sl_compat);

		if (SlIterateArray() == -1) return;
		SlObject(&_fork_economy, slt);
		if (SlIterateArray() != -1) SlErrorCorrupt("Too many FKEC entries");
	}
};

static const SaveLoad _fork_asset_desc[] = {
	SaveLoad::Variable<VarFileType::U8>("owner", SLE_OBJECT_ADDRESS(ForkAsset, owner), SaveLoadVersion::ForkEconomy),
	SaveLoad::Variable<VarFileType::U8>("type", SLE_OBJECT_ADDRESS(ForkAsset, type), SaveLoadVersion::ForkEconomy),
	SaveLoad::Variable<VarFileType::U8>("location_kind", SLE_OBJECT_ADDRESS(ForkAsset, location_kind), SaveLoadVersion::ForkEconomy),
	SaveLoad::Variable<VarFileType::U32>("location", SLE_OBJECT_ADDRESS(ForkAsset, location), SaveLoadVersion::ForkEconomy),
	SaveLoad::Variable<VarFileType::I64>("purchase_price", SLE_OBJECT_ADDRESS(ForkAsset, purchase_price), SaveLoadVersion::ForkEconomy),
	SaveLoad::Variable<VarFileType::I64>("value", SLE_OBJECT_ADDRESS(ForkAsset, value), SaveLoadVersion::ForkEconomy),
	SaveLoad::Variable<VarFileType::I64>("monthly_income", SLE_OBJECT_ADDRESS(ForkAsset, monthly_income), SaveLoadVersion::ForkEconomy),
	SaveLoad::Variable<VarFileType::I64>("monthly_cost", SLE_OBJECT_ADDRESS(ForkAsset, monthly_cost), SaveLoadVersion::ForkEconomy),
	SaveLoad::Variable<VarFileType::U32>("age_months", SLE_OBJECT_ADDRESS(ForkAsset, age_months), SaveLoadVersion::ForkEconomy),
	SaveLoad::Array<VarFileType::I64, 4>("data", SLE_OBJECT_ADDRESS(ForkAsset, data), SaveLoadVersion::ForkEconomy),
};

struct FKASChunkHandler : ChunkHandler {
	FKASChunkHandler() : ChunkHandler("FKAS", ChunkType::Table) {}

	void Save() const override
	{
		SlTableHeader(_fork_asset_desc);

		for (ForkAsset *a : ForkAsset::Iterate()) {
			SlSetArrayIndex(a->index);
			SlObject(a, _fork_asset_desc);
		}
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(_fork_asset_desc, _fork_asset_sl_compat);

		int index;
		while ((index = SlIterateArray()) != -1) {
			ForkAsset *a = ForkAsset::CreateAtIndex(ForkAssetID(index));
			SlObject(a, slt);
		}
	}
};

static const FKECChunkHandler FKEC;
static const FKASChunkHandler FKAS;
static const ChunkHandlerRef fork_economy_chunk_handlers[] = {
	FKEC,
	FKAS,
};

extern const ChunkHandlerTable _fork_economy_chunk_handlers(fork_economy_chunk_handlers);
