/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_firm_gui.cpp Windows of the firms: the firms of a town, founding a firm and running one. */

#include "stdafx.h"

#include "command_func.h"
#include "company_base.h"
#include "company_func.h"
#include "fork_economy.h"
#include "fork_firm.h"
#include "fork_firm_cmd.h"
#include "fork_logos.h"
#include "fork_stock.h"
#include "gfx_func.h"
#include "strings_func.h"
#include "textbuf_gui.h"
#include "town.h"
#include "window_func.h"
#include "window_gui.h"
#include "zoom_func.h"

#include "table/strings.h"

#include "safeguards.h"

/** @return The firms in a town, in a fixed order. */
static std::vector<const ForkFirm *> GetFirmsOfTown(TownID town)
{
	std::vector<const ForkFirm *> firms;
	for (const ForkFirm *f : ForkFirm::Iterate()) {
		if (f->GetTown() == town) firms.push_back(f);
	}
	return firms;
}

/** Widgets of the window with the firms of a town. */
enum ForkFirmsWidgets : WidgetID {
	WID_FF_CAPTION, ///< Caption of the window.
	WID_FF_LIST, ///< List of the firms.
	WID_FF_CREATE, ///< Found a firm.
	WID_FF_INVEST, ///< Open the stock exchange.
};

/** Window with the firms in one town. The window number is the town. */
struct ForkFirmsWindow : Window {
	const Town *town; ///< Town of this window.

	ForkFirmsWindow(WindowDesc &desc, WindowNumber number) : Window(desc), town(Town::Get(static_cast<TownID>(number)))
	{
		this->InitNested(number);
		this->UpdateButtons();
	}

	void UpdateButtons()
	{
		const bool can_found = Company::IsValidID(_local_company) && this->town->cache.population >= FORK_FIRM_MIN_POPULATION && GetForkFirmsInTown(this->town->index) < FORK_FIRM_MAX_PER_TOWN;
		this->SetWidgetDisabledState(WID_FF_CREATE, !can_found);
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_FF_CAPTION) return GetString(STR_FORK_FIRMS_CAPTION, this->town->index);
		return this->Window::GetWidgetString(widget, stringid);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget != WID_FF_LIST) return;

		size.width = std::max<uint>(size.width, ScaleGUITrad(420));
		size.height = (3 + 2 * FORK_FIRM_MAX_PER_TOWN) * GetCharacterHeight(FontSize::Normal) + padding.height;
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, bool gui_scope = true) override
	{
		if (!gui_scope) return;
		this->UpdateButtons();
		this->SetDirty();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_FF_LIST) return;

		const Rect ir = r.Shrink(WidgetDimensions::scaled.frametext);
		const int line = GetCharacterHeight(FontSize::Normal);
		int y = ir.top;

		DrawString(ir.left, ir.right, y, GetString(this->town->cache.population >= FORK_FIRM_MIN_POPULATION ? STR_FORK_FIRMS_HINT : STR_FORK_FIRMS_TOO_SMALL, FORK_FIRM_MIN_POPULATION));
		y += 2 * line;

		const std::vector<const ForkFirm *> firms = GetFirmsOfTown(this->town->index);
		if (firms.empty()) DrawString(ir.left, ir.right, y, GetString(STR_FORK_FIRMS_NONE));
		for (const ForkFirm *f : firms) {
			const bool own = f->owner == _local_company;
			DrawString(ir.left, ir.right, y, GetString(STR_FORK_FIRMS_ROW1, f->name, STR_FORK_PRODUCT_DISHWASHER + f->product, own ? STR_FORK_FIRMS_OWN : STR_FORK_FIRMS_INDEPENDENT));
			DrawString(ir.left, ir.right, y + line, GetString(STR_FORK_FIRMS_ROW2, GetForkFirmSellingPrice(f), f->quality, f->value));
			y += 2 * line;
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		if (widget == WID_FF_CREATE) {
			ShowForkFirmCreateWindow(this->town->index.base());
			return;
		}

		if (widget == WID_FF_INVEST) {
			ShowForkExchangeWindow();
			return;
		}

		if (widget != WID_FF_LIST) return;

		const NWidgetBase *nwid = this->GetWidget<NWidgetBase>(WID_FF_LIST);
		const int line = GetCharacterHeight(FontSize::Normal);
		const int y = pt.y - static_cast<int>(nwid->pos_y) - WidgetDimensions::scaled.frametext.top - 2 * line;
		if (y < 0) return;

		const std::vector<const ForkFirm *> firms = GetFirmsOfTown(this->town->index);
		const size_t row = static_cast<size_t>(y / (2 * line));
		if (row < firms.size() && firms[row]->owner == _local_company) ShowForkFirmWindow(firms[row]->index.base());
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_fork_firms_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::Brown),
		NWidget(WWT_CAPTION, Colours::Brown, WID_FF_CAPTION), SetToolTip(STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, Colours::Brown),
		NWidget(WWT_STICKYBOX, Colours::Brown),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::Brown, WID_FF_LIST), SetResize(1, 1), SetFill(1, 1),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FF_CREATE), SetMinimalSize(120, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_FIRMS_CREATE, STR_FORK_FIRMS_CREATE_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FF_INVEST), SetMinimalSize(120, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_INVEST_BUTTON, STR_FORK_INVEST_TOOLTIP),
		NWidget(WWT_RESIZEBOX, Colours::Brown),
	EndContainer(),
};

