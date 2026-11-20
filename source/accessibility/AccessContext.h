/* AccessContext.h
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

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

class Engine;
class MainPanel;
class Panel;
class PlayerInfo;
class UI;



// Shared state of the accessibility layer, for use by the readers.
class AccessContext {
public:
	static PlayerInfo &Player();
	static UI &ActiveUI();
	static UI &MenuUI();
	static UI &GameUI();
	// The flight panel, if a game is running.
	static MainPanel *Main();
	// The engine, if a game is running. Only read it from the main thread
	// after Engine::Wait() or inside Accessibility::FlightStep().
	static Engine *GetEngine();
	// True when the player is flying (no other panel is open).
	static bool InFlight();
	// The panel that currently receives keys.
	static Panel *TopPanel();

	// Deliver a key press to the active UI as if typed by the player.
	static bool PressKey(int32_t key, uint16_t mod = 0);

	// Replace the review cursor contents with a custom list (for example the
	// key reference). It is discarded when the screen changes.
	static void LoadReview(const std::string &title, std::vector<ReviewGroup> groups);

	// Audio cue settings.
	static bool AudioCues();
	static void SetAudioCues(bool on);

	// Recent game messages, newest first.
	static std::vector<std::string> RecentMessages();

	// Persist the accessibility settings.
	static void SaveSettings();
};
