/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_products_gui.cpp Windows of the fork economy: company overview, products and statistics. */

#include "stdafx.h"

#include <algorithm>
#include <vector>

#include "command_func.h"
#include "company_base.h"
#include "company_func.h"
#include "company_manager_face.h"
#include "error.h"
#include "fork_economy.h"
#include "fork_firm.h"
#include "fork_products.h"
#include "fork_products_cmd.h"
#include "fork_stock.h"
#include "fork_stock_cmd.h"
#include "gfx_func.h"
#include "strings_func.h"
#include "textbuf_gui.h"
#include "timer/timer_game_calendar.h"
#include "town.h"
#include "window_func.h"
#include "window_gui.h"
#include "zoom_func.h"

#include "table/strings.h"

#include "safeguards.h"

/** Open the product creation window; defined after its window description. */
void ShowForkProductCreateWindow();

/** @return All products of one company in catalog order. */
static std::vector<const ForkUserProduct *> GetProductsOf(CompanyID company)
{
	std::vector<const ForkUserProduct *> result;
	for (const ForkUserProduct *p : ForkUserProduct::Iterate()) {
		if (p->owner == company) result.push_back(p);
	}
	std::sort(result.begin(), result.end(), [](const ForkUserProduct *a, const ForkUserProduct *b) { return a->index < b->index; });
	return result;
}

/** @return Number of staff of a company in the labor market. */
static uint32_t GetCompanyStaffCount(CompanyID company)
{
	uint32_t total = 0;
	for (const ForkAsset *a : ForkAsset::Iterate()) {
		if (a->owner == company && a->type == static_cast<uint8_t>(ForkAssetType::Staff)) total += a->data[2] > 0 ? static_cast<uint32_t>(a->data[2]) : 0;
	}
	return total;
}

/* ---------------------------------------------------------------------------------------------------------------- */
/* Company overview.                                                                                                 */

enum ForkCompanyWidgets : WidgetID {
	WID_FCO_CAPTION,
	WID_FCO_FACE,
	WID_FCO_INFO,
	WID_FCO_PRODUCTS,
	WID_FCO_STATS,
	WID_FCO_SHARE,
	WID_FCO_SELL,
};

static constexpr StringID _fork_industry_names[FORK_INDUSTRY_TYPE_COUNT] = {
	STR_FORK_INDUSTRY_HOUSEHOLD,
	STR_FORK_INDUSTRY_LIFE_TECH,
	STR_FORK_INDUSTRY_COMMUNICATIONS,
	STR_FORK_INDUSTRY_BEVERAGES,
	STR_FORK_INDUSTRY_FOOD,
	STR_FORK_INDUSTRY_CLOTHING,
	STR_FORK_INDUSTRY_MEDICAL,
	STR_FORK_INDUSTRY_GAMES,
	STR_FORK_INDUSTRY_APPS,
	STR_FORK_INDUSTRY_BUILDING,
	STR_FORK_INDUSTRY_ENERGY,
};

