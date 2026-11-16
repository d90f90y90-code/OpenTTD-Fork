/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_usedmarket_gui.cpp Window of the used vehicle market. */

#include "stdafx.h"

#include "command_func.h"
#include "company_base.h"
#include "company_func.h"
#include "engine_type.h"
#include "fork_economy.h"
#include "fork_lease.h"
#include "fork_lease_cmd.h"
#include "fork_logos.h"
#include "fork_usedmarket.h"
#include "fork_usedmarket_cmd.h"
#include "gfx_func.h"
#include "palette_func.h"
#include "strings_func.h"
#include "tilehighlight_func.h"
#include "window_func.h"
#include "window_gui.h"
#include "zoom_func.h"

#include "table/sprites.h"
#include "table/strings.h"

#include "safeguards.h"

/** Widgets of the used vehicle market window. */
enum ForkUsedMarketWidgets : WidgetID {
	WID_UM_CAPTION, ///< Caption of the window.
	WID_UM_DEALER_0, ///< Dealer TTD MAX.
	WID_UM_DEALER_1, ///< Dealer TAJCAR.
	WID_UM_DEALER_2, ///< Dealer LIMANO.
	WID_UM_DEALER_3, ///< Dealer SEAMAR.
	WID_UM_DEALER_4, ///< Dealer AIRIX.
	WID_UM_OFFERS, ///< Panel with the offers of the selected dealer.
	WID_UM_BUY, ///< Buy the selected offer.
	WID_UM_MODE_USED, ///< Show the used vehicles that can be bought.
	WID_UM_MODE_LEASE, ///< Show the vehicles that can be paid in installments.
	WID_UM_MODE_RENT, ///< Show the vehicles that can be rented.
	WID_UM_MONTH_DOWN, ///< One month less.
	WID_UM_MONTH_UP, ///< One month more.
};

static constexpr ForkLogo _dealer_logos[FORK_DEALER_COUNT] = {ForkLogo::TtdMax, ForkLogo::Tajcar, ForkLogo::Limano, ForkLogo::Seamar, ForkLogo::Airix};

/** Window that shows the offers of the five dealers. */
struct ForkUsedMarketWindow : Window {
	uint dealer = 0; ///< Dealer that is shown.
	int selected = -1; ///< Index in #offers of the offer that is selected, or -1.
	bool placing = false; ///< Whether the player is asked to point out a depot.
	uint mode = 0; ///< What is shown: 0 used vehicles, 1 installment contracts, 2 rental contracts.
	uint months = 12; ///< Duration of the contract that is wanted, for installment and rental contracts.
	std::vector<ForkUsedOffer> offers; ///< Offers of the shown dealer.

	ForkUsedMarketWindow(WindowDesc &desc, WindowNumber number) : Window(desc)
	{
		this->InitNested(number);
		this->Refresh();
	}

	/** Get the offers again and check that the selection is still possible. */
	void Refresh()
	{
		this->offers = GetForkUsedOffers(this->dealer, _local_company);
		if (this->selected >= static_cast<int>(this->offers.size()) || (this->selected >= 0 && this->offers[this->selected].sold >= this->offers[this->selected].stock)) {
			this->selected = -1;
			if (this->placing) {
				ResetObjectToPlace();
				this->placing = false;
			}
		}
		this->SetWidgetDisabledState(WID_UM_BUY, this->selected < 0);
		this->SetWidgetLoweredState(WID_UM_MODE_USED, this->mode == 0);
		this->SetWidgetLoweredState(WID_UM_MODE_LEASE, this->mode == 1);
		this->SetWidgetLoweredState(WID_UM_MODE_RENT, this->mode == 2);
		this->months = std::clamp<uint>(this->months, this->GetLowestMonths(), FORK_LEASE_MAX_MONTHS);
		this->SetWidgetDisabledState(WID_UM_MONTH_DOWN, this->mode == 0 || this->months <= this->GetLowestMonths());
		this->SetWidgetDisabledState(WID_UM_MONTH_UP, this->mode == 0 || this->months >= FORK_LEASE_MAX_MONTHS);
	}

	/** @return The shortest duration that can be chosen now: for rent it is what the selected offer comes with. */
	uint GetLowestMonths() const
	{
		if (this->mode == 1) return FORK_LEASE_MIN_MONTHS;
		if (this->mode == 2) {
			if (this->selected >= 0 && this->selected < static_cast<int>(this->offers.size())) return GetForkRentalBaseMonths(this->dealer, this->offers[this->selected].slot, _local_company);
			return 3;
		}
		return 0;
	}

