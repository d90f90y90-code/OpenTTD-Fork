/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_rights_gui.cpp Windows for buying rights and signing annual licenses in a town. */

#include "stdafx.h"

#include "command_func.h"
#include "company_base.h"
#include "company_func.h"
#include "fork_economy.h"
#include "fork_rights_cmd.h"
#include "gfx_func.h"
#include "palette_func.h"
#include "strings_func.h"
#include "town.h"
#include "window_func.h"
#include "window_gui.h"
#include "zoom_func.h"

#include "table/strings.h"

#include "safeguards.h"

/** Widgets of the rights window. */
enum ForkRightsWidgets : WidgetID {
	WID_FR_CAPTION, ///< Caption of the window.
	WID_FR_LIST, ///< List of the rights.
	WID_FR_SCROLLBAR, ///< Scrollbar of the list.
};

/** Window with the rights of one town. The window number is the town. */
struct ForkRightsWindow : Window {
	const Town *town; ///< Town of this window.
	Scrollbar *vscroll = nullptr; ///< Scrollbar of the list.
	std::vector<ForkRightCategory> categories; ///< The rights this town sells.
	int line = 0; ///< Height of a line of text.
	int btn_h = 0; ///< Height of a button.
	int sep = 0; ///< Space above and below the thin line between a right and its license.
	int block_h = 0; ///< Height of the part of the list for one right.
	int btn_w = 0; ///< Width of the buttons.