struct ForkCompanyWindow : Window {
	ForkCompanyWindow(WindowDesc &desc, WindowNumber number) : Window(desc)
	{
		this->InitNested(number);
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_FCO_CAPTION && Company::IsValidID(static_cast<CompanyID>(this->window_number))) {
			return GetString(STR_FORK_COMPANY_CAPTION, Company::Get(static_cast<CompanyID>(this->window_number))->name);
		}
		return this->Window::GetWidgetString(widget, stringid);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget == WID_FCO_FACE) {
			size.width = std::max<uint>(size.width, ScaleGUITrad(92));
			size.height = std::max<uint>(size.height, ScaleGUITrad(119));
		} else if (widget == WID_FCO_INFO) {
			size.width = std::max<uint>(size.width, ScaleGUITrad(330));
			size.height = 4 * GetCharacterHeight(FontSize::Normal) + padding.height;
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		const CompanyID company = static_cast<CompanyID>(this->window_number);
		if (!Company::IsValidID(company)) return;
		const Company *c = Company::Get(company);

		if (widget == WID_FCO_FACE) {
			DrawCompanyManagerFace(c->face, c->colour, r);
			return;
		}
		if (widget != WID_FCO_INFO) return;

		Money expected = 0;
		Money current = 0;
		for (const ForkFirm *f : ForkFirm::Iterate()) {
			if (f->owner != company) continue;
			expected += f->profit_average * 12;
			current += f->profit_last;
		}
		for (const ForkUserProduct *p : GetProductsOf(company)) {
			expected += (p->income_last - p->cost_last) * 12;
			current += p->income_last - p->cost_last;
		}

		const Rect ir = r.Shrink(WidgetDimensions::scaled.frametext);
		const int line = GetCharacterHeight(FontSize::Normal);
		int y = ir.top;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_COMPANY_INFO, GetCompanyStaffCount(company), expected, current));
		y += line;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_COMPANY_VALUE, GetForkCompanySalePrice(company)));
		y += line;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_PRODUCTS_CAPTION, GetProductsOf(company).size()));
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_FCO_PRODUCTS: ShowForkProductsWindow(); break;
			case WID_FCO_STATS: ShowForkProductStatsWindow(); break;
			case WID_FCO_SHARE: {
				for (const ForkFirm *f : ForkFirm::Iterate()) {
					if (f->owner != static_cast<CompanyID>(this->window_number)) continue;
					Command<Commands::SellForkFirmShare>::Post(STR_ERROR_FORK_FIRM_CAN_T_CHANGE, f->index.base(), uint8_t(10));
					return;
				}
				ShowForkExchangeWindow();
				break;
			}
			case WID_FCO_SELL:
				Command<Commands::SellForkCompany>::Post(STR_ERROR_FORK_FIRM_CAN_T_CHANGE, static_cast<CompanyID>(this->window_number));
				this->Close();
				break;
		}
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_fork_company_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::Brown),
		NWidget(WWT_CAPTION, Colours::Brown, WID_FCO_CAPTION), SetToolTip(STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, Colours::Brown),
		NWidget(WWT_STICKYBOX, Colours::Brown),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_EMPTY, Colours::Invalid, WID_FCO_FACE), SetMinimalSize(92, 119), SetFill(0, 0),
		NWidget(WWT_PANEL, Colours::Brown, WID_FCO_INFO), SetResize(1, 1), SetFill(1, 1),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FCO_PRODUCTS), SetMinimalSize(120, 12), SetFill(1, 0), SetResize(1, 0), SetStringTip(STR_FORK_COMPANY_PRODUCTS, STR_FORK_COMPANY_PRODUCTS_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FCO_STATS), SetMinimalSize(100, 12), SetFill(1, 0), SetResize(1, 0), SetStringTip(STR_FORK_COMPANY_STATS, STR_FORK_COMPANY_STATS_TOOLTIP),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FCO_SHARE), SetMinimalSize(90, 12), SetFill(1, 0), SetResize(1, 0), SetStringTip(STR_FORK_PRODUCTS_SELL_SHARE, STR_FORK_PRODUCTS_SELL_SHARE_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FCO_SELL), SetMinimalSize(120, 12), SetFill(1, 0), SetResize(1, 0), SetStringTip(STR_FORK_COMPANY_SELL, STR_FORK_COMPANY_SELL_TOOLTIP),
		NWidget(WWT_RESIZEBOX, Colours::Brown),
	EndContainer(),
};

static WindowDesc _fork_company_desc(
	WindowPosition::Automatic, {}, 0, 0,
	WindowClass::ForkCompany, WindowClass::None,
	{},
	_nested_fork_company_widgets
);

void ShowForkCompanyWindow(CompanyID company)
{
	AllocateWindowDescFront<ForkCompanyWindow>(_fork_company_desc, company);
}

/* ---------------------------------------------------------------------------------------------------------------- */
/* Product list.                                                                                                     */

enum ForkProductsWidgets : WidgetID {
	WID_FP_CAPTION,
	WID_FP_LIST,
	WID_FP_NEW,
	WID_FP_QTY_DOWN,
	WID_FP_QTY_UP,
	WID_FP_PRICE_DOWN,
	WID_FP_PRICE_UP,
	WID_FP_QUALITY_DOWN,
	WID_FP_QUALITY_UP,
	WID_FP_DISCOUNT_DOWN,
	WID_FP_DISCOUNT_UP,
	WID_FP_RENAME,
	WID_FP_SHARE,
};

static constexpr uint FP_LIST_LINES = 8; ///< Visible product rows.

struct ForkProductsWindow : Window {
	uint selected = 0; ///< Selected product row.

