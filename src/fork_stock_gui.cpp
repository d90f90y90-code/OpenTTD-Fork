/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_stock_gui.cpp Window of the stock exchange: listed companies and the firms of the towns. */

#include "stdafx.h"

#include "command_func.h"
#include "company_base.h"
#include "company_func.h"
#include "fork_economy.h"
#include "fork_firm.h"
#include "fork_stock.h"
#include "fork_stock_cmd.h"
#include "gfx_func.h"
#include "palette_func.h"
#include "strings_func.h"
#include "town.h"
#include "window_func.h"
#include "window_gui.h"
#include "zoom_func.h"

#include "table/strings.h"

#include "safeguards.h"

/** Widgets of the stock exchange window. */
enum ForkExchangeWidgets : WidgetID {
	WID_SX_CAPTION, ///< Caption of the window.
	WID_SX_TAB_LISTED, ///< Show the listed companies.
	WID_SX_TAB_LOCAL, ///< Show the firms of the towns.
	WID_SX_LIST, ///< List of companies.
	WID_SX_SCROLLBAR, ///< Scrollbar of the list.
	WID_SX_INFO, ///< Panel with the texts about the selected company.
	WID_SX_BTN_1, ///< First action button.
	WID_SX_BTN_2, ///< Second action button.
	WID_SX_BTN_3, ///< Third action button.
	WID_SX_BTN_4, ///< Fourth action button.
};

static constexpr uint SX_INFO_LINES = 3; ///< Number of text lines in the information panel.

/** @return The firms of the towns whose shares can be bought, in a fixed order. */
static std::vector<const ForkFirm *> GetIndependentFirms()
{
	std::vector<const ForkFirm *> firms;
	for (const ForkFirm *f : ForkFirm::Iterate()) {
		if (!Company::IsValidID(f->owner)) firms.push_back(f);
	}
	return firms;
}

/** Window with the stock exchange. */
struct ForkExchangeWindow : Window {
	Scrollbar *vscroll = nullptr; ///< Scrollbar of the list.
	uint tab = 0; ///< 0 for the listed companies, 1 for the firms of the towns.
	int selected = -1; ///< The row that was chosen, or -1.
	int line = 0; ///< Height of a line of text.
	int block_h = 0; ///< Height of a row.

