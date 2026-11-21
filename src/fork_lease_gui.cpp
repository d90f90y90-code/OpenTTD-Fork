/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_lease_gui.cpp Window that asks whether a rental contract that ended should be renewed. */

#include "stdafx.h"

#include "command_func.h"
#include "company_func.h"
#include "engine_type.h"
#include "fork_economy.h"
#include "fork_lease.h"
#include "fork_lease_cmd.h"
#include "gfx_func.h"
#include "strings_func.h"
#include "vehicle_base.h"
#include "window_func.h"
#include "window_gui.h"
#include "zoom_func.h"

#include "table/strings.h"

#include "safeguards.h"

/** Widgets of the renewal window. */
enum ForkLeaseRenewWidgets : WidgetID {
	WID_FLR_CAPTION, ///< Caption of the window.
	WID_FLR_INFO, ///< Panel with the texts.
	WID_FLR_RENEW, ///< Renew the contract.
	WID_FLR_RETURN, ///< Return the vehicle.
};

static constexpr uint FLR_INFO_LINES = 4; ///< Number of text lines in the information panel.

/** Window that asks what to do with a rental contract that ended. The window number is the vehicle. */
struct ForkLeaseRenewWindow : Window {
	VehicleID vehicle; ///< Vehicle of the contract.

	ForkLeaseRenewWindow(WindowDesc &desc, WindowNumber number) : Window(desc), vehicle(static_cast<VehicleID>(number))
	{
		this->InitNested(number);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget != WID_FLR_INFO) return;

		size.width = std::max<uint>(size.width, ScaleGUITrad(400));
		size.height = FLR_INFO_LINES * GetCharacterHeight(FontSize::Normal) + padding.height;
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_FLR_INFO) return;

		const Vehicle *v = Vehicle::GetIfValid(this->vehicle);
		if (v == nullptr) return;

		uint months;
		Money old_rent;
		const Money new_rent = GetForkLeaseRenewRent(this->vehicle.base(), months, old_rent);

		const Rect ir = r.Shrink(WidgetDimensions::scaled.frametext);
		const int line = GetCharacterHeight(FontSize::Normal);
		int y = ir.top;

		DrawString(ir.left, ir.right, y, GetString(STR_FORK_LEASE_RENEW_TEXT, PackEngineNameDParam(v->engine_type, EngineNameContext::Generic)));
		y += line;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_LEASE_RENEW_OFFER, months, new_rent));
		y += line;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_LEASE_RENEW_OLD, old_rent));
		y += line;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_LEASE_RENEW_HINT));
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_FLR_RENEW:
				Command<Commands::RenewForkLease>::Post(STR_ERROR_FORK_LEASE_CAN_T_RENEW, this->vehicle, true);
				this->Close();
				break;

			case WID_FLR_RETURN:
				Command<Commands::RenewForkLease>::Post(STR_ERROR_FORK_LEASE_CAN_T_RENEW, this->vehicle, false);
				this->Close();
				break;
		}
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_fork_lease_renew_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::Grey),
		NWidget(WWT_CAPTION, Colours::Grey, WID_FLR_CAPTION), SetStringTip(STR_FORK_LEASE_RENEW_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, Colours::Grey),
		NWidget(WWT_STICKYBOX, Colours::Grey),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::Grey, WID_FLR_INFO), SetResize(1, 1), SetFill(1, 1),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_FLR_RENEW), SetMinimalSize(100, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_LEASE_RENEW_BTN, STR_FORK_LEASE_RENEW_BTN_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Grey, WID_FLR_RETURN), SetMinimalSize(100, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_LEASE_RETURN_BTN, STR_FORK_LEASE_RETURN_BTN_TOOLTIP),
		NWidget(WWT_RESIZEBOX, Colours::Grey),
	EndContainer(),
};

static WindowDesc _fork_lease_renew_desc(
	WindowPosition::Center, "fork_lease_renew", 0, 0,
	WindowClass::ForkLeaseRenew, WindowClass::None,
	{},
	_nested_fork_lease_renew_widgets
);

/**
 * Ask whether the rental contract of a vehicle is renewed.
 * @param vehicle Index of the vehicle.
 */
void ShowForkLeaseRenewWindow(uint vehicle)
{
	AllocateWindowDescFront<ForkLeaseRenewWindow>(_fork_lease_renew_desc, vehicle);
}
