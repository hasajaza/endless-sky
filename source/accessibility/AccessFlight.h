/* AccessFlight.h
Copyright (c) 2026 by the Endless Sky Accessibility contributors

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include "AccessReader.h"

#include <list>
#include <string>
#include <vector>

class Engine;
class Mission;
class Ship;
class ShipEvent;



// Spaceflight: event-driven announcements (targets, damage, threats), the
// information commands, the "radar" review lists and the audio beacon. All
// functions that read ships must run while the engine's calculation thread is
// idle (inside Accessibility::FlightStep, or after Engine::Wait()).
class AccessFlight {
public:
	static void Step(Engine &engine, const std::list<ShipEvent> &events, bool isActive);

	// Information commands.
	static std::string Location();
	static std::string Status();
	static std::string Target();
	static std::string Destination();
	static std::string Nearby();
	static std::string Hostiles();
	static std::string Fleet();
	static std::string Cargo();
	static std::string Weapons();
	static std::string Motion();
	static std::string ToggleBeacon();
	static std::string MissionSummary(const Mission &mission, bool accepted = true);

	// Review cursor groups for flight (planets, ships, asteroids, links).
	static void BuildReview(std::vector<ReviewGroup> &groups);

	// Spoken description of a ship as the player can see it.
	static std::string DescribeShip(const Ship &ship, bool brief);
};
