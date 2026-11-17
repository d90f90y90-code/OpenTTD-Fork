/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_media_gui.cpp Windows of the television and radio channels. */

#include "stdafx.h"

#include "command_func.h"
#include "company_base.h"
#include "company_func.h"
#include "fork_economy.h"
#include "fork_logos.h"
#include "fork_media.h"
#include "fork_media_cmd.h"
#include "gfx_func.h"
#include "strings_func.h"
#include "textbuf_gui.h"
#include "town.h"
#include "window_func.h"
#include "window_gui.h"
#include "zoom_func.h"

#include "table/strings.h"

#include "safeguards.h"

/** @return The picture of the building of a kind of channel at a construction stage. */
static ForkLogo GetBuildingLogo(uint kind, uint stage)
{
	return static_cast<ForkLogo>(static_cast<uint>(ForkLogo::ChannelBuildings) + std::min<uint>(kind, FORK_CHANNEL_KINDS - 1) * 3 + std::min<uint>(stage, 2));
}

/*
 * ----------------------------------------------------------------------------------------------
 * Window with the four kinds of channel of a town.
 * ----------------------------------------------------------------------------------------------
 */

/** Widgets of the window with the channels of a town. */
enum ForkMediaWidgets : WidgetID {
	WID_FM_CAPTION, ///< Caption of the window.
	WID_FM_LIST, ///< Panel with the four kinds of channel.
};

/** Window that lists the four channels of a town; the window number is the town. */
struct ForkMediaWindow : Window {
	const Town *town; ///< Town of this window.

	ForkMediaWindow(WindowDesc &desc, WindowNumber number) : Window(desc), town(Town::Get(static_cast<TownID>(number)))
	{
		this->InitNested(number);
	}

	int GetRowHeight() const
	{
		return std::max<int>(ScaleGUITrad(64), 3 * GetCharacterHeight(FontSize::Normal));
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_FM_CAPTION) return GetString(STR_FORK_MEDIA_CAPTION, this->town->index);
		return this->Window::GetWidgetString(widget, stringid);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget != WID_FM_LIST) return;

		size.width = std::max<uint>(size.width, ScaleGUITrad(400));
		size.height = 3 * GetCharacterHeight(FontSize::Normal) + FORK_CHANNEL_KINDS * this->GetRowHeight() + padding.height;
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, bool gui_scope = true) override
	{
		if (gui_scope) this->SetDirty();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_FM_LIST) return;

		const Rect ir = r.Shrink(WidgetDimensions::scaled.frametext);
		const int line = GetCharacterHeight(FontSize::Normal);
		int y = ir.top;

		if (!IsForkMediaTown(this->town)) {
			DrawString(ir.left, ir.right, y, GetString(STR_FORK_MEDIA_TOO_SMALL, FORK_MEDIA_MIN_POPULATION, this->town->cache.population));
			return;
		}
		DrawString(ir.left, ir.right, y, GetString(STR_FORK_MEDIA_HINT));
		y += 3 * line;

		const int row_h = this->GetRowHeight();
		const int image = row_h - 4;
		for (uint kind = 0; kind < FORK_CHANNEL_KINDS; kind++) {
			const int top = y + static_cast<int>(kind) * row_h;
			const ForkAsset *a = FindForkChannel(this->town->index, kind);
			DrawForkLogo(GetBuildingLogo(kind, a == nullptr ? 2 : GetForkChannelStage(a)), Rect{ir.left, top + 2, ir.left + image - 1, top + 2 + image - 1});

			const int text_left = ir.left + image + WidgetDimensions::scaled.hsep_normal;
			DrawString(text_left, ir.right, top + 2, GetString(STR_FORK_MEDIA_KIND_0 + kind));

			if (a == nullptr) {
				DrawString(text_left, ir.right, top + 2 + line, GetString(STR_FORK_MEDIA_ROW_AVAILABLE, GetForkChannelPrice(kind)));
			} else if (a->owner != _local_company) {
				DrawString(text_left, ir.right, top + 2 + line, GetString(STR_FORK_MEDIA_ROW_OTHER, a->name, a->owner));
			} else if (!IsForkChannelOperating(a)) {
				DrawString(text_left, ir.right, top + 2 + line, GetString(STR_FORK_MEDIA_ROW_BUILDING, a->name, a->age_months, FORK_CHANNEL_BUILD_MONTHS));
			} else {
				DrawString(text_left, ir.right, top + 2 + line, GetString(STR_FORK_MEDIA_ROW_ON_AIR, a->name, GetForkChannelStats(a).audience));
			}
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		if (widget != WID_FM_LIST || !IsForkMediaTown(this->town) || !Company::IsValidID(_local_company)) return;

		const NWidgetBase *nwi = this->GetWidget<NWidgetBase>(WID_FM_LIST);
		const int y = pt.y - static_cast<int>(nwi->pos_y) - WidgetDimensions::scaled.frametext.top - 3 * GetCharacterHeight(FontSize::Normal);
		if (y < 0) return;

		const uint kind = static_cast<uint>(y / this->GetRowHeight());
		if (kind >= FORK_CHANNEL_KINDS) return;

		const ForkAsset *a = FindForkChannel(this->town->index, kind);
		if (a == nullptr) {
			ShowForkChannelCreateWindow(this->town->index.base(), kind);
		} else if (a->owner == _local_company) {
			ShowForkChannelWindow(a->index.base());
		}
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_fork_media_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::Brown),
		NWidget(WWT_CAPTION, Colours::Brown, WID_FM_CAPTION), SetToolTip(STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, Colours::Brown),
		NWidget(WWT_STICKYBOX, Colours::Brown),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::Brown, WID_FM_LIST), SetResize(1, 1), SetFill(1, 1),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, Colours::Brown), SetFill(1, 0), SetResize(1, 0),
		EndContainer(),
		NWidget(WWT_RESIZEBOX, Colours::Brown),
	EndContainer(),
};

