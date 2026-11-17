/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fork_logos.h Logos of the fork's companies, drawn without being sprites. */

#ifndef FORK_LOGOS_H
#define FORK_LOGOS_H

#include "gfx_type.h"

/** The logos that are built into the game. */
enum class ForkLogo : uint8_t {
	CarMarket, ///< Sign of the used vehicle market.
	TtdMax, ///< Dealer of used trains.
	Tajcar, ///< Dealer of used road vehicles.
	Limano, ///< Dealer of used vehicles of every kind.
	Seamar, ///< Dealer of used ships.
	Airix, ///< Dealer of used aircraft.
	Buildings, ///< First of the 24 building images: 12 headquarters, then 12 laboratories, each with 3 stages of construction.
	End = Buildings + 24, ///< End marker.
};

void DrawForkLogo(ForkLogo logo, const Rect &r);
ForkLogo GetForkBuildingLogo(bool laboratory, uint design, uint stage);

#endif /* FORK_LOGOS_H */
