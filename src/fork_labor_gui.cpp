/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_labor_gui.cpp Window for hiring and firing staff in a town. */

#include "stdafx.h"

#include "command_func.h"
#include "company_base.h"
#include "company_func.h"
#include "fork_economy.h"
#include "fork_firm.h"
#include "fork_labor_cmd.h"
#include "gfx_func.h"
#include "strings_func.h"
#include "town.h"
#include "window_func.h"
#include "window_gui.h"
#include "zoom_func.h"

#include "table/strings.h"

#include "safeguards.h"

/** Widgets of the staff window. */
enum ForkStaffWidgets : WidgetID {
	WID_FST_CAPTION, ///< Caption of the window.
	WID_FST_INFO, ///< Panel with the texts.
	WID_FST_HIRE_1, ///< Hire one person.
	WID_FST_HIRE_5, ///< Hire five people.
	WID_FST_FIRE_1, ///< Fire one person.
	WID_FST_FIRE_5, ///< Fire five people.
};

static constexpr uint FST_INFO_LINES = 6; ///< Number of text lines in the information panel.

/** Window to manage the staff in one town. The window number is the town. */
struct ForkStaffWindow : Window {
	const Town *town; ///< Town of this window.

	ForkStaffWindow(WindowDesc &desc, WindowNumber number) : Window(desc), town(Town::Get(static_cast<TownID>(number)))
	{
		this->InitNested(number);
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_FST_CAPTION) return GetString(STR_FORK_STAFF_CAPTION, this->town->index);
		return this->Window::GetWidgetString(widget, stringid);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget != WID_FST_INFO) return;

		size.width = std::max<uint>(size.width, ScaleGUITrad(360));
		size.height = FST_INFO_LINES * GetCharacterHeight(FontSize::Normal) + padding.height;
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, bool gui_scope = true) override
	{
		if (gui_scope) this->SetDirty();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_FST_INFO || !Company::IsValidID(_local_company)) return;

		const Town *t = this->town;
		const uint staff = GetForkCompanyStaff(_local_company, t->index);
		const uint houses = GetForkCompanyHouses(_local_company, t->index);
		const uint shops = GetForkCompanyShops(_local_company, t->index);
		const uint required = GetForkRequiredStaff(houses, shops, GetForkCompanyFirms(_local_company, t->index));
		const Money wage = GetForkWage(t);

		const Rect ir = r.Shrink(WidgetDimensions::scaled.frametext);
		const int line = GetCharacterHeight(FontSize::Normal);
		int y = ir.top;

		DrawString(ir.left, ir.right, y, GetString(STR_FORK_STAFF_EMPLOYED, staff, GetForkStaffLimit(t), required));
		y += line;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_STAFF_WAGE, wage));
		y += line;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_STAFF_COSTS, wage * 2, wage));
		y += line;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_STAFF_ASSETS, houses, shops));
		y += line;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_STAFF_EFFICIENCY, GetForkStaffEfficiency(_local_company, t->index)));
		y += line;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_STAFF_SALARIES, wage * staff));
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		const TownID town_id = this->town->index;
		switch (widget) {
			case WID_FST_HIRE_1:
				Command<Commands::HireForkStaff>::Post(STR_ERROR_FORK_CAN_T_HIRE, town_id, static_cast<uint8_t>(1));
				break;

			case WID_FST_HIRE_5:
				Command<Commands::HireForkStaff>::Post(STR_ERROR_FORK_CAN_T_HIRE, town_id, static_cast<uint8_t>(5));
				break;

			case WID_FST_FIRE_1:
				Command<Commands::FireForkStaff>::Post(STR_ERROR_FORK_CAN_T_FIRE, town_id, static_cast<uint8_t>(1));
				break;

			case WID_FST_FIRE_5:
				Command<Commands::FireForkStaff>::Post(STR_ERROR_FORK_CAN_T_FIRE, town_id, static_cast<uint8_t>(5));
				break;
		}
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_fork_staff_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::Brown),
		NWidget(WWT_CAPTION, Colours::Brown, WID_FST_CAPTION), SetToolTip(STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, Colours::Brown),
		NWidget(WWT_STICKYBOX, Colours::Brown),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::Brown, WID_FST_INFO), SetResize(1, 1), SetFill(1, 1),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FST_HIRE_1), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_STAFF_HIRE_1, STR_FORK_STAFF_HIRE_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FST_HIRE_5), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_STAFF_HIRE_5, STR_FORK_STAFF_HIRE_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FST_FIRE_1), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_STAFF_FIRE_1, STR_FORK_STAFF_FIRE_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FST_FIRE_5), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_STAFF_FIRE_5, STR_FORK_STAFF_FIRE_TOOLTIP),
		NWidget(WWT_RESIZEBOX, Colours::Brown),
	EndContainer(),
};

static WindowDesc _fork_staff_desc(
	WindowPosition::Automatic, "fork_staff", 0, 0,
	WindowClass::ForkStaff, WindowClass::None,
	{},
	_nested_fork_staff_widgets
);

/**
 * Show the staff window of a town.
 * @param town Index of the town.
 */
void ShowForkStaffWindow(uint town)
{
	AllocateWindowDescFront<ForkStaffWindow>(_fork_staff_desc, town);
}
