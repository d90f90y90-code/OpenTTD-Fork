/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_shop_gui.cpp Window for opening and selling shops in a town and setting the prices. */

#include "stdafx.h"

#include "command_func.h"
#include "company_base.h"
#include "company_func.h"
#include "fork_economy.h"
#include "fork_shop_cmd.h"
#include "gfx_func.h"
#include "strings_func.h"
#include "town.h"
#include "window_func.h"
#include "window_gui.h"
#include "zoom_func.h"

#include "table/strings.h"

#include "safeguards.h"

/** Widgets of the shops window. */
enum ForkShopsWidgets : WidgetID {
	WID_FSH_CAPTION, ///< Caption of the window.
	WID_FSH_INFO, ///< Panel with the texts.
	WID_FSH_BUY_1, ///< Buy one shop.
	WID_FSH_BUY_10, ///< Buy ten shops.
	WID_FSH_SELL_1, ///< Sell one shop.
	WID_FSH_SELL_10, ///< Sell ten shops.
	WID_FSH_PRICE_DOWN, ///< Lower the prices.
	WID_FSH_PRICE_UP, ///< Raise the prices.
};

static constexpr uint FSH_INFO_LINES = 6; ///< Number of text lines in the information panel.

/** Window to run shops in one town. The window number is the town. */
struct ForkShopsWindow : Window {
	const Town *town; ///< Town of this window.

	ForkShopsWindow(WindowDesc &desc, WindowNumber number) : Window(desc), town(Town::Get(static_cast<TownID>(number)))
	{
		this->InitNested(number);
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_FSH_CAPTION) return GetString(STR_FORK_SHOP_CAPTION, this->town->index);
		return this->Window::GetWidgetString(widget, stringid);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget != WID_FSH_INFO) return;

		size.width = std::max<uint>(size.width, ScaleGUITrad(360));
		size.height = FSH_INFO_LINES * GetCharacterHeight(FontSize::Normal) + padding.height;
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, bool gui_scope = true) override
	{
		if (gui_scope) this->SetDirty();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_FSH_INFO || !Company::IsValidID(_local_company)) return;

		const Town *t = this->town;
		const uint shops = GetForkCompanyShops(_local_company, t->index);
		const uint price = GetForkShopPrice(_local_company, t->index);
		const Money value = GetForkShopValue(t);
		const ForkShopResult res = GetForkShopResult(t, _local_company, shops, price);
		const Money tax = value * shops * GetForkTownTaxPermille(t) / 8000;

		const Rect ir = r.Shrink(WidgetDimensions::scaled.frametext);
		const int line = GetCharacterHeight(FontSize::Normal);
		int y = ir.top;

		DrawString(ir.left, ir.right, y, GetString(STR_FORK_SHOP_OWNED, shops, GetForkShopLimit(t), value * shops));
		y += line;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_SHOP_PRICE, GetForkShopBuyPrice(t), GetForkShopSellValue(t)));
		y += line;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_SHOP_SPENDING, GetForkTownRetailSpending(t)));
		y += line;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_SHOP_YOUR_PRICES, price, res.share_permille / 10, res.share_permille % 10));
		y += line;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_SHOP_SALES, res.sales, res.sales - res.profit));
		y += line;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_SHOP_PROFIT, res.profit, tax));
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		const TownID town_id = this->town->index;
		switch (widget) {
			case WID_FSH_BUY_1:
				Command<Commands::BuyForkShops>::Post(STR_ERROR_FORK_CAN_T_BUY_SHOPS, town_id, static_cast<uint8_t>(1));
				break;

			case WID_FSH_BUY_10:
				Command<Commands::BuyForkShops>::Post(STR_ERROR_FORK_CAN_T_BUY_SHOPS, town_id, static_cast<uint8_t>(10));
				break;

			case WID_FSH_SELL_1:
				Command<Commands::SellForkShops>::Post(STR_ERROR_FORK_CAN_T_SELL_SHOPS, town_id, static_cast<uint8_t>(1));
				break;

			case WID_FSH_SELL_10:
				Command<Commands::SellForkShops>::Post(STR_ERROR_FORK_CAN_T_SELL_SHOPS, town_id, static_cast<uint8_t>(10));
				break;

			case WID_FSH_PRICE_DOWN: {
				uint percent = GetForkShopPrice(_local_company, town_id);
				percent = percent > 80 ? percent - 10 : 70;
				Command<Commands::SetForkShopPrice>::Post(STR_ERROR_FORK_CAN_T_SET_SHOP_PRICE, town_id, static_cast<uint16_t>(percent));
				break;
			}

			case WID_FSH_PRICE_UP: {
				uint percent = GetForkShopPrice(_local_company, town_id);
				percent = percent < 140 ? percent + 10 : 150;
				Command<Commands::SetForkShopPrice>::Post(STR_ERROR_FORK_CAN_T_SET_SHOP_PRICE, town_id, static_cast<uint16_t>(percent));
				break;
			}
		}
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_fork_shops_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::Brown),
		NWidget(WWT_CAPTION, Colours::Brown, WID_FSH_CAPTION), SetToolTip(STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, Colours::Brown),
		NWidget(WWT_STICKYBOX, Colours::Brown),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::Brown, WID_FSH_INFO), SetResize(1, 1), SetFill(1, 1),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FSH_BUY_1), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_SHOP_BUY_1, STR_FORK_SHOP_BUY_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FSH_BUY_10), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_SHOP_BUY_10, STR_FORK_SHOP_BUY_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FSH_SELL_1), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_SHOP_SELL_1, STR_FORK_SHOP_SELL_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FSH_SELL_10), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_SHOP_SELL_10, STR_FORK_SHOP_SELL_TOOLTIP),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FSH_PRICE_DOWN), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_SHOP_PRICE_DOWN, STR_FORK_SHOP_PRICE_DOWN_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FSH_PRICE_UP), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_SHOP_PRICE_UP, STR_FORK_SHOP_PRICE_UP_TOOLTIP),
		NWidget(WWT_RESIZEBOX, Colours::Brown),
	EndContainer(),
};

static WindowDesc _fork_shops_desc(
	WindowPosition::Automatic, {}, 0, 0,
	WindowClass::ForkShops, WindowClass::None,
	{},
	_nested_fork_shops_widgets
);

/**
 * Show the shops window of a town.
 * @param town Index of the town.
 */
void ShowForkShopsWindow(uint town)
{
	AllocateWindowDescFront<ForkShopsWindow>(_fork_shops_desc, town);
}