static WindowDesc _fork_firms_desc(
	WindowPosition::Automatic, "fork_firms", 0, 0,
	WindowClass::ForkFirms, WindowClass::None,
	{},
	_nested_fork_firms_widgets
);

/**
 * Show the firms of a town.
 * @param town Index of the town.
 */
void ShowForkFirmsWindow(uint town)
{
	AllocateWindowDescFront<ForkFirmsWindow>(_fork_firms_desc, town);
}

/** Widgets of the window for founding a firm. */
enum ForkFirmCreateWidgets : WidgetID {
	WID_FFC_CAPTION, ///< Caption of the window.
	WID_FFC_INFO, ///< Panel with the texts.
	WID_FFC_NAME, ///< Button that asks for the name.
	WID_FFC_PRODUCT_0, ///< First of the buttons to choose the product; there is one for every product.
	WID_FFC_PRODUCT_LAST = WID_FFC_PRODUCT_0 + FORK_PRODUCT_COUNT - 1, ///< Last of the product buttons.
	WID_FFC_FOUND, ///< Found the firm.
};

/** Window for choosing a name and a product before founding a firm. The window number is the town. */
struct ForkFirmCreateWindow : Window {
	const Town *town; ///< Town where the firm will be.
	std::string name; ///< Name that was chosen.
	uint product = 0; ///< Product that was chosen.

	ForkFirmCreateWindow(WindowDesc &desc, WindowNumber number) : Window(desc), town(Town::Get(static_cast<TownID>(number)))
	{
		this->name = GetString(STR_FORK_FIRM_DEFAULT_NAME, this->town->index);
		this->InitNested(number);
		this->UpdateButtons();
	}

