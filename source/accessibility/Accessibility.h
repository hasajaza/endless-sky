/* Accessibility.h
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

#include <cstdint>
#include <list>
#include <string>

class Engine;
class PlayerInfo;
class ShipEvent;
class UI;
union SDL_Event;



// The entry points through which the game talks to the accessibility layer.
// These are the only calls added to the game's own code paths; everything else
// lives in the "accessibility" folder.
class Accessibility {
public:
	static void Init(PlayerInfo &player, UI &menuPanels, UI &gamePanels);
	static void Quit();

	// Called for every SDL event before the game sees it. Returns true if the
	// event was an accessibility command (and so must not reach the game).
	static bool HandleEvent(const SDL_Event &event);
	// Called after the game has processed a key press.
	static void AfterKey(int32_t key, uint16_t mod);
	// Called once per frame after the panels have been stepped.
	static void Step(bool isFastForward);
	// Called from MainPanel::Step while the engine's calculation thread is
	// idle, so it is safe to read ships, asteroids and other objects.
	static void FlightStep(Engine &engine, const std::list<ShipEvent> &events, bool isActive);
	// Called (possibly from another thread) whenever the game shows a message.
	static void OnMessage(const std::string &text, const std::string &category);
};
