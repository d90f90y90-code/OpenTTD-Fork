/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_education_gui.cpp Window to run private schools and universities in one town. */

#include "stdafx.h"

#include <bit>

#include "command_func.h"
#include "company_base.h"
#include "company_func.h"
#include "fork_buildings.h"
#include "fork_economy.h"
#include "fork_education_cmd.h"
#include "gfx_func.h"
#include "querystring_gui.h"
#include "strings_func.h"
#include "textbuf_gui.h"
#include "town.h"
#include "window_func.h"
#include "window_gui.h"
#include "zoom_func.h"

#include "table/strings.h"

#include "safeguards.h"

/** Widgets of the education window. */
enum ForkEducationWidgets : WidgetID {
	WID_FED_CAPTION, ///< Caption of the window.
	WID_FED_TAB_PRIMARY, ///< Tab of the primary school.
	WID_FED_TAB_SECONDARY, ///< Tab of the secondary school.
	WID_FED_TAB_UNIVERSITY, ///< Tab of the university.
	WID_FED_INFO, ///< Panel with the texts.
	WID_FED_BUILD, ///< Build the institution.
	WID_FED_SELL, ///< Sell the institution.
	WID_FED_RENAME, ///< Rename the institution.
	WID_FED_UPGRADE, ///< Enlarge the school.
	WID_FED_SHIFT, ///< Switch double shift on or off.
	WID_FED_FACULTY, ///< Open or close the selected faculty.
	WID_FED_FEE_DOWN_BIG, ///< Lower the fee by a lot.
	WID_FED_FEE_DOWN, ///< Lower the fee.
	WID_FED_FEE_UP, ///< Raise the fee.
	WID_FED_FEE_UP_BIG, ///< Raise the fee by a lot.
};

static constexpr uint FED_HEADER_LINES = 9; ///< Text lines above the faculty list.

/** Window to run private education in one town. The window number is the town. */
struct ForkEducationWindow : Window {
	const Town *town; ///< Town of this window.
	ForkSchoolKind kind = ForkSchoolKind::Primary; ///< Tab that is shown.
	uint faculty = 0; ///< Selected faculty of the university.

	ForkEducationWindow(WindowDesc &desc, WindowNumber number) : Window(desc), town(Town::Get(static_cast<TownID>(number)))
	{
		this->InitNested(number);
		this->SetTab(ForkSchoolKind::Primary);
	}

	/** @return The institution that is shown, or nullptr when the company has none of this kind. */
	const ForkAsset *Current() const
	{
		return Company::IsValidID(_local_company) ? GetForkSchool(_local_company, this->town->index, this->kind) : nullptr;
	}