	ForkRightsWindow(WindowDesc &desc, WindowNumber number) : Window(desc), town(Town::Get(static_cast<TownID>(number)))
	{
		for (uint c = 0; c < static_cast<uint>(ForkRightCategory::End); c++) {
			if (IsForkRightAvailable(this->town, static_cast<ForkRightCategory>(c))) this->categories.push_back(static_cast<ForkRightCategory>(c));
		}

		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_FR_SCROLLBAR);
		this->FinishInitNested(number);
		this->vscroll->SetCount(this->categories.size());
	}

	/** Work out the sizes of the parts of the list. */
	void UpdateMetrics()
	{
		this->line = GetCharacterHeight(FontSize::Normal);
		this->btn_h = this->line + ScaleGUITrad(6);
		this->sep = ScaleGUITrad(5);
		this->block_h = this->line + this->btn_h + (2 * this->sep + 1) + this->btn_h + ScaleGUITrad(8);
		this->btn_w = std::max(GetStringBoundingBox(STR_FORK_RIGHT_BUY).width, GetStringBoundingBox(STR_FORK_RIGHT_LICENSE_BTN).width) + ScaleGUITrad(20);
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_FR_CAPTION) return GetString(STR_FORK_RIGHTS_CAPTION, this->town->index);
		return this->Window::GetWidgetString(widget, stringid);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget != WID_FR_LIST) return;

		this->UpdateMetrics();
		size.width = std::max<uint>(size.width, ScaleGUITrad(380));
		fill.height = resize.height = this->block_h;
		size.height = 3 * this->block_h;
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_FR_LIST, WidgetDimensions::scaled.framerect.Vertical());
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, bool gui_scope = true) override
	{
		if (!gui_scope) return;
		this->vscroll->SetCount(this->categories.size());
		this->SetDirty();
	}

	/** Draw a button with a text. */
	void DrawButton(const Rect &r, StringID text, bool enabled) const
	{
		DrawFrameRect(r, Colours::Grey, FrameFlags{});
		DrawString(r.left, r.right, r.top + (r.Height() - this->line) / 2, text, enabled ? TextColour::Black : TextColour::Grey, AlignmentH::Centre);
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_FR_LIST || !Company::IsValidID(_local_company)) return;

		const bool rtl = _current_text_dir == TD_RTL;
		const Rect ir = r.Shrink(WidgetDimensions::scaled.framerect);
		const int gap = ScaleGUITrad(8);
		const int text_left = rtl ? ir.left + this->btn_w + gap : ir.left;
		const int text_right = rtl ? ir.right : ir.right - this->btn_w - gap;
		const int btn_left = rtl ? ir.left : ir.right - this->btn_w;

		int y = ir.top;
		const size_t last = std::min<size_t>(this->categories.size(), this->vscroll->GetPosition() + this->vscroll->GetCapacity());
		for (size_t i = this->vscroll->GetPosition(); i < last; i++) {
			const ForkRightCategory cat = this->categories[i];
			const ForkRightStatus status = GetForkRightStatus(_local_company, this->town->index, cat);
			const bool can_buy = !status.owned;

			/* The name of the right, and what the company holds of it. */
			DrawString(ir.left, ir.right, y, GetString(STR_FORK_RIGHT_LINE_NAME, STR_FORK_RIGHT_NAME_MINING + static_cast<uint>(cat)));
			StringID state = status.owned ? STR_FORK_RIGHT_STATUS_OWNED : (status.license_months > 0 ? STR_FORK_RIGHT_STATUS_LICENSED : STR_FORK_RIGHT_STATUS_NONE);
			if (status.license_months > 0) {
				DrawString(ir.left, ir.right, y, GetString(state, status.license_months), TextColour::FromString, AlignmentH::End);
			} else {
				DrawString(ir.left, ir.right, y, state, TextColour::FromString, AlignmentH::End);
			}
			y += this->line;

			/* The price of the right, and the button to buy it. */
			const Rect buy{btn_left, y, btn_left + this->btn_w - 1, y + this->btn_h - 1};
			DrawString(text_left, text_right, y + (this->btn_h - this->line) / 2, GetString(STR_FORK_RIGHT_PRICE, GetForkRightPrice(this->town, cat)));
			this->DrawButton(buy, STR_FORK_RIGHT_BUY, can_buy);
			y += this->btn_h + this->sep;

			/* A thin line between the right and its license. */
			GfxFillRect(Rect{ir.left, y, ir.right, y}, GetColourGradient(Colours::Brown, Shade::Dark));
			y += this->sep + 1;

			/* The rent of the license, and the button to sign one. */
			const Rect lic{btn_left, y, btn_left + this->btn_w - 1, y + this->btn_h - 1};
			DrawString(text_left, text_right, y + (this->btn_h - this->line) / 2, GetString(STR_FORK_RIGHT_LICENSE_LINE, GetForkLicenseMonthlyRent(this->town, cat)));
			this->DrawButton(lic, STR_FORK_RIGHT_LICENSE_BTN, can_buy);
			y += this->btn_h + ScaleGUITrad(8);
		}
	}

	void OnClick(Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		if (widget != WID_FR_LIST || !Company::IsValidID(_local_company)) return;

		const NWidgetBase *nwid = this->GetWidget<NWidgetBase>(WID_FR_LIST);
		const WidgetDimensions &wd = WidgetDimensions::scaled;
		const int rel_y = pt.y - nwid->pos_y - wd.framerect.top;
		if (rel_y < 0) return;

		const size_t index = this->vscroll->GetPosition() + rel_y / this->block_h;
		if (index >= this->categories.size()) return;

		/* Only the buttons react. */
		const bool rtl = _current_text_dir == TD_RTL;
		const int rel_x = pt.x - nwid->pos_x;
		const bool on_button = rtl ? rel_x <= wd.framerect.left + this->btn_w : rel_x >= static_cast<int>(nwid->current_x) - wd.framerect.right - this->btn_w;
		if (!on_button) return;

		const ForkRightCategory cat = this->categories[index];
		if (GetForkRightStatus(_local_company, this->town->index, cat).owned) return;

		const int local = rel_y % this->block_h;
		const int row_buy = this->line;
		const int row_license = this->line + this->btn_h + 2 * this->sep + 1;
		if (local >= row_buy && local < row_buy + this->btn_h) {
			Command<Commands::BuyForkRight>::Post(STR_ERROR_FORK_CAN_T_BUY_RIGHT, this->town->index, static_cast<uint8_t>(cat));
		} else if (local >= row_license && local < row_license + this->btn_h) {
			ShowForkLicenseWindow(this->town->index.base(), static_cast<uint>(cat));
		}
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_fork_rights_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::Brown),
		NWidget(WWT_CAPTION, Colours::Brown, WID_FR_CAPTION), SetToolTip(STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, Colours::Brown),
		NWidget(WWT_STICKYBOX, Colours::Brown),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, Colours::Brown, WID_FR_LIST), SetScrollbar(WID_FR_SCROLLBAR), SetResize(1, 1), SetFill(1, 1), SetMinimalSize(300, 60),
		EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_VSCROLLBAR, Colours::Brown, WID_FR_SCROLLBAR),
			NWidget(WWT_RESIZEBOX, Colours::Brown),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _fork_rights_desc(
	WindowPosition::Automatic, "fork_rights", 0, 0,
	WindowClass::ForkRights, WindowClass::None,
	{},
	_nested_fork_rights_widgets
);