	ForkExchangeWindow(WindowDesc &desc, WindowNumber number) : Window(desc)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_SX_SCROLLBAR);
		this->FinishInitNested(number);
		this->Refresh();
	}

	size_t RowCount() const
	{
		return this->tab == 0 ? FORK_LISTED_COUNT : GetIndependentFirms().size();
	}

	/** Update the buttons and the scrollbar to what is chosen. */
	void Refresh()
	{
		this->SetWidgetLoweredState(WID_SX_TAB_LISTED, this->tab == 0);
		this->SetWidgetLoweredState(WID_SX_TAB_LOCAL, this->tab == 1);
		this->vscroll->SetCount(this->RowCount());
		if (this->selected >= static_cast<int>(this->RowCount())) this->selected = -1;

		const bool have = this->selected >= 0 && Company::IsValidID(_local_company);
		this->SetWidgetDisabledState(WID_SX_BTN_1, !have);
		this->SetWidgetDisabledState(WID_SX_BTN_2, !have);
		this->SetWidgetDisabledState(WID_SX_BTN_3, !have);
		this->SetWidgetDisabledState(WID_SX_BTN_4, !have || this->tab == 1);
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_SX_BTN_1) return GetString(this->tab == 0 ? STR_FORK_STOCK_BUY_10 : STR_FORK_LOCAL_BUY);
		if (widget == WID_SX_BTN_2) return GetString(this->tab == 0 ? STR_FORK_STOCK_BUY_100 : STR_FORK_LOCAL_SELL);
		if (widget == WID_SX_BTN_3) return GetString(this->tab == 0 ? STR_FORK_STOCK_SELL_10 : STR_FORK_LOCAL_TAKEOVER);
		if (widget == WID_SX_BTN_4) return GetString(this->tab == 0 ? STR_FORK_STOCK_SELL_100 : STR_FORK_STOCK_NO_ACTION);
		return this->Window::GetWidgetString(widget, stringid);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		this->line = GetCharacterHeight(FontSize::Normal);
		this->block_h = 2 * this->line + ScaleGUITrad(6);

		if (widget == WID_SX_LIST) {
			size.width = std::max<uint>(size.width, ScaleGUITrad(420));
			fill.height = resize.height = this->block_h;
			size.height = 5 * this->block_h;
		} else if (widget == WID_SX_INFO) {
			size.height = SX_INFO_LINES * this->line + padding.height;
		}
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_SX_LIST, WidgetDimensions::scaled.framerect.Vertical());
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, bool gui_scope = true) override
	{
		if (!gui_scope) return;
		this->Refresh();
		this->SetDirty();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (!Company::IsValidID(_local_company)) return;

		if (widget == WID_SX_LIST) {
			this->DrawList(r);
		} else if (widget == WID_SX_INFO) {
			this->DrawInfo(r);
		}
	}

	void DrawList(const Rect &r) const
	{
		const Rect ir = r.Shrink(WidgetDimensions::scaled.framerect);
		const std::vector<const ForkFirm *> firms = this->tab == 1 ? GetIndependentFirms() : std::vector<const ForkFirm *>{};
		const size_t count = this->RowCount();
		const size_t last = std::min<size_t>(count, this->vscroll->GetPosition() + this->vscroll->GetCapacity());

		int y = ir.top;
		for (size_t i = this->vscroll->GetPosition(); i < last; i++) {
			if (static_cast<int>(i) == this->selected) GfxFillRect(Rect{ir.left, y, ir.right, y + this->block_h - 2}, GetColourGradient(Colours::Brown, Shade::Lightest));

			if (this->tab == 0) {
				const int change = GetForkStockChangePermille(i);
				const uint held = GetForkStockHeld(_local_company, i);
				DrawString(ir.left, ir.right, y, GetString(STR_FORK_STOCK_ROW1, STR_FORK_STOCK_NAME_0 + i, GetForkStockPrice(i), GetString(change >= 0 ? STR_FORK_STOCK_UP : STR_FORK_STOCK_DOWN, std::abs(change) / 10, std::abs(change) % 10)));
				DrawString(ir.left, ir.right, y + this->line, GetString(STR_FORK_STOCK_ROW2, held, GetForkStockPrice(i) * held, GetForkStockYieldTenths(i) / 10, GetForkStockYieldTenths(i) % 10));
			} else {
				const ForkFirm *f = firms[i];
				DrawString(ir.left, ir.right, y, GetString(STR_FORK_LOCAL_ROW1, f->name, f->GetTown(), STR_FORK_PRODUCT_DISHWASHER + f->product, f->value));
				DrawString(ir.left, ir.right, y + this->line, GetString(STR_FORK_LOCAL_ROW2, GetForkFirmStake(_local_company, f->index.base()), GetForkFirmShareValue(f)));
			}
			y += this->block_h;
		}
	}

	void DrawInfo(const Rect &r) const
	{
		const Rect ir = r.Shrink(WidgetDimensions::scaled.frametext);
		int y = ir.top;

		if (this->selected < 0) {
			DrawString(ir.left, ir.right, y, GetString(this->tab == 0 ? STR_FORK_EXCHANGE_HINT_LISTED : STR_FORK_EXCHANGE_HINT_LOCAL));
			return;
		}

		if (this->tab == 0) {
			const uint i = static_cast<uint>(this->selected);
			const uint held = GetForkStockHeld(_local_company, i);
			DrawString(ir.left, ir.right, y, GetString(STR_FORK_STOCK_INFO1, STR_FORK_STOCK_NAME_0 + i, GetForkStockPrice(i)));
			y += this->line;
			DrawString(ir.left, ir.right, y, GetString(STR_FORK_STOCK_INFO2, held, GetForkStockPrice(i) * held, GetForkStockCost(_local_company, i)));
			y += this->line;
			DrawString(ir.left, ir.right, y, GetString(STR_FORK_EXCHANGE_HINT_LISTED));
		} else {
			const std::vector<const ForkFirm *> firms = GetIndependentFirms();
			if (static_cast<size_t>(this->selected) >= firms.size()) return;
			const ForkFirm *f = firms[this->selected];
			DrawString(ir.left, ir.right, y, GetString(STR_FORK_LOCAL_INFO1, f->name, GetForkFirmShareValue(f) * 5, f->profit_last));
			y += this->line;
			DrawString(ir.left, ir.right, y, GetString(STR_FORK_LOCAL_INFO2, GetForkFirmStake(_local_company, f->index.base()), GetForkFirmStakeTotal(f->index.base()), GetForkFirmTakeoverPrice(f)));
			y += this->line;
			DrawString(ir.left, ir.right, y, GetString(STR_FORK_EXCHANGE_HINT_LOCAL));
		}
	}

	void OnClick(Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_SX_TAB_LISTED:
			case WID_SX_TAB_LOCAL:
				this->tab = widget == WID_SX_TAB_LISTED ? 0 : 1;
				this->selected = -1;
				this->vscroll->SetPosition(0);
				this->Refresh();
				this->SetDirty();
				return;

			case WID_SX_LIST: {
				const NWidgetBase *nwid = this->GetWidget<NWidgetBase>(WID_SX_LIST);
				const int rel_y = pt.y - static_cast<int>(nwid->pos_y) - WidgetDimensions::scaled.framerect.top;
				if (rel_y < 0 || this->block_h <= 0) return;
				const size_t row = this->vscroll->GetPosition() + rel_y / this->block_h;
				if (row < this->RowCount()) this->selected = static_cast<int>(row);
				this->Refresh();
				this->SetDirty();
				return;
			}
		}

		if (this->selected < 0 || widget < WID_SX_BTN_1 || widget > WID_SX_BTN_4) return;

		if (this->tab == 0) {
			const uint8_t listed = static_cast<uint8_t>(this->selected);
			switch (widget) {
				case WID_SX_BTN_1: Command<Commands::BuyForkStock>::Post(STR_ERROR_FORK_STOCK_CAN_T_BUY, listed, static_cast<uint8_t>(10)); break;
				case WID_SX_BTN_2: Command<Commands::BuyForkStock>::Post(STR_ERROR_FORK_STOCK_CAN_T_BUY, listed, static_cast<uint8_t>(100)); break;
				case WID_SX_BTN_3: Command<Commands::SellForkStock>::Post(STR_ERROR_FORK_STOCK_CAN_T_SELL, listed, static_cast<uint8_t>(10)); break;
				case WID_SX_BTN_4: Command<Commands::SellForkStock>::Post(STR_ERROR_FORK_STOCK_CAN_T_SELL, listed, static_cast<uint8_t>(100)); break;
			}
		} else {
			const std::vector<const ForkFirm *> firms = GetIndependentFirms();
			if (static_cast<size_t>(this->selected) >= firms.size()) return;
			const uint32_t id = firms[this->selected]->index.base();
			switch (widget) {
				case WID_SX_BTN_1: Command<Commands::BuyForkFirmShare>::Post(STR_ERROR_FORK_STOCK_CAN_T_BUY, id, static_cast<uint8_t>(5)); break;
				case WID_SX_BTN_2: Command<Commands::SellForkFirmShare>::Post(STR_ERROR_FORK_STOCK_CAN_T_SELL, id, static_cast<uint8_t>(5)); break;
				case WID_SX_BTN_3: Command<Commands::TakeOverForkFirm>::Post(STR_ERROR_FORK_STOCK_CAN_T_TAKEOVER, id); break;
			}
		}
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_fork_exchange_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::Brown),
		NWidget(WWT_CAPTION, Colours::Brown, WID_SX_CAPTION), SetStringTip(STR_FORK_EXCHANGE_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, Colours::Brown),
		NWidget(WWT_STICKYBOX, Colours::Brown),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_SX_TAB_LISTED), SetMinimalSize(100, 14), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_EXCHANGE_TAB_LISTED, STR_FORK_EXCHANGE_TAB_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_SX_TAB_LOCAL), SetMinimalSize(100, 14), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_EXCHANGE_TAB_LOCAL, STR_FORK_EXCHANGE_TAB_TOOLTIP),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, Colours::Brown, WID_SX_LIST), SetScrollbar(WID_SX_SCROLLBAR), SetResize(1, 1), SetFill(1, 1), SetMinimalSize(300, 60),
		EndContainer(),
		NWidget(NWID_VSCROLLBAR, Colours::Brown, WID_SX_SCROLLBAR),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::Brown, WID_SX_INFO), SetResize(1, 0), SetFill(1, 0),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_SX_BTN_1), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetToolTip(STR_FORK_EXCHANGE_BUTTON_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_SX_BTN_2), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetToolTip(STR_FORK_EXCHANGE_BUTTON_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_SX_BTN_3), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetToolTip(STR_FORK_EXCHANGE_BUTTON_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_SX_BTN_4), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetToolTip(STR_FORK_EXCHANGE_BUTTON_TOOLTIP),
		NWidget(WWT_RESIZEBOX, Colours::Brown),
	EndContainer(),
};

static WindowDesc _fork_exchange_desc(
	WindowPosition::Automatic, "fork_exchange", 0, 0,
	WindowClass::ForkExchange, WindowClass::None,
	{},
	_nested_fork_exchange_widgets
);

/** Show the stock exchange. */
void ShowForkExchangeWindow()
{
	AllocateWindowDescFront<ForkExchangeWindow>(_fork_exchange_desc, 0);
}