static WindowDesc _fork_media_desc(
	WindowPosition::Automatic, {}, 0, 0,
	WindowClass::ForkMedia, WindowClass::None,
	{},
	_nested_fork_media_widgets
);

/**
 * Show the window with the channels of a town.
 * @param town Index of the town.
 */
void ShowForkMediaWindow(uint town)
{
	AllocateWindowDescFront<ForkMediaWindow>(_fork_media_desc, town);
}

/*
 * ----------------------------------------------------------------------------------------------
 * Window to set up a new channel.
 * ----------------------------------------------------------------------------------------------
 */

/** Widgets of the window to build a channel. */
enum ForkChannelCreateWidgets : WidgetID {
	WID_FCC_CAPTION, ///< Caption of the window.
	WID_FCC_INFO, ///< Panel with the picture and the texts.
	WID_FCC_NAME, ///< Change the name.
	WID_FCC_FREE, ///< The channel is free.
	WID_FCC_PAID, ///< The channel has a paid package.
	WID_FCC_PACKAGE_DOWN, ///< Lower the price of the package.
	WID_FCC_PACKAGE_UP, ///< Raise the price of the package.
	WID_FCC_CONFIRM, ///< Build the channel.
	WID_FCC_CANCEL, ///< Close the window.
};

/** Window to choose the name and the access of a new channel; the window number is the town times four plus the kind. */
struct ForkChannelCreateWindow : Window {
	const Town *town; ///< Town to build in.
	uint kind; ///< Kind of channel.
	std::string name; ///< Name that was chosen.
	bool paid = false; ///< Whether the channel has a paid package.
	uint package = 100; ///< Price of the package.

	ForkChannelCreateWindow(WindowDesc &desc, WindowNumber number) : Window(desc), town(Town::Get(static_cast<TownID>(number / FORK_CHANNEL_KINDS))), kind(number % FORK_CHANNEL_KINDS)
	{
		this->name = GetString(STR_FORK_MEDIA_DEFAULT_NAME_0 + this->kind, this->town->index);
		this->InitNested(number);
		this->UpdateButtons();
	}