	void UpdateButtons()
	{
		for (uint i = 0; i < FORK_PRODUCT_COUNT; i++) this->SetWidgetLoweredState(WID_FFC_PRODUCT_0 + i, i == this->product);
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_FFC_CAPTION) return GetString(STR_FORK_FIRM_CREATE_CAPTION, this->town->index);
		if (widget == WID_FFC_NAME) return GetString(STR_FORK_FIRM_NAME_BUTTON, this->name);
		return this->Window::GetWidgetString(widget, stringid);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget != WID_FFC_INFO) return;

		size.width = std::max<uint>(size.width, ScaleGUITrad(400));
		size.height = 4 * GetCharacterHeight(FontSize::Normal) + padding.height;
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_FFC_INFO) return;

		const Rect ir = r.Shrink(WidgetDimensions::scaled.frametext);
		const int line = GetCharacterHeight(FontSize::Normal);
		int y = ir.top;

		DrawString(ir.left, ir.right, y, GetString(STR_FORK_FIRM_CREATE_SPECIALIZATION));
		y += line;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_FIRM_CREATE_COST, GetForkFirmFoundingCost()));
		y += line;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_FIRM_CREATE_PRODUCT, STR_FORK_PRODUCT_DISHWASHER + this->product));
		y += line;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_FIRM_CREATE_HINT));
	}

	void OnQueryTextFinished(std::optional<std::string> str) override
	{
		if (!str.has_value() || str->empty()) return;
		this->name = *str;
		this->SetDirty();
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		if (widget == WID_FFC_NAME) {
			ShowQueryString(this->name, STR_FORK_FIRM_NAME_CAPTION, MAX_LENGTH_COMPANY_NAME_CHARS, this, CS_ALPHANUMERAL, {});
			return;
		}

		if (widget >= WID_FFC_PRODUCT_0 && widget <= WID_FFC_PRODUCT_LAST) {
			this->product = widget - WID_FFC_PRODUCT_0;
			this->UpdateButtons();
			this->SetDirty();
			return;
		}

		if (widget == WID_FFC_FOUND) {
			Command<Commands::CreateForkFirm>::Post(STR_ERROR_FORK_FIRM_CAN_T_FOUND, this->town->index, static_cast<uint8_t>(this->product), this->name);
			this->Close();
		}
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_fork_firm_create_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::Brown),
		NWidget(WWT_CAPTION, Colours::Brown, WID_FFC_CAPTION), SetToolTip(STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, Colours::Brown),
		NWidget(WWT_STICKYBOX, Colours::Brown),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::Brown, WID_FFC_INFO), SetResize(1, 1), SetFill(1, 1),
	EndContainer(),
	NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FFC_NAME), SetMinimalSize(100, 12), SetFill(1, 0), SetResize(1, 0), SetToolTip(STR_FORK_FIRM_NAME_TOOLTIP),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FFC_PRODUCT_0 + 0), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_PRODUCT_DISHWASHER, STR_FORK_FIRM_PRODUCT_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FFC_PRODUCT_0 + 1), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_PRODUCT_WASHING_MACHINE, STR_FORK_FIRM_PRODUCT_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FFC_PRODUCT_0 + 2), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_PRODUCT_LAWN_MOWER, STR_FORK_FIRM_PRODUCT_TOOLTIP),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FFC_PRODUCT_0 + 3), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_PRODUCT_LAMPS, STR_FORK_FIRM_PRODUCT_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FFC_PRODUCT_0 + 4), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_PRODUCT_TELEVISION, STR_FORK_FIRM_PRODUCT_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FFC_PRODUCT_0 + 5), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_PRODUCT_RADIO, STR_FORK_FIRM_PRODUCT_TOOLTIP),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FFC_FOUND), SetMinimalSize(120, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_FIRM_FOUND, STR_FORK_FIRM_FOUND_TOOLTIP),
		NWidget(WWT_RESIZEBOX, Colours::Brown),
	EndContainer(),
};

static WindowDesc _fork_firm_create_desc(
	WindowPosition::Automatic, "fork_firm_create", 0, 0,
	WindowClass::ForkFirmCreate, WindowClass::None,
	{},
	_nested_fork_firm_create_widgets
);

/**
 * Show the window for founding a firm.
 * @param town Index of the town.
 */
void ShowForkFirmCreateWindow(uint town)
{
	AllocateWindowDescFront<ForkFirmCreateWindow>(_fork_firm_create_desc, town);
}

/** Widgets of the window for running a firm. */
enum ForkFirmWidgets : WidgetID {
	WID_FM_CAPTION, ///< Caption of the window.
	WID_FM_HQ, ///< Picture of the headquarters.
	WID_FM_LAB, ///< Picture of the laboratory.
	WID_FM_INFO, ///< Panel with the texts.
	WID_FM_RATE_DOWN, ///< Make less.
	WID_FM_RATE_UP, ///< Make more.
	WID_FM_QUALITY_DOWN, ///< Lower the quality.
	WID_FM_QUALITY_UP, ///< Raise the quality.
	WID_FM_PRICE_DOWN, ///< Lower the price.
	WID_FM_PRICE_UP, ///< Raise the price.
	WID_FM_DISCOUNT_DOWN, ///< Lower the discount.
	WID_FM_DISCOUNT_UP, ///< Raise the discount.
	WID_FM_DUMP, ///< Throw away the stock.
};