	ForkProductsWindow(WindowDesc &desc, WindowNumber number) : Window(desc)
	{
		this->InitNested(number);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget != WID_FP_LIST) return;
		size.width = std::max<uint>(size.width, ScaleGUITrad(460));
		size.height = (FP_LIST_LINES + 1) * GetCharacterHeight(FontSize::Normal) + padding.height;
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, bool gui_scope = true) override
	{
		if (!gui_scope) return;
		const auto products = GetProductsOf(_local_company);
		if (!products.empty()) this->selected = std::min<uint>(this->selected, static_cast<uint>(products.size()) - 1);
		this->SetDirty();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_FP_LIST) return;
		const Rect ir = r.Shrink(WidgetDimensions::scaled.frametext);
		const int line = GetCharacterHeight(FontSize::Normal);
		const auto products = GetProductsOf(_local_company);
		int y = ir.top;

		if (products.empty()) {
			DrawString(ir.left, ir.right, y, STR_FORK_PRODUCTS_EMPTY);
			return;
		}

		uint row = 0;
		for (const ForkUserProduct *p : products) {
			if (row < this->selected || row >= this->selected + FP_LIST_LINES) { row++; continue; }
			DrawString(ir.left, ir.right, y, GetString(STR_FORK_PRODUCTS_ROW, p->name, p->quantity, static_cast<Money>(p->price), p->quality, p->discount, p->popularity), row == this->selected ? TextColour::White : TextColour::Black);
			y += line;
			row++;
		}
	}

	void ChangeSelected(uint8_t what, int delta)
	{
		const auto products = GetProductsOf(_local_company);
		if (this->selected >= products.size()) return;
		const ForkUserProduct *p = products[this->selected];
		uint value = 0;
		switch (what) {
			case 0: value = std::max<uint>(1U, std::min<uint>(10000U, p->quantity + delta * 10)); break;
			case 1: value = std::max<uint>(1U, std::min<uint>(1000000U, p->price + delta * 5)); break;
			case 2: value = std::clamp<uint>(p->quality + delta, 1, 5); break;
			case 3: value = std::clamp<uint>(p->discount + delta * 5, 0, 50); break;
		}
		Command<Commands::SetForkProduct>::Post(STR_ERROR_FORK_FIRM_CAN_T_CHANGE, p->index.base(), what, value);
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		if (widget == WID_FP_NEW) {
			ShowForkProductCreateWindow();
			return;
		}
		if (widget == WID_FP_RENAME) {
			const auto products = GetProductsOf(_local_company);
			if (this->selected < products.size()) ShowQueryString(products[this->selected]->name, STR_FORK_PRODUCT_CREATE_NAME, MAX_LENGTH_COMPANY_NAME_CHARS, this, CS_ALPHANUMERAL, {});
			return;
		}
		if (widget == WID_FP_SHARE) {
			for (const ForkFirm *f : ForkFirm::Iterate()) {
				if (f->owner != _local_company) continue;
				Command<Commands::SellForkFirmShare>::Post(STR_ERROR_FORK_FIRM_CAN_T_CHANGE, f->index.base(), uint8_t(10));
				return;
			}
			ShowForkExchangeWindow();
			return;
		}

		if (widget == WID_FP_LIST) {
			const int line = GetCharacterHeight(FontSize::Normal);
			const NWidgetBase *nwid = this->GetWidget<NWidgetBase>(WID_FP_LIST);
			const int row = (pt.y - static_cast<int>(nwid->pos_y) - WidgetDimensions::scaled.frametext.top) / line;
			if (row >= 0 && row < static_cast<int>(FP_LIST_LINES)) {
				this->selected = this->selected + row;
				const auto products = GetProductsOf(_local_company);
				if (this->selected >= products.size()) this->selected = products.empty() ? 0 : static_cast<uint>(products.size()) - 1;
				this->SetDirty();
			}
			return;
		}

		switch (widget) {
			case WID_FP_QTY_DOWN: this->ChangeSelected(0, -1); break;
			case WID_FP_QTY_UP: this->ChangeSelected(0, 1); break;
			case WID_FP_PRICE_DOWN: this->ChangeSelected(1, -1); break;
			case WID_FP_PRICE_UP: this->ChangeSelected(1, 1); break;
			case WID_FP_QUALITY_DOWN: this->ChangeSelected(2, -1); break;
			case WID_FP_QUALITY_UP: this->ChangeSelected(2, 1); break;
			case WID_FP_DISCOUNT_DOWN: this->ChangeSelected(3, -1); break;
			case WID_FP_DISCOUNT_UP: this->ChangeSelected(3, 1); break;
		}
	}

	void OnQueryTextFinished(std::optional<std::string> str) override
	{
		if (!str.has_value() || str->empty()) return;
		const auto products = GetProductsOf(_local_company);
		if (this->selected < products.size()) Command<Commands::RenameForkProduct>::Post(STR_ERROR_FORK_FIRM_CAN_T_CHANGE, products[this->selected]->index.base(), *str);
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_fork_products_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::Brown),
		NWidget(WWT_CAPTION, Colours::Brown), SetStringTip(STR_FORK_PRODUCTS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, Colours::Brown),
		NWidget(WWT_STICKYBOX, Colours::Brown),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::Brown, WID_FP_LIST), SetResize(1, 1), SetFill(1, 1),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FP_QTY_DOWN), SetMinimalSize(60, 12), SetStringTip(STR_FORK_PRODUCTS_QTY_DOWN, {}),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FP_QTY_UP), SetMinimalSize(60, 12), SetStringTip(STR_FORK_PRODUCTS_QTY_UP, {}),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FP_PRICE_DOWN), SetMinimalSize(60, 12), SetStringTip(STR_FORK_PRODUCTS_PRICE_DOWN, {}),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FP_PRICE_UP), SetMinimalSize(60, 12), SetStringTip(STR_FORK_PRODUCTS_PRICE_UP, {}),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FP_QUALITY_DOWN), SetMinimalSize(60, 12), SetStringTip(STR_FORK_PRODUCTS_QUALITY_DOWN, {}),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FP_QUALITY_UP), SetMinimalSize(60, 12), SetStringTip(STR_FORK_PRODUCTS_QUALITY_UP, {}),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FP_DISCOUNT_DOWN), SetMinimalSize(60, 12), SetStringTip(STR_FORK_PRODUCTS_DISCOUNT_DOWN, {}),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FP_DISCOUNT_UP), SetMinimalSize(60, 12), SetStringTip(STR_FORK_PRODUCTS_DISCOUNT_UP, {}),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FP_NEW), SetMinimalSize(100, 12), SetStringTip(STR_FORK_PRODUCTS_NEW, STR_FORK_PRODUCTS_NEW_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FP_RENAME), SetMinimalSize(80, 12), SetStringTip(STR_FORK_PRODUCTS_RENAME, {}),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FP_SHARE), SetMinimalSize(80, 12), SetStringTip(STR_FORK_PRODUCTS_SELL_SHARE, STR_FORK_PRODUCTS_SELL_SHARE_TOOLTIP),
	EndContainer(),
};