	void UpdateButtons()
	{
		this->SetWidgetLoweredState(WID_FCC_FREE, !this->paid);
		this->SetWidgetLoweredState(WID_FCC_PAID, this->paid);
		this->SetWidgetDisabledState(WID_FCC_PACKAGE_DOWN, !this->paid);
		this->SetWidgetDisabledState(WID_FCC_PACKAGE_UP, !this->paid);
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_FCC_CAPTION) return GetString(STR_FORK_CREATE_CAPTION, STR_FORK_MEDIA_KIND_0 + this->kind);
		return this->Window::GetWidgetString(widget, stringid);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget != WID_FCC_INFO) return;

		size.width = std::max<uint>(size.width, ScaleGUITrad(420));
		size.height = std::max<uint>(ScaleGUITrad(124), 6 * GetCharacterHeight(FontSize::Normal)) + padding.height;
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_FCC_INFO) return;

		const Rect ir = r.Shrink(WidgetDimensions::scaled.frametext);
		const int image = std::max<int>(ScaleGUITrad(112), ir.Height());
		DrawForkLogo(GetBuildingLogo(this->kind, 2), Rect{ir.left, ir.top, ir.left + image - 1, ir.top + image - 1});

		const int left = ir.left + image + WidgetDimensions::scaled.hsep_normal;
		const int line = GetCharacterHeight(FontSize::Normal);
		int y = ir.top;
		DrawString(left, ir.right, y, GetString(STR_FORK_CREATE_COST, GetForkChannelPrice(this->kind), FORK_CHANNEL_BUILD_MONTHS));
		y += line;
		DrawString(left, ir.right, y, GetString(STR_FORK_CREATE_NAME, this->name));
		y += 2 * line;
		if (this->paid) {
			y = DrawStringMultiLine(left, ir.right, y, ir.bottom, GetString(STR_FORK_CREATE_PAID_INFO, this->package));
		} else {
			y = DrawStringMultiLine(left, ir.right, y, ir.bottom, GetString(STR_FORK_CREATE_FREE_INFO));
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_FCC_NAME:
				ShowQueryString(this->name, STR_FORK_CREATE_NAME_CAPTION, 31, this, CS_ALPHANUMERAL, {});
				break;

			case WID_FCC_FREE:
				this->paid = false;
				break;

			case WID_FCC_PAID:
				this->paid = true;
				break;

			case WID_FCC_PACKAGE_DOWN:
				this->package = this->package <= 10 ? 1 : ((this->package - 1) / 10) * 10;
				break;

			case WID_FCC_PACKAGE_UP:
				this->package = std::min<uint>(FORK_CHANNEL_PACKAGE_MAX, (this->package / 10 + 1) * 10);
				break;

			case WID_FCC_CONFIRM:
				Command<Commands::BuildForkChannel>::Post(STR_ERROR_FORK_MEDIA_CAN_T_BUILD, this->town->index, static_cast<uint8_t>(this->kind), static_cast<uint16_t>(this->paid ? this->package : 0), this->name);
				this->Close();
				return;

			case WID_FCC_CANCEL:
				this->Close();
				return;
		}
		this->UpdateButtons();
		this->SetDirty();
	}

	void OnQueryTextFinished(std::optional<std::string> str) override
	{
		if (!str.has_value() || str->empty()) return;
		this->name = *str;
		this->SetDirty();
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_fork_channel_create_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::Brown),
		NWidget(WWT_CAPTION, Colours::Brown, WID_FCC_CAPTION), SetToolTip(STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, Colours::Brown),
		NWidget(WWT_STICKYBOX, Colours::Brown),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::Brown, WID_FCC_INFO), SetResize(1, 1), SetFill(1, 1),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_TEXTBTN, Colours::Brown, WID_FCC_FREE), SetMinimalSize(80, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_CREATE_FREE, STR_FORK_CREATE_FREE_TOOLTIP),
		NWidget(WWT_TEXTBTN, Colours::Brown, WID_FCC_PAID), SetMinimalSize(80, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_CREATE_PAID, STR_FORK_CREATE_PAID_TOOLTIP),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FCC_PACKAGE_DOWN), SetMinimalSize(80, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_CREATE_PACKAGE_DOWN, STR_FORK_CREATE_PACKAGE_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FCC_PACKAGE_UP), SetMinimalSize(80, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_CREATE_PACKAGE_UP, STR_FORK_CREATE_PACKAGE_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FCC_NAME), SetMinimalSize(80, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_CREATE_NAME_BUTTON, STR_FORK_CREATE_NAME_TOOLTIP),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FCC_CONFIRM), SetMinimalSize(80, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_CREATE_CONFIRM, STR_FORK_CREATE_CONFIRM_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FCC_CANCEL), SetMinimalSize(80, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_CREATE_CANCEL, STR_NULL),
		NWidget(WWT_RESIZEBOX, Colours::Brown),
	EndContainer(),
};

static WindowDesc _fork_channel_create_desc(
	WindowPosition::Automatic, {}, 0, 0,
	WindowClass::ForkChannelCreate, WindowClass::ForkMedia,
	{},
	_nested_fork_channel_create_widgets
);

/**
 * Show the window to set up a new channel.
 * @param town Index of the town.
 * @param kind Kind of channel.
 */
