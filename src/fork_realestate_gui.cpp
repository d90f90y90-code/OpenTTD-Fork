/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_realestate_gui.cpp Window for buying and selling houses in a town and setting the rent. */

#include "stdafx.h"

#include "command_func.h"
#include "company_base.h"
#include "company_func.h"
#include "fork_economy.h"
#include "fork_realestate_cmd.h"
#include "gfx_func.h"
#include "strings_func.h"
#include "town.h"
#include "window_func.h"
#include "window_gui.h"
#include "zoom_func.h"

#include "table/strings.h"

#include "safeguards.h"

/** Widgets of the real estate window. */
enum ForkRealEstateWidgets : WidgetID {
	WID_FRE_CAPTION, ///< Caption of the window.
	WID_FRE_INFO, ///< Panel with the texts.
	WID_FRE_BUY_1, ///< Buy one house.
	WID_FRE_BUY_10, ///< Buy ten houses.
	WID_FRE_SELL_1, ///< Sell one house.
	WID_FRE_SELL_10, ///< Sell ten houses.
	WID_FRE_RENT_DOWN, ///< Lower the rent.
	WID_FRE_RENT_UP, ///< Raise the rent.
};

static constexpr uint FRE_INFO_LINES = 6; ///< Number of text lines in the information panel.

/** Window to trade houses in one town. The window number is the town. */
struct ForkRealEstateWindow : Window {
	const Town *town; ///< Town of this window.

	ForkRealEstateWindow(WindowDesc &desc, WindowNumber number) : Window(desc), town(Town::Get(static_cast<TownID>(number)))
	{
		this->InitNested(number);
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_FRE_CAPTION) return GetString(STR_FORK_RE_CAPTION, this->town->index);
		return this->Window::GetWidgetString(widget, stringid);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget != WID_FRE_INFO) return;

		size.width = std::max<uint>(size.width, ScaleGUITrad(360));
		size.height = FRE_INFO_LINES * GetCharacterHeight(FontSize::Normal) + padding.height;
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, bool gui_scope = true) override
	{
		if (gui_scope) this->SetDirty();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_FRE_INFO || !Company::IsValidID(_local_company)) return;

		const Town *t = this->town;
		const uint houses = GetForkCompanyHouses(_local_company, t->index);
		const uint rent_percent = GetForkRentPercent(_local_company, t->index);
		const int swing = GetForkPropertySwing();
		const Money value = GetForkHouseValue(t);
		const Money income = GetForkHouseRent(t, rent_percent) * houses * GetForkStaffEfficiency(_local_company, t->index) / 100;
		const Money tax = value * houses * GetForkTownTaxPermille(t) / 8000;

		const Rect ir = r.Shrink(WidgetDimensions::scaled.frametext);
		const int line = GetCharacterHeight(FontSize::Normal);
		int y = ir.top;

		DrawString(ir.left, ir.right, y, GetString(STR_FORK_RE_OWNED, houses, GetForkHouseLimit(t), value * houses));
		y += line;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_RE_PRICE, GetForkHouseBuyPrice(t), GetForkHouseSellValue(t)));
		y += line;
		const int swing_abs = swing < 0 ? -swing : swing;
		DrawString(ir.left, ir.right, y, GetString(swing < 0 ? STR_FORK_RE_MARKET_DOWN : STR_FORK_RE_MARKET_UP, swing_abs / 10, swing_abs % 10));
		y += line;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_RE_MARKET_RENT, GetForkHouseRent(t, 100)));
		y += line;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_RE_YOUR_RENT, rent_percent, GetForkOccupancy(rent_percent)));
		y += line;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_RE_INCOME, income, tax));
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		const TownID town_id = this->town->index;
		switch (widget) {
			case WID_FRE_BUY_1:
				Command<Commands::BuyForkHouses>::Post(STR_ERROR_FORK_CAN_T_BUY_HOUSES, town_id, static_cast<uint8_t>(1));
				break;

			case WID_FRE_BUY_10:
				Command<Commands::BuyForkHouses>::Post(STR_ERROR_FORK_CAN_T_BUY_HOUSES, town_id, static_cast<uint8_t>(10));
				break;

			case WID_FRE_SELL_1:
				Command<Commands::SellForkHouses>::Post(STR_ERROR_FORK_CAN_T_SELL_HOUSES, town_id, static_cast<uint8_t>(1));
				break;

			case WID_FRE_SELL_10:
				Command<Commands::SellForkHouses>::Post(STR_ERROR_FORK_CAN_T_SELL_HOUSES, town_id, static_cast<uint8_t>(10));
				break;

			case WID_FRE_RENT_DOWN: {
				uint percent = GetForkRentPercent(_local_company, town_id);
				percent = percent > 60 ? percent - 10 : 50;
				Command<Commands::SetForkRent>::Post(STR_ERROR_FORK_CAN_T_SET_RENT, town_id, static_cast<uint16_t>(percent));
				break;
			}

			case WID_FRE_RENT_UP: {
				uint percent = GetForkRentPercent(_local_company, town_id);
				percent = percent < 240 ? percent + 10 : 250;
				Command<Commands::SetForkRent>::Post(STR_ERROR_FORK_CAN_T_SET_RENT, town_id, static_cast<uint16_t>(percent));
				break;
			}
		}
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_fork_real_estate_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::Brown),
		NWidget(WWT_CAPTION, Colours::Brown, WID_FRE_CAPTION), SetToolTip(STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, Colours::Brown),
		NWidget(WWT_STICKYBOX, Colours::Brown),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::Brown, WID_FRE_INFO), SetResize(1, 1), SetFill(1, 1),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FRE_BUY_1), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_RE_BUY_1, STR_FORK_RE_BUY_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FRE_BUY_10), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_RE_BUY_10, STR_FORK_RE_BUY_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FRE_SELL_1), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_RE_SELL_1, STR_FORK_RE_SELL_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FRE_SELL_10), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_RE_SELL_10, STR_FORK_RE_SELL_TOOLTIP),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FRE_RENT_DOWN), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_RE_RENT_DOWN, STR_FORK_RE_RENT_DOWN_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FRE_RENT_UP), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_RE_RENT_UP, STR_FORK_RE_RENT_UP_TOOLTIP),
		NWidget(WWT_RESIZEBOX, Colours::Brown),
	EndContainer(),
};

static WindowDesc _fork_real_estate_desc(
	WindowPosition::Automatic, {}, 0, 0,
	WindowClass::ForkRealEstate, WindowClass::None,
	{},
	_nested_fork_real_estate_widgets
);

/**
 * Show the real estate window of a town.
 * @param town Index of the town.
 */
void ShowForkRealEstateWindow(uint town)
{
	AllocateWindowDescFront<ForkRealEstateWindow>(_fork_real_estate_desc, town);
}