static WindowDesc _fork_products_desc(
	WindowPosition::Automatic, {}, 0, 0,
	WindowClass::ForkProducts, WindowClass::None,
	{},
	_nested_fork_products_widgets
);

/* ---------------------------------------------------------------------------------------------------------------- */
/* Create a product.                                                                                                 */

enum ForkProductCreateWidgets : WidgetID {
	WID_FPC2_CAPTION,
	WID_FPC2_INFO,
	WID_FPC2_NAME,
	WID_FPC2_INDUSTRY_0,
	WID_FPC2_INDUSTRY_LAST = WID_FPC2_INDUSTRY_0 + FORK_INDUSTRY_TYPE_COUNT - 1,
	WID_FPC2_PRODUCT_0,
	WID_FPC2_PRODUCT_LAST = WID_FPC2_PRODUCT_0 + 15,
	WID_FPC2_QTY_DOWN,
	WID_FPC2_QTY_UP,
	WID_FPC2_PRICE_DOWN,
	WID_FPC2_PRICE_UP,
	WID_FPC2_MAKE,
};

struct ForkProductCreateWindow : Window {
	uint industry = 0;     ///< Selected industry.
	uint product = 0;      ///< Selected catalog entry within the industry.
	uint32_t quantity = 10; ///< Monthly quantity of the new product.
	uint32_t price = 100;  ///< Price of one unit.
	std::string name;      ///< Name of the new product.

	ForkProductCreateWindow(WindowDesc *desc, WindowNumber number) : Window(*desc)
	{
		this->InitNested(number);
		this->SelectFirstProduct();
		this->name = GetString(STR_FORK_PRODUCT_CREATE_NAME);
	}