static constexpr uint FM_INFO_LINES = 10; ///< Number of text lines in the information panel.

/** Window to run a firm of the company. The window number is the firm. */
struct ForkFirmWindow : Window {
	ForkFirmID firm; ///< The firm.

	ForkFirmWindow(WindowDesc &desc, WindowNumber number) : Window(desc), firm(static_cast<ForkFirmID>(number))
	{
		this->InitNested(number);
	}

	const ForkFirm *GetFirm() const
	{
		return ForkFirm::GetIfValid(this->firm);
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_FM_CAPTION) {
			const ForkFirm *f = this->GetFirm();
			return f != nullptr ? f->name : std::string{};
		}
		return this->Window::GetWidgetString(widget, stringid);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget != WID_FM_INFO) return;

		size.width = std::max<uint>(size.width, ScaleGUITrad(420));
		size.height = FM_INFO_LINES * GetCharacterHeight(FontSize::Normal) + padding.height;
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, bool gui_scope = true) override
	{
		if (!gui_scope) return;
		if (this->GetFirm() == nullptr) {
			this->Close();
			return;
		}
		this->SetDirty();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		const ForkFirm *f = this->GetFirm();
		if (f == nullptr) return;

		/* The glass headquarters and the laboratory are built over the first months of the firm. */
		if (widget == WID_FM_HQ || widget == WID_FM_LAB) {
			const uint stage = f->age_months < 2 ? 0 : (f->age_months < 6 ? 1 : 2);
			const bool lab = widget == WID_FM_LAB;
			DrawForkLogo(GetForkBuildingLogo(lab, lab ? f->product : f->index.base(), stage), r.Shrink(WidgetDimensions::scaled.bevel));
			return;
		}
		if (widget != WID_FM_INFO) return;

		const Rect ir = r.Shrink(WidgetDimensions::scaled.frametext);
		const int line = GetCharacterHeight(FontSize::Normal);
		int y = ir.top;

		DrawString(ir.left, ir.right, y, GetString(STR_FORK_FIRM_LINE_PRODUCT, STR_FORK_PRODUCT_DISHWASHER + f->product, f->value));
		y += line;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_FIRM_LINE_QUALITY, STR_FORK_QUALITY_1 + (f->quality - 1), f->quality, f->rate, GetForkFirmCapacity(f)));
		y += line;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_FIRM_LINE_PRICE, GetForkProductPrice(f->product) * f->price_percent / 100, f->price_percent, f->discount, GetForkFirmSellingPrice(f)));
		y += line;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_FIRM_LINE_MARKET, _fork_economy.firm_ref_price[f->product]));
		y += line;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_FIRM_LINE_STOCK, f->stock, f->made_last, f->sold_last));
		y += line;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_FIRM_LINE_MONEY, f->revenue_last, f->profit_last));
		y += line;

		/* What the competitors with the same product ask and make. */
		uint shown = 0;
		for (const ForkFirm *c : ForkFirm::Iterate()) {
			if (c == f || c->product != f->product) continue;
			if (shown == 0) {
				DrawString(ir.left, ir.right, y, GetString(STR_FORK_FIRM_COMPETITORS));
				y += line;
			}
			if (shown >= 3) break;
			DrawString(ir.left, ir.right, y, GetString(STR_FORK_FIRM_COMPETITOR_LINE, c->name, GetForkFirmSellingPrice(c), c->quality));
			y += line;
			shown++;
		}
		if (shown == 0) DrawString(ir.left, ir.right, y, GetString(STR_FORK_FIRM_NO_COMPETITORS));
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		const ForkFirm *f = this->GetFirm();
		if (f == nullptr) return;

		const uint32_t id = this->firm.base();
		auto send = [id](uint8_t what, int value) {
			Command<Commands::SetForkFirm>::Post(STR_ERROR_FORK_FIRM_CAN_T_CHANGE, id, what, static_cast<uint8_t>(value));
		};

		switch (widget) {
			case WID_FM_RATE_DOWN: send(0, std::max<int>(0, f->rate - 10)); break;
			case WID_FM_RATE_UP: send(0, std::min<int>(100, f->rate + 10)); break;
			case WID_FM_QUALITY_DOWN: send(1, std::max<int>(1, f->quality - 1)); break;
			case WID_FM_QUALITY_UP: send(1, std::min<int>(FORK_FIRM_MAX_QUALITY, f->quality + 1)); break;
			case WID_FM_PRICE_DOWN: send(2, std::max<int>(50, f->price_percent - 5)); break;
			case WID_FM_PRICE_UP: send(2, std::min<int>(200, f->price_percent + 5)); break;
			case WID_FM_DISCOUNT_DOWN: send(3, std::max<int>(0, f->discount - 5)); break;
			case WID_FM_DISCOUNT_UP: send(3, std::min<int>(50, f->discount + 5)); break;
			case WID_FM_DUMP: Command<Commands::DumpForkFirmStock>::Post(STR_ERROR_FORK_FIRM_CAN_T_CHANGE, id); break;
		}
	}
};