	/** @return The terms of a contract for an offer, with the duration that is wanted. */
	ForkLeaseTerms GetTerms(const ForkUsedOffer &o) const
	{
		if (this->mode == 1) return GetForkInstallmentTerms(o.price, this->months);
		return GetForkRentalTerms(o.price, GetForkRentalBaseMonths(this->dealer, o.slot, _local_company), this->months);
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_UM_BUY) return GetString(this->mode == 1 ? STR_FORK_UM_LEASE_BUY : (this->mode == 2 ? STR_FORK_UM_RENT_BUY : STR_FORK_UM_BUY));
		return this->Window::GetWidgetString(widget, stringid);
	}

	int GetLineHeight() const
	{
		return GetCharacterHeight(FontSize::Normal);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget != WID_UM_OFFERS) return;

		size.width = std::max<uint>(size.width, ScaleGUITrad(440));
		size.height = (3 + 2 * FORK_OFFERS_PER_DEALER) * this->GetLineHeight() + padding.height;
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, bool gui_scope = true) override
	{
		if (!gui_scope) return;
		this->Refresh();
		this->SetDirty();
	}

	void OnPlaceObjectAbort() override
	{
		this->placing = false;
		this->SetDirty();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget >= WID_UM_DEALER_0 && widget <= WID_UM_DEALER_4) {
			const uint index = widget - WID_UM_DEALER_0;
			const Rect ir = r.Shrink(WidgetDimensions::scaled.frametext);
			DrawForkLogo(_dealer_logos[index], ir);
			if (index == this->dealer) {
				const int t = std::max(2, ScaleGUITrad(2));
				GfxFillRect(r.left, r.top, r.right, r.top + t - 1, PC_WHITE);
				GfxFillRect(r.left, r.bottom - t + 1, r.right, r.bottom, PC_WHITE);
				GfxFillRect(r.left, r.top, r.left + t - 1, r.bottom, PC_WHITE);
				GfxFillRect(r.right - t + 1, r.top, r.right, r.bottom, PC_WHITE);
			}
			return;
		}

		if (widget != WID_UM_OFFERS) return;

		const Rect ir = r.Shrink(WidgetDimensions::scaled.frametext);
		const int line = this->GetLineHeight();
		int y = ir.top;

		DrawString(ir.left, ir.right, y, GetString(STR_FORK_UM_DEALER_0 + this->dealer));
		y += line;
		StringID hint = this->mode == 1 ? STR_FORK_UM_LEASE_HINT : (this->mode == 2 ? STR_FORK_UM_RENT_HINT : STR_FORK_UM_OFFERS_HINT);
		DrawString(ir.left, ir.right, y, GetString(this->offers.empty() ? STR_FORK_UM_NO_OFFERS : (this->placing ? STR_FORK_UM_PICK_DEPOT : hint)));
		y += line;
		if (this->mode != 0) {
			uint installments, rentals;
			Money monthly;
			GetForkLeaseSummary(_local_company, installments, rentals, monthly);
			DrawString(ir.left, ir.right, y, GetString(STR_FORK_UM_CONTRACTS, installments, rentals, monthly));
		}
		y += 2 * line;

		for (size_t i = 0; i < this->offers.size(); i++) {
			const ForkUsedOffer &o = this->offers[i];
			const int top = y + static_cast<int>(i) * 2 * line;
			if (static_cast<int>(i) == this->selected) GfxFillRect(ir.left, top, ir.right, top + 2 * line - 1, PC_WHITE);

			DrawString(ir.left, ir.right, top, GetString(STR_FORK_UM_OFFER_LINE1, PackEngineNameDParam(o.engine, EngineNameContext::PurchaseList), o.age_months / 12, o.age_months % 12));

			StringID second = STR_FORK_UM_OFFER_FAIR;
			if (o.sold >= o.stock) {
				second = STR_FORK_UM_OFFER_SOLD;
			} else if (o.deal < 0) {
				second = STR_FORK_UM_OFFER_BARGAIN;
			} else if (o.deal > 0) {
				second = STR_FORK_UM_OFFER_PRICEY;
			}
			if (this->mode != 0 && o.sold < o.stock) {
				const ForkLeaseTerms t = this->GetTerms(o);
				if (this->mode == 1) {
					DrawString(ir.left, ir.right, top + line, GetString(STR_FORK_UM_LEASE_LINE, t.down, t.payment, t.months, t.interest_percent, t.total));
				} else {
					DrawString(ir.left, ir.right, top + line, GetString(STR_FORK_UM_RENT_LINE, t.payment, t.months, t.base_months, t.surcharge_percent));
				}
			} else {
				DrawString(ir.left, ir.right, top + line, GetString(second, o.price_percent, o.price, o.new_price, o.stock - std::min(o.sold, o.stock)));
			}
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		if (widget >= WID_UM_DEALER_0 && widget <= WID_UM_DEALER_4) {
			this->dealer = widget - WID_UM_DEALER_0;
			this->selected = -1;
			this->Refresh();
			this->SetDirty();
			return;
		}

		if (widget >= WID_UM_MODE_USED && widget <= WID_UM_MODE_RENT) {
			this->mode = widget - WID_UM_MODE_USED;
			this->selected = -1;
			if (this->placing) {
				ResetObjectToPlace();
				this->placing = false;
			}
			this->Refresh();
			this->SetDirty();
			return;
		}

		if (widget == WID_UM_MONTH_DOWN || widget == WID_UM_MONTH_UP) {
			if (widget == WID_UM_MONTH_UP) {
				this->months++;
			} else if (this->months > 0) {
				this->months--;
			}
			this->Refresh();
			this->SetDirty();
			return;
		}

		if (widget == WID_UM_OFFERS) {
			const NWidgetBase *nwi = this->GetWidget<NWidgetBase>(WID_UM_OFFERS);
			const int line = this->GetLineHeight();
			const int y = pt.y - static_cast<int>(nwi->pos_y) - WidgetDimensions::scaled.frametext.top - 3 * line;
			if (y < 0) return;

			const size_t row = static_cast<size_t>(y / (2 * line));
			if (row < this->offers.size() && this->offers[row].sold < this->offers[row].stock) {
				this->selected = static_cast<int>(row);
				if (this->mode == 2) this->months = std::max(this->months, this->GetLowestMonths());
				this->Refresh();
				this->SetDirty();
			}
			return;
		}

		if (widget == WID_UM_BUY && this->selected >= 0) {
			this->placing = true;
			SetObjectToPlaceWnd(ANIMCURSOR_PICKSTATION, PAL_NONE, HT_RECT, this);
			this->SetDirty();
		}
	}

	void OnPlaceObject([[maybe_unused]] Point pt, TileIndex tile) override
	{
		if (!this->placing || this->selected < 0 || this->selected >= static_cast<int>(this->offers.size())) return;

		if (this->mode == 0) {
			Command<Commands::BuyUsedVehicle>::Post(STR_ERROR_FORK_UM_CAN_T_BUY, tile, static_cast<uint8_t>(this->dealer), this->offers[this->selected].slot);
		} else {
			const ForkUsedOffer &o = this->offers[this->selected];
			Command<Commands::LeaseForkVehicle>::Post(STR_ERROR_FORK_LEASE_CAN_T, tile, static_cast<uint8_t>(this->dealer), o.slot, static_cast<uint8_t>(this->mode), static_cast<uint8_t>(this->GetTerms(o).months));
		}
		ResetObjectToPlace();
		this->placing = false;
		this->SetDirty();
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_fork_used_market_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::Grey),
		NWidget(WWT_CAPTION, Colours::Grey, WID_UM_CAPTION), SetStringTip(STR_FORK_UM_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, Colours::Grey),
		NWidget(WWT_STICKYBOX, Colours::Grey),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PANEL, Colours::Grey, WID_UM_DEALER_0), SetMinimalSize(72, 52), SetFill(1, 0), SetResize(1, 0), SetToolTip(STR_FORK_UM_DEALER_TOOLTIP),
		EndContainer(),
		NWidget(WWT_PANEL, Colours::Grey, WID_UM_DEALER_1), SetMinimalSize(72, 52), SetFill(1, 0), SetResize(1, 0), SetToolTip(STR_FORK_UM_DEALER_TOOLTIP),
		EndContainer(),
		NWidget(WWT_PANEL, Colours::Grey, WID_UM_DEALER_2), SetMinimalSize(72, 52), SetFill(1, 0), SetResize(1, 0), SetToolTip(STR_FORK_UM_DEALER_TOOLTIP),
		EndContainer(),
		NWidget(WWT_PANEL, Colours::Grey, WID_UM_DEALER_3), SetMinimalSize(72, 52), SetFill(1, 0), SetResize(1, 0), SetToolTip(STR_FORK_UM_DEALER_TOOLTIP),
		EndContainer(),
		NWidget(WWT_PANEL, Colours::Grey, WID_UM_DEALER_4), SetMinimalSize(72, 52), SetFill(1, 0), SetResize(1, 0), SetToolTip(STR_FORK_UM_DEALER_TOOLTIP),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_UM_MODE_USED), SetMinimalSize(80, 14), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_UM_MODE_USED, STR_FORK_UM_MODE_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_UM_MODE_LEASE), SetMinimalSize(80, 14), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_UM_MODE_LEASE, STR_FORK_UM_MODE_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_UM_MODE_RENT), SetMinimalSize(80, 14), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_UM_MODE_RENT, STR_FORK_UM_MODE_TOOLTIP),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::Grey, WID_UM_OFFERS), SetResize(1, 1), SetFill(1, 1),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_UM_MONTH_DOWN), SetMinimalSize(80, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_UM_MONTH_DOWN, STR_FORK_UM_MONTH_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_UM_MONTH_UP), SetMinimalSize(80, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_UM_MONTH_UP, STR_FORK_UM_MONTH_TOOLTIP),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_UM_BUY), SetMinimalSize(120, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_UM_BUY, STR_FORK_UM_BUY_TOOLTIP),
		NWidget(WWT_RESIZEBOX, Colours::Grey),
	EndContainer(),
};

static WindowDesc _fork_used_market_desc(
	WindowPosition::Automatic, {}, 0, 0,
	WindowClass::ForkUsedMarket, WindowClass::None,
	{},
	_nested_fork_used_market_widgets
);

/** Show the window of the used vehicle market. */
void ShowForkUsedMarketWindow()
{
	AllocateWindowDescFront<ForkUsedMarketWindow>(_fork_used_market_desc, 0);
}