	void SelectFirstProduct()
	{
		this->product = 0;
		for (uint i = 0; i < FORK_CATALOG_COUNT; i++) {
			if (_fork_catalog[i].type == static_cast<ForkIndustryType>(this->industry)) {
				this->product = i;
				this->price = _fork_catalog[i].base_price;
				return;
			}
		}
	}

	std::vector<uint16_t> ProductsOfIndustry() const
	{
		std::vector<uint16_t> result;
		for (uint16_t i = 0; i < FORK_CATALOG_COUNT; i++) {
			if (_fork_catalog[i].type == static_cast<ForkIndustryType>(this->industry)) result.push_back(i);
		}
		return result;
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget != WID_FPC2_INFO) return;
		size.width = std::max<uint>(size.width, ScaleGUITrad(420));
		size.height = 5 * GetCharacterHeight(FontSize::Normal) + padding.height;
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_FPC2_NAME) return GetString(STR_FORK_PRODUCT_CREATE_NAME, this->name);
		if (widget >= WID_FPC2_PRODUCT_0 && widget <= WID_FPC2_PRODUCT_LAST) {
			const uint slot = widget - WID_FPC2_PRODUCT_0;
			const auto list = this->ProductsOfIndustry();
			if (slot >= list.size()) return std::string{};
			const ForkCatalogEntry *e = GetForkCatalogEntry(list[slot]);
			if (e == nullptr) return std::string{};
			const bool unlocked = IsForkProductUnlocked(list[slot], ForkProductCurrentYear());
			return unlocked ? GetString(STR_JUST_RAW_STRING, std::string(e->arabic)) : GetString(STR_FORK_PRODUCT_CREATE_LOCKED, e->unlock_year);
		}
		return this->Window::GetWidgetString(widget, stringid);
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_FPC2_INFO) return;
		const Rect ir = r.Shrink(WidgetDimensions::scaled.frametext);
		const int line = GetCharacterHeight(FontSize::Normal);
		int y = ir.top;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_PRODUCT_CREATE_INDUSTRY, _fork_industry_names[this->industry]));
		y += line;
		const ForkCatalogEntry *e = GetForkCatalogEntry(this->product);
		if (e != nullptr) DrawString(ir.left, ir.right, y, GetString(STR_FORK_PRODUCT_CREATE_LIST, GetString(STR_JUST_RAW_STRING, std::string(e->arabic)), static_cast<Money>(e->base_price)));
		y += line;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_PRODUCT_CREATE_SETTINGS, this->quantity, static_cast<Money>(this->price)));
		y += line;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_FIRM_CREATE_COST, GetForkFirmFoundingCost()));
		y += line;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_PRODUCT_CREATE_MAKE, this->name));
	}

	void OnQueryTextFinished(std::optional<std::string> str) override
	{
		if (str.has_value() && !str->empty()) this->name = *str;
		this->SetDirty();
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		if (widget >= WID_FPC2_INDUSTRY_0 && widget <= WID_FPC2_INDUSTRY_LAST) {
			this->industry = widget - WID_FPC2_INDUSTRY_0;
			this->SelectFirstProduct();
			this->SetDirty();
			return;
		}
		if (widget >= WID_FPC2_PRODUCT_0 && widget <= WID_FPC2_PRODUCT_LAST) {
			const auto list = this->ProductsOfIndustry();
			const uint slot = widget - WID_FPC2_PRODUCT_0;
			if (slot < list.size() && IsForkProductUnlocked(list[slot], ForkProductCurrentYear())) this->product = list[slot];
			this->SetDirty();
			return;
		}
		if (widget == WID_FPC2_NAME) {
			ShowQueryString(this->name, STR_FORK_PRODUCT_CREATE_NAME, MAX_LENGTH_COMPANY_NAME_CHARS, this, CS_ALPHANUMERAL, {});
			return;
		}
		switch (widget) {
			case WID_FPC2_QTY_DOWN: this->quantity = std::max<uint32_t>(1, this->quantity - std::min<uint32_t>(10, this->quantity - 1)); break;
			case WID_FPC2_QTY_UP: this->quantity = std::min<uint32_t>(10000, this->quantity + 10); break;
			case WID_FPC2_PRICE_DOWN: this->price = std::max<uint32_t>(1, this->price - std::min<uint32_t>(5, this->price - 1)); break;
			case WID_FPC2_PRICE_UP: this->price = std::min<uint32_t>(1000000, this->price + 5); break;
		}
		if (widget >= WID_FPC2_QTY_DOWN && widget <= WID_FPC2_PRICE_UP) {
			this->SetDirty();
			return;
		}
		if (widget == WID_FPC2_MAKE) {
			if (!IsForkProductUnlocked(this->product, ForkProductCurrentYear())) {
				ShowErrorMessage(GetEncodedString(STR_ERROR_FORK_PRODUCT_LOCKED), {}, WarningLevel::Info);
				return;
			}
			Command<Commands::CreateForkProduct>::Post(STR_ERROR_FORK_PRODUCT_NAME, static_cast<uint16_t>(this->product), this->quantity, this->price, this->name);
			this->Close();
		}
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_fork_product_create_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::Brown),
		NWidget(WWT_CAPTION, Colours::Brown, WID_FPC2_CAPTION), SetStringTip(STR_FORK_PRODUCT_CREATE_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, Colours::Brown),
		NWidget(WWT_STICKYBOX, Colours::Brown),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::Brown, WID_FPC2_INFO), SetResize(1, 0), SetFill(1, 0),
	EndContainer(),
	NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FPC2_NAME), SetMinimalSize(120, 12), SetFill(1, 0), SetResize(1, 0),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FPC2_INDUSTRY_0 + 0), SetMinimalSize(70, 12), SetStringTip(STR_FORK_INDUSTRY_HOUSEHOLD, {}),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FPC2_INDUSTRY_0 + 1), SetMinimalSize(70, 12), SetStringTip(STR_FORK_INDUSTRY_LIFE_TECH, {}),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FPC2_INDUSTRY_0 + 2), SetMinimalSize(70, 12), SetStringTip(STR_FORK_INDUSTRY_COMMUNICATIONS, {}),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FPC2_INDUSTRY_0 + 3), SetMinimalSize(70, 12), SetStringTip(STR_FORK_INDUSTRY_BEVERAGES, {}),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FPC2_INDUSTRY_0 + 4), SetMinimalSize(70, 12), SetStringTip(STR_FORK_INDUSTRY_FOOD, {}),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FPC2_INDUSTRY_0 + 5), SetMinimalSize(70, 12), SetStringTip(STR_FORK_INDUSTRY_CLOTHING, {}),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FPC2_INDUSTRY_0 + 6), SetMinimalSize(70, 12), SetStringTip(STR_FORK_INDUSTRY_MEDICAL, {}),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FPC2_INDUSTRY_0 + 7), SetMinimalSize(70, 12), SetStringTip(STR_FORK_INDUSTRY_GAMES, {}),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FPC2_INDUSTRY_0 + 8), SetMinimalSize(70, 12), SetStringTip(STR_FORK_INDUSTRY_APPS, {}),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FPC2_INDUSTRY_0 + 9), SetMinimalSize(70, 12), SetStringTip(STR_FORK_INDUSTRY_BUILDING, {}),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FPC2_INDUSTRY_0 + 10), SetMinimalSize(70, 12), SetStringTip(STR_FORK_INDUSTRY_ENERGY, {}),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FPC2_PRODUCT_0 + 0), SetMinimalSize(70, 12),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FPC2_PRODUCT_0 + 1), SetMinimalSize(70, 12),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FPC2_PRODUCT_0 + 2), SetMinimalSize(70, 12),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FPC2_PRODUCT_0 + 3), SetMinimalSize(70, 12),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FPC2_PRODUCT_0 + 4), SetMinimalSize(70, 12),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FPC2_PRODUCT_0 + 5), SetMinimalSize(70, 12),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FPC2_PRODUCT_0 + 6), SetMinimalSize(70, 12),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FPC2_PRODUCT_0 + 7), SetMinimalSize(70, 12),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FPC2_QTY_DOWN), SetMinimalSize(70, 12), SetStringTip(STR_FORK_PRODUCTS_QTY_DOWN, {}),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FPC2_QTY_UP), SetMinimalSize(70, 12), SetStringTip(STR_FORK_PRODUCTS_QTY_UP, {}),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FPC2_PRICE_DOWN), SetMinimalSize(70, 12), SetStringTip(STR_FORK_PRODUCTS_PRICE_DOWN, {}),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FPC2_PRICE_UP), SetMinimalSize(70, 12), SetStringTip(STR_FORK_PRODUCTS_PRICE_UP, {}),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FPC2_MAKE), SetMinimalSize(120, 12), SetFill(1, 0), SetResize(1, 0), SetStringTip(STR_FORK_PRODUCT_CREATE_MAKE, {}),
		NWidget(WWT_RESIZEBOX, Colours::Brown),
	EndContainer(),
};

