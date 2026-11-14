/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_economy_gui.cpp Window showing the extended economy: market, assets and the business ledger. */

#include "stdafx.h"

#include "company_func.h"
#include "fork_economy.h"
#include "gfx_func.h"
#include "strings_func.h"
#include "window_func.h"
#include "window_gui.h"
#include "zoom_func.h"

#include "table/strings.h"

#include "safeguards.h"

/** Widgets of the business window. */
enum ForkBusinessWidgets : WidgetID {
	WID_FB_PANEL, ///< Panel with the texts.
};

/** Window with an overview of the extended economy for the local company. */
struct ForkBusinessWindow : Window {
	ForkBusinessWindow(WindowDesc &desc, WindowNumber number) : Window(desc)
	{
		this->InitNested(number);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget != WID_FB_PANEL) return;

		size.width = std::max<uint>(size.width, ScaleGUITrad(320));
		size.height = (2 + FORK_LEDGER_CATEGORIES) * GetCharacterHeight(FontSize::Normal) + padding.height;
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, bool gui_scope = true) override
	{
		if (gui_scope) this->SetDirty();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_FB_PANEL) return;

		const Rect ir = r.Shrink(WidgetDimensions::scaled.frametext);
		const int line = GetCharacterHeight(FontSize::Normal);
		int y = ir.top;

		DrawString(ir.left, ir.right, y, GetString(STR_FORK_BUSINESS_MARKET, STR_FORK_PHASE_NORMAL + _fork_economy.market_phase, _fork_economy.market_index / 10));
		y += line;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_BUSINESS_ASSETS, GetForkAssetCount(_local_company)));
		y += line;

		for (size_t i = 0; i < FORK_LEDGER_CATEGORIES; i++) {
			Money amount = GetForkLedger(_local_company, static_cast<ForkLedgerCategory>(i), true);
			DrawString(ir.left, ir.right, y, GetString(STR_FORK_LEDGER_ROW, STR_FORK_CATEGORY_TAX + i, amount));
			y += line;
		}
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_fork_business_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::Grey),
		NWidget(WWT_CAPTION, Colours::Grey), SetStringTip(STR_FORK_BUSINESS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, Colours::Grey),
		NWidget(WWT_STICKYBOX, Colours::Grey),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::Grey, WID_FB_PANEL), SetResize(1, 1), SetFill(1, 1),
	EndContainer(),
};

static WindowDesc _fork_business_desc(
	WindowPosition::Center, {}, 0, 0,
	WindowClass::ForkBusiness, WindowClass::None,
	{},
	_nested_fork_business_widgets
);

/** Show the business window. */
void ShowForkBusinessWindow()
{
	AllocateWindowDescFront<ForkBusinessWindow>(_fork_business_desc, 0);
}