	/** Show another tab. */
	void SetTab(ForkSchoolKind k)
	{
		this->kind = k;
		this->SetWidgetsLoweredState(k == ForkSchoolKind::Primary, WID_FED_TAB_PRIMARY);
		this->SetWidgetsLoweredState(k == ForkSchoolKind::Secondary, WID_FED_TAB_SECONDARY);
		this->SetWidgetsLoweredState(k == ForkSchoolKind::University, WID_FED_TAB_UNIVERSITY);
		this->SetDirty();
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_FED_CAPTION) return GetString(STR_FORK_EDU_CAPTION, this->town->index);
		return this->Window::GetWidgetString(widget, stringid);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget != WID_FED_INFO) return;

		size.width = std::max<uint>(size.width, ScaleGUITrad(420));
		size.height = (FED_HEADER_LINES + FORK_FACULTIES + 1) * GetCharacterHeight(FontSize::Normal) + padding.height;
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, bool gui_scope = true) override
	{
		if (gui_scope) this->SetDirty();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_FED_INFO || !Company::IsValidID(_local_company)) return;

		const Rect ir = r.Shrink(WidgetDimensions::scaled.frametext);
		const int line = GetCharacterHeight(FontSize::Normal);
		int y = ir.top;
		auto draw = [&](std::string &&text) {
			DrawString(ir.left, ir.right, y, text);
			y += line;
		};

		const ForkAsset *a = this->Current();
		if (a == nullptr) {
			draw(GetString(STR_FORK_EDU_NO_INSTITUTION));
			draw(GetString(STR_FORK_EDU_BUILD_INFO, GetForkSchoolBuildCost(this->kind), GetForkSchoolMinPopulation(this->kind), this->town->cache.population));
			return;
		}

		const ForkSchoolResult res = GetForkSchoolResult(this->town, a);
		const bool university = this->kind == ForkSchoolKind::University;

		draw(GetString(STR_FORK_EDU_NAME_LINE, GetForkSchoolName(a), a->value, GetForkSchoolSellValue(a)));
		draw(GetString(STR_FORK_EDU_STATUS, STR_FORK_EDU_STAGE_FOUNDATION + GetForkSchoolStage(a)));
		draw(GetString(STR_FORK_EDU_SEATS, res.enrolled, res.capacity));
		draw(GetString(STR_FORK_EDU_OPTIONS, university ? STR_FORK_EDU_NOT_APPLICABLE : (IsForkSchoolUpgraded(a) ? STR_FORK_EDU_YES : STR_FORK_EDU_NO), IsForkSchoolDoubleShift(a) ? STR_FORK_EDU_YES : STR_FORK_EDU_NO));
		if (university) {
			draw(GetString(STR_FORK_EDU_FACULTIES, std::popcount(GetForkFacultyOpenMask(a)), FORK_FACULTIES));
		} else {
			draw(GetString(STR_FORK_EDU_FEE, GetForkSchoolFee(a), GetForkSchoolFeeMax(this->kind)));
		}
		draw(GetString(STR_FORK_EDU_QUALITY, res.quality, res.popularity));
		draw(GetString(STR_FORK_EDU_INCOME, res.income, res.running));
		draw(GetString(STR_FORK_EDU_TAX, res.tax_percent, res.tax));
		draw(GetString(STR_FORK_EDU_PROFIT, res.income - res.running - res.tax));

		if (!university) return;

		const uint mask = GetForkFacultyOpenMask(a);
		for (uint f = 0; f < FORK_FACULTIES; f++) {
			const std::string marker = f == this->faculty ? ">" : " ";
			const uint desire = GetForkFacultyDesire(this->town, f);
			if (mask & (1U << f)) {
				draw(GetString(STR_FORK_EDU_FACULTY_OPEN_ROW, marker, GetForkFacultyName(f), desire, GetForkFacultyFee(a, f), res.fac_enrolled[f], res.fac_popularity[f]));
			} else {
				draw(GetString(STR_FORK_EDU_FACULTY_CLOSED_ROW, marker, GetForkFacultyName(f), desire, GetForkFacultyCost(f)));
			}
		}
	}

	/** Change the fee of the school, or of the selected faculty. */
	void ChangeFee(int direction, bool big)
	{
		const ForkAsset *a = this->Current();
		if (a == nullptr) return;
		const TownID town_id = this->town->index;

		if (this->kind == ForkSchoolKind::University) {
			const int step = big ? 200 : 50;
			const int fee = std::clamp<int>(static_cast<int>(GetForkFacultyFee(a, this->faculty)) + direction * step, 100, 1000);
			Command<Commands::ForkSchoolAction>::Post(STR_ERROR_FORK_EDU_CAN_T_CHANGE, town_id, static_cast<uint8_t>(this->kind), static_cast<uint8_t>(ForkSchoolAction::SetFacultyFee), static_cast<uint16_t>(this->faculty | (fee << 4)));
		} else {
			const int step = big ? 5 : 1;
			const int fee = std::clamp<int>(static_cast<int>(GetForkSchoolFee(a)) + direction * step, 1, GetForkSchoolFeeMax(this->kind));
			Command<Commands::ForkSchoolAction>::Post(STR_ERROR_FORK_EDU_CAN_T_CHANGE, town_id, static_cast<uint8_t>(this->kind), static_cast<uint8_t>(ForkSchoolAction::SetFee), static_cast<uint16_t>(fee));
		}
	}

	void OnClick(Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		const TownID town_id = this->town->index;
		const uint8_t kind = static_cast<uint8_t>(this->kind);
		const ForkAsset *a = this->Current();

		switch (widget) {
			case WID_FED_TAB_PRIMARY: this->SetTab(ForkSchoolKind::Primary); break;
			case WID_FED_TAB_SECONDARY: this->SetTab(ForkSchoolKind::Secondary); break;
			case WID_FED_TAB_UNIVERSITY: this->SetTab(ForkSchoolKind::University); break;

			case WID_FED_INFO: {
				if (this->kind != ForkSchoolKind::University || a == nullptr) break;
				const int top = this->GetWidget<NWidgetBase>(WID_FED_INFO)->pos_y + WidgetDimensions::scaled.frametext.top;
				const int row = (pt.y - top) / GetCharacterHeight(FontSize::Normal) - static_cast<int>(FED_HEADER_LINES);
				if (row >= 0 && row < static_cast<int>(FORK_FACULTIES)) {
					this->faculty = row;
					this->SetDirty();
				}
				break;
			}

			case WID_FED_BUILD:
				Command<Commands::BuildForkSchool>::Post(STR_ERROR_FORK_EDU_CAN_T_BUILD, town_id, kind);
				break;

			case WID_FED_SELL:
				Command<Commands::SellForkSchool>::Post(STR_ERROR_FORK_EDU_CAN_T_SELL, town_id, kind);
				break;

			case WID_FED_RENAME:
				if (a != nullptr) ShowQueryString(GetForkSchoolName(a), STR_FORK_EDU_RENAME_CAPTION, 32, this, CS_ALPHANUMERAL, {QueryStringFlag::EnableDefault, QueryStringFlag::LengthIsInChars});
				break;

			case WID_FED_UPGRADE:
				Command<Commands::ForkSchoolAction>::Post(STR_ERROR_FORK_EDU_CAN_T_CHANGE, town_id, kind, static_cast<uint8_t>(ForkSchoolAction::Upgrade), static_cast<uint16_t>(0));
				break;

			case WID_FED_SHIFT:
				Command<Commands::ForkSchoolAction>::Post(STR_ERROR_FORK_EDU_CAN_T_CHANGE, town_id, kind, static_cast<uint8_t>(ForkSchoolAction::ToggleShift), static_cast<uint16_t>(0));
				break;

			case WID_FED_FACULTY: {
				if (a == nullptr) break;
				const bool open = (GetForkFacultyOpenMask(a) & (1U << this->faculty)) != 0;
				Command<Commands::ForkSchoolAction>::Post(STR_ERROR_FORK_EDU_CAN_T_CHANGE, town_id, kind, static_cast<uint8_t>(open ? ForkSchoolAction::CloseFaculty : ForkSchoolAction::OpenFaculty), static_cast<uint16_t>(this->faculty));
				break;
			}

			case WID_FED_FEE_DOWN_BIG: this->ChangeFee(-1, true); break;
			case WID_FED_FEE_DOWN: this->ChangeFee(-1, false); break;
			case WID_FED_FEE_UP: this->ChangeFee(1, false); break;
			case WID_FED_FEE_UP_BIG: this->ChangeFee(1, true); break;
		}
	}

	void OnQueryTextFinished(std::optional<std::string> str) override
	{
		if (!str.has_value()) return;
		Command<Commands::RenameForkSchool>::Post(STR_ERROR_FORK_EDU_CAN_T_RENAME, this->town->index, static_cast<uint8_t>(this->kind), *str);
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_fork_education_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::Brown),
		NWidget(WWT_CAPTION, Colours::Brown, WID_FED_CAPTION), SetToolTip(STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, Colours::Brown),
		NWidget(WWT_STICKYBOX, Colours::Brown),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FED_TAB_PRIMARY), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_EDU_TAB_PRIMARY, STR_FORK_EDU_TAB_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FED_TAB_SECONDARY), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_EDU_TAB_SECONDARY, STR_FORK_EDU_TAB_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FED_TAB_UNIVERSITY), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_EDU_TAB_UNIVERSITY, STR_FORK_EDU_TAB_TOOLTIP),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::Brown, WID_FED_INFO), SetResize(1, 1), SetFill(1, 1),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FED_BUILD), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_EDU_BUILD, STR_FORK_EDU_BUILD_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FED_SELL), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_EDU_SELL, STR_FORK_EDU_SELL_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FED_RENAME), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_EDU_RENAME, STR_FORK_EDU_RENAME_TOOLTIP),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FED_UPGRADE), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_EDU_UPGRADE, STR_FORK_EDU_UPGRADE_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FED_SHIFT), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_EDU_SHIFT, STR_FORK_EDU_SHIFT_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FED_FACULTY), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_EDU_FACULTY, STR_FORK_EDU_FACULTY_TOOLTIP),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FED_FEE_DOWN_BIG), SetMinimalSize(40, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_EDU_FEE_DOWN_BIG, STR_FORK_EDU_FEE_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FED_FEE_DOWN), SetMinimalSize(40, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_EDU_FEE_DOWN, STR_FORK_EDU_FEE_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FED_FEE_UP), SetMinimalSize(40, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_EDU_FEE_UP, STR_FORK_EDU_FEE_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FED_FEE_UP_BIG), SetMinimalSize(40, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_EDU_FEE_UP_BIG, STR_FORK_EDU_FEE_TOOLTIP),
		NWidget(WWT_RESIZEBOX, Colours::Brown),
	EndContainer(),
};

static WindowDesc _fork_education_desc(
	WindowPosition::Automatic, {}, 0, 0,
	WindowClass::ForkEducation, WindowClass::None,
	{},
	_nested_fork_education_widgets
);

/**
 * Show the private education window of a town.
 * @param town Index of the town.
 */
void ShowForkEducationWindow(uint town)
{
	AllocateWindowDescFront<ForkEducationWindow>(_fork_education_desc, town);
}