void ShowForkChannelCreateWindow(uint town, uint kind)
{
	AllocateWindowDescFront<ForkChannelCreateWindow>(_fork_channel_create_desc, town * FORK_CHANNEL_KINDS + kind);
}

/*
 * ----------------------------------------------------------------------------------------------
 * Window of a channel of the local company.
 * ----------------------------------------------------------------------------------------------
 */

/** Widgets of the window of a channel. */
enum ForkChannelWidgets : WidgetID {
	WID_FCH_CAPTION, ///< Caption of the window.
	WID_FCH_INFO, ///< Panel with the picture and the numbers.
	WID_FCH_FREE, ///< Make the channel free.
	WID_FCH_PAID, ///< Give the channel a paid package.
	WID_FCH_PACKAGE_DOWN, ///< Lower the price of the package.
	WID_FCH_PACKAGE_UP, ///< Raise the price of the package.
	WID_FCH_PROGRAMS, ///< Make programs (not yet).
	WID_FCH_SCHEDULE, ///< Schedule of the day (not yet).
	WID_FCH_PROMOTION, ///< Offers of advertisers (not yet).
};

static constexpr uint FCH_INFO_LINES = 9; ///< Number of text lines next to the picture.

/** Window of a channel; the window number is the number of the asset. */
struct ForkChannelWindow : Window {
	ForkAssetID asset; ///< The channel.

	ForkChannelWindow(WindowDesc &desc, WindowNumber number) : Window(desc), asset(static_cast<ForkAssetID>(number))
	{
		this->InitNested(number);
		this->SetWidgetDisabledState(WID_FCH_PROGRAMS, true);
		this->SetWidgetDisabledState(WID_FCH_SCHEDULE, true);
		this->SetWidgetDisabledState(WID_FCH_PROMOTION, true);
	}