/**
 * Show the rights window of a town.
 * @param town Index of the town.
 */
void ShowForkRightsWindow(uint town)
{
	AllocateWindowDescFront<ForkRightsWindow>(_fork_rights_desc, town);
}

/** Widgets of the license window. */
enum ForkLicenseWidgets : WidgetID {
	WID_FLC_CAPTION, ///< Caption of the window.
	WID_FLC_INFO, ///< Panel with the texts.
	WID_FLC_YEAR_DOWN, ///< One year less.
	WID_FLC_YEAR_UP, ///< One year more.
	WID_FLC_SIGN, ///< Sign the agreement.
};

static constexpr uint FLC_INFO_LINES = 5; ///< Number of text lines in the information panel.

/** Window with the terms of a license, where the duration is chosen before signing. The window number is the town times 16 plus the category. */
struct ForkLicenseWindow : Window {
	const Town *town; ///< Town of the license.
	ForkRightCategory category; ///< Category of the license.
	uint years = 1; ///< Duration in years that is offered.

	ForkLicenseWindow(WindowDesc &desc, WindowNumber number) : Window(desc), town(Town::Get(static_cast<TownID>(number / 16))), category(static_cast<ForkRightCategory>(number % 16))
	{
		this->InitNested(number);
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_FLC_CAPTION) return GetString(STR_FORK_LICENSE_CAPTION, this->town->index);
		return this->Window::GetWidgetString(widget, stringid);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget != WID_FLC_INFO) return;

		size.width = std::max<uint>(size.width, ScaleGUITrad(360));
		size.height = FLC_INFO_LINES * GetCharacterHeight(FontSize::Normal) + padding.height;
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_FLC_INFO) return;

		const Money rent = GetForkLicenseMonthlyRent(this->town, this->category);
		const Rect ir = r.Shrink(WidgetDimensions::scaled.frametext);
		const int line = GetCharacterHeight(FontSize::Normal);
		int y = ir.top;

		DrawString(ir.left, ir.right, y, GetString(STR_FORK_LICENSE_TYPE, STR_FORK_RIGHT_NAME_MINING + static_cast<uint>(this->category)));
		y += line;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_LICENSE_RENT, rent));
		y += line;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_LICENSE_DURATION, this->years));
		y += line;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_LICENSE_TOTAL, rent * (this->years * 12)));
		y += line;
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_LICENSE_NOTE));
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_FLC_YEAR_DOWN:
				if (this->years > 1) this->years--;
				this->SetDirty();
				break;

			case WID_FLC_YEAR_UP:
				if (this->years < 3) this->years++;
				this->SetDirty();
				break;

			case WID_FLC_SIGN:
				Command<Commands::SignForkLicense>::Post(STR_ERROR_FORK_CAN_T_SIGN_LICENSE, this->town->index, static_cast<uint8_t>(this->category), static_cast<uint8_t>(this->years));
				this->Close();
				break;
		}
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_fork_license_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::Brown),
		NWidget(WWT_CAPTION, Colours::Brown, WID_FLC_CAPTION), SetToolTip(STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, Colours::Brown),
		NWidget(WWT_STICKYBOX, Colours::Brown),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::Brown, WID_FLC_INFO), SetResize(1, 1), SetFill(1, 1),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FLC_YEAR_DOWN), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_LICENSE_YEAR_DOWN, STR_FORK_LICENSE_YEAR_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FLC_YEAR_UP), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_LICENSE_YEAR_UP, STR_FORK_LICENSE_YEAR_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FLC_SIGN), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_LICENSE_SIGN, STR_FORK_LICENSE_SIGN_TOOLTIP),
		NWidget(WWT_RESIZEBOX, Colours::Brown),
	EndContainer(),
};

static WindowDesc _fork_license_desc(
	WindowPosition::Automatic, "fork_license", 0, 0,
	WindowClass::ForkLicense, WindowClass::None,
	{},
	_nested_fork_license_widgets
);

/**
 * Show the terms of a license.
 * @param town Index of the town.
 * @param category Category of the license.
 */
void ShowForkLicenseWindow(uint town, uint category)
{
	AllocateWindowDescFront<ForkLicenseWindow>(_fork_license_desc, town * 16 + category);
}