/** Make a pair of buttons for the firm window. */
#define FM_BUTTON_PAIR(down, up, down_str, up_str, tip) \
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize), \
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, down), SetMinimalSize(100, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(down_str, tip), \
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, up), SetMinimalSize(100, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(up_str, tip), \
	EndContainer()

static constexpr std::initializer_list<NWidgetPart> _nested_fork_firm_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::Brown),
		NWidget(WWT_CAPTION, Colours::Brown, WID_FM_CAPTION), SetToolTip(STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, Colours::Brown),
		NWidget(WWT_STICKYBOX, Colours::Brown),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PANEL, Colours::Brown, WID_FM_HQ), SetMinimalSize(110, 110), SetFill(1, 0), SetResize(1, 0),
		EndContainer(),
		NWidget(WWT_PANEL, Colours::Brown, WID_FM_LAB), SetMinimalSize(110, 110), SetFill(1, 0), SetResize(1, 0),
		EndContainer(),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::Brown, WID_FM_INFO), SetResize(1, 1), SetFill(1, 1),
	EndContainer(),
	FM_BUTTON_PAIR(WID_FM_RATE_DOWN, WID_FM_RATE_UP, STR_FORK_FIRM_RATE_DOWN, STR_FORK_FIRM_RATE_UP, STR_FORK_FIRM_RATE_TOOLTIP),
	FM_BUTTON_PAIR(WID_FM_QUALITY_DOWN, WID_FM_QUALITY_UP, STR_FORK_FIRM_QUALITY_DOWN, STR_FORK_FIRM_QUALITY_UP, STR_FORK_FIRM_QUALITY_TOOLTIP),
	FM_BUTTON_PAIR(WID_FM_PRICE_DOWN, WID_FM_PRICE_UP, STR_FORK_FIRM_PRICE_DOWN, STR_FORK_FIRM_PRICE_UP, STR_FORK_FIRM_PRICE_TOOLTIP),
	FM_BUTTON_PAIR(WID_FM_DISCOUNT_DOWN, WID_FM_DISCOUNT_UP, STR_FORK_FIRM_DISCOUNT_DOWN, STR_FORK_FIRM_DISCOUNT_UP, STR_FORK_FIRM_DISCOUNT_TOOLTIP),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FM_DUMP), SetMinimalSize(100, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_FIRM_DUMP, STR_FORK_FIRM_DUMP_TOOLTIP),
		NWidget(WWT_RESIZEBOX, Colours::Brown),
	EndContainer(),
};

static WindowDesc _fork_firm_desc(
	WindowPosition::Automatic, "fork_firm", 0, 0,
	WindowClass::ForkFirm, WindowClass::None,
	{},
	_nested_fork_firm_widgets
);

/**
 * Show the window to run a firm.
 * @param firm Index of the firm.
 */
void ShowForkFirmWindow(uint firm)
{
	AllocateWindowDescFront<ForkFirmWindow>(_fork_firm_desc, firm);
}