static WindowDesc _fork_product_create_desc(
	WindowPosition::Automatic, {}, 0, 0,
	WindowClass::ForkProducts, WindowClass::None,
	{},
	_nested_fork_product_create_widgets
);

void ShowForkProductCreateWindow()
{
	AllocateWindowDescFront<ForkProductCreateWindow>(_fork_product_create_desc, 0);
}

/* ---------------------------------------------------------------------------------------------------------------- */
/* Statistics.                                                                                                       */

enum ForkProductStatsWidgets : WidgetID {
	WID_FPS_CAPTION,
	WID_FPS_CHART,
	WID_FPS_FILTER,
};

struct ForkProductStatsWindow : Window {
	int filter = -1; ///< -1 = all products.

	ForkProductStatsWindow(WindowDesc &desc, WindowNumber number) : Window(desc)
	{
		this->InitNested(number);
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_FPS_FILTER) {
			if (this->filter < 0) return GetString(STR_FORK_STATS_ALL);
			const ForkUserProduct *p = ForkUserProduct::GetIfValid(static_cast<ForkUserProductID>(this->filter));
			return p != nullptr ? GetString(STR_JUST_RAW_STRING, p->name) : GetString(STR_FORK_STATS_ALL);
		}
		return this->Window::GetWidgetString(widget, stringid);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget != WID_FPS_CHART) return;
		size.width = std::max<uint>(size.width, ScaleGUITrad(400));
		size.height = 10 * GetCharacterHeight(FontSize::Normal) + padding.height;
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_FPS_CHART) return;
		auto products = GetProductsOf(_local_company);
		if (products.empty()) {
			DrawString(r.left, r.right, r.top, STR_FORK_STATS_EMPTY);
			return;
		}

		const Rect ir = r.Shrink(WidgetDimensions::scaled.frametext);
		const int line = GetCharacterHeight(FontSize::Normal);
		const int bar_width = std::max<int>(20, (ir.right - ir.left) * 2 / 3);
		int y = ir.top;
		for (const ForkUserProduct *p : products) {
			if (this->filter >= 0 && static_cast<int>(p->index.base()) != this->filter) continue;
			if (y + line > ir.bottom) break;
			DrawString(ir.left, ir.right, y, GetString(STR_FORK_STATS_LABEL, p->name, p->popularity, p->income_last));
			y += line;
			Rect bar{ir.left, y, ir.left + bar_width * p->popularity / 100, y + line / 2};
			GfxFillRect(bar, GetColourGradient(Colours::Green, Shade::Normal));
			y += line / 2;
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		if (widget != WID_FPS_FILTER) return;
		auto products = GetProductsOf(_local_company);
		if (products.empty()) return;
		if (this->filter < 0) {
			this->filter = static_cast<int>(products.front()->index.base());
		} else {
			bool found = false;
			for (size_t i = 0; i < products.size(); i++) {
				if (static_cast<int>(products[i]->index.base()) == this->filter) {
					if (i + 1 < products.size()) { this->filter = static_cast<int>(products[i + 1]->index.base()); found = true; }
					break;
				}
			}
			if (!found) this->filter = -1;
		}
		this->SetDirty();
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_fork_stats_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::Brown),
		NWidget(WWT_CAPTION, Colours::Brown), SetStringTip(STR_FORK_STATS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, Colours::Brown),
		NWidget(WWT_STICKYBOX, Colours::Brown),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::Brown, WID_FPS_CHART), SetResize(1, 1), SetFill(1, 1),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FPS_FILTER), SetMinimalSize(100, 12), SetFill(1, 0), SetResize(1, 0), SetStringTip(STR_FORK_STATS_FILTER, {}),
		NWidget(WWT_RESIZEBOX, Colours::Brown),
	EndContainer(),
};

static WindowDesc _fork_stats_desc(
	WindowPosition::Automatic, {}, 0, 0,
	WindowClass::ForkProductStats, WindowClass::None,
	{},
	_nested_fork_stats_widgets
);

void ShowForkProductStatsWindow()
{
	AllocateWindowDescFront<ForkProductStatsWindow>(_fork_stats_desc, 0);
}