	const ForkAsset *GetChannel() const
	{
		const ForkAsset *a = ForkAsset::GetIfValid(this->asset);
		if (a == nullptr || a->type != static_cast<uint8_t>(ForkAssetType::MediaChannel)) return nullptr;
		return a;
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_FCH_CAPTION) {
			const ForkAsset *a = this->GetChannel();
			return GetString(STR_FORK_CHANNEL_CAPTION, a == nullptr ? std::string{} : a->name);
		}
		return this->Window::GetWidgetString(widget, stringid);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget != WID_FCH_INFO) return;

		size.width = std::max<uint>(size.width, ScaleGUITrad(440));
		size.height = std::max<uint>(ScaleGUITrad(124), FCH_INFO_LINES * GetCharacterHeight(FontSize::Normal)) + padding.height;
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, bool gui_scope = true) override
	{
		if (!gui_scope) return;
		if (this->GetChannel() == nullptr) {
			this->Close();
			return;
		}
		this->SetDirty();
	}

	void OnPaint() override
	{
		const ForkAsset *a = this->GetChannel();
		const bool paid = a != nullptr && a->data[1] > 0;
		this->SetWidgetLoweredState(WID_FCH_FREE, !paid);
		this->SetWidgetLoweredState(WID_FCH_PAID, paid);
		this->SetWidgetDisabledState(WID_FCH_PACKAGE_DOWN, !paid);
		this->SetWidgetDisabledState(WID_FCH_PACKAGE_UP, !paid);
		this->DrawWidgets();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_FCH_INFO) return;
		const ForkAsset *a = this->GetChannel();
		if (a == nullptr) return;

		const uint kind = static_cast<uint>(std::clamp<int64_t>(a->data[0], 0, FORK_CHANNEL_KINDS - 1));
		const ForkChannelStats s = GetForkChannelStats(a);

		const Rect ir = r.Shrink(WidgetDimensions::scaled.frametext);
		const int image = std::max<int>(ScaleGUITrad(112), 0);
		DrawForkLogo(GetBuildingLogo(kind, GetForkChannelStage(a)), Rect{ir.left, ir.top, ir.left + image - 1, ir.top + image - 1});

		const int left = ir.left + image + WidgetDimensions::scaled.hsep_normal;
		const int line = GetCharacterHeight(FontSize::Normal);
		int y = ir.top;

		const Town *t = Town::GetIfValid(static_cast<TownID>(a->location));
		if (t != nullptr) {
			DrawString(left, ir.right, y, GetString(STR_FORK_CHANNEL_TOWN, STR_FORK_MEDIA_KIND_0 + kind, t->index));
			y += line;
		}

		if (!IsForkChannelOperating(a)) {
			DrawString(left, ir.right, y, GetString(STR_FORK_CHANNEL_STATUS_BUILDING, a->age_months, FORK_CHANNEL_BUILD_MONTHS));
			y += line;
			DrawString(left, ir.right, y, GetString(STR_FORK_CHANNEL_VALUE, a->value));
			return;
		}

		DrawString(left, ir.right, y, GetString(STR_FORK_CHANNEL_STATUS_ON_AIR));
		y += line;
		DrawString(left, ir.right, y, GetString(STR_FORK_CHANNEL_AUDIENCE, s.audience, static_cast<uint>(a->data[2] / 10), static_cast<uint>(a->data[3] / 10)));
		y += line;
		if (a->data[1] == 0) {
			DrawString(left, ir.right, y, GetString(STR_FORK_CHANNEL_ACCESS_FREE));
		} else {
			DrawString(left, ir.right, y, GetString(STR_FORK_CHANNEL_ACCESS_PAID, static_cast<uint>(a->data[1]), s.subscribers));
		}
		y += line;
		DrawString(left, ir.right, y, GetString(STR_FORK_CHANNEL_STAFF, s.efficiency, FORK_CHANNEL_STAFF));
		y += line;
		DrawString(left, ir.right, y, GetString(STR_FORK_CHANNEL_INCOME, s.subscription_income, s.ad_income));
		y += line;
		DrawString(left, ir.right, y, GetString(STR_FORK_CHANNEL_COSTS, s.running_cost, s.net));
		y += line;
		DrawString(left, ir.right, y, GetString(STR_FORK_CHANNEL_VALUE, a->value));
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		const ForkAsset *a = this->GetChannel();
		if (a == nullptr) return;

		uint package = static_cast<uint>(a->data[1]);
		switch (widget) {
			case WID_FCH_FREE:
				package = 0;
				break;

			case WID_FCH_PAID:
				if (package == 0) package = 100;
				break;

			case WID_FCH_PACKAGE_DOWN:
				if (package == 0) return;
				package = package <= 10 ? 1 : ((package - 1) / 10) * 10;
				break;

			case WID_FCH_PACKAGE_UP:
				if (package == 0) return;
				package = std::min<uint>(FORK_CHANNEL_PACKAGE_MAX, (package / 10 + 1) * 10);
				break;

			default:
				return;
		}
		Command<Commands::SetForkChannelPackage>::Post(STR_ERROR_FORK_MEDIA_CAN_T_CHANGE, this->asset, static_cast<uint16_t>(package));
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_fork_channel_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::Brown),
		NWidget(WWT_CAPTION, Colours::Brown, WID_FCH_CAPTION), SetToolTip(STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, Colours::Brown),
		NWidget(WWT_STICKYBOX, Colours::Brown),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::Brown, WID_FCH_INFO), SetResize(1, 1), SetFill(1, 1),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_TEXTBTN, Colours::Brown, WID_FCH_FREE), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_CREATE_FREE, STR_FORK_CREATE_FREE_TOOLTIP),
		NWidget(WWT_TEXTBTN, Colours::Brown, WID_FCH_PAID), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_CREATE_PAID, STR_FORK_CREATE_PAID_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FCH_PACKAGE_DOWN), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_CREATE_PACKAGE_DOWN, STR_FORK_CREATE_PACKAGE_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FCH_PACKAGE_UP), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_CREATE_PACKAGE_UP, STR_FORK_CREATE_PACKAGE_TOOLTIP),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FCH_PROGRAMS), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_CHANNEL_PROGRAMS, STR_FORK_CHANNEL_SOON_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FCH_SCHEDULE), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_CHANNEL_SCHEDULE, STR_FORK_CHANNEL_SOON_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_FCH_PROMOTION), SetMinimalSize(70, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_FORK_CHANNEL_PROMOTION, STR_FORK_CHANNEL_SOON_TOOLTIP),
		NWidget(WWT_RESIZEBOX, Colours::Brown),
	EndContainer(),
};

static WindowDesc _fork_channel_desc(
	WindowPosition::Automatic, {}, 0, 0,
	WindowClass::ForkChannel, WindowClass::ForkMedia,
	{},
	_nested_fork_channel_widgets
);

/**
 * Show the window of a channel.
 * @param asset Index of the channel in the pool of assets.
 */
void ShowForkChannelWindow(uint asset)
{
	AllocateWindowDescFront<ForkChannelWindow>(_fork_channel_desc, asset);
}
