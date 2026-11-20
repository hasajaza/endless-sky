/* AccessKeys.h
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
#include <filesystem>
#include <string>
#include <vector>



// The extra keyboard commands added by the accessibility layer. The game's own
// commands (class Command) are never replaced; these only cover functions the
// original controls do not provide. All of them use a modifier or a function
// key that the game does not bind by default, and each can be rebound in the
// "accessibility keys.txt" file in the configuration folder.
enum class AccessAction : int {
	NONE = 0,
	// Speech and help.
	CONTEXT_HELP,
	KEY_REFERENCE,
	REPEAT,
	HISTORY_BACK,
	HISTORY_FORWARD,
	CYCLE_ENGINE,
	VERBOSITY,
	TOGGLE_CUES,
	// Information commands.
	LOCATION,
	STATUS,
	TARGET,
	DESTINATION,
	NEARBY,
	HOSTILES,
	FLEET,
	MISSIONS,
	CARGO,
	MESSAGES,
	WEAPONS,
	MOTION,
	BEACON,
	// Review cursor.
	REVIEW_PREVIOUS,
	REVIEW_NEXT,
	REVIEW_FIRST,
	REVIEW_LAST,
	REVIEW_PREVIOUS_GROUP,
	REVIEW_NEXT_GROUP,
	REVIEW_ACTIVATE,
	REVIEW_CURRENT,
	COUNT
};



class AccessKeys {
public:
	static constexpr uint16_t CTRL = 1;
	static constexpr uint16_t SHIFT = 2;
	static constexpr uint16_t ALT = 4;

	struct Binding {
		int32_t keycode = 0;
		uint16_t modifiers = 0;
	};

	struct Info {
		AccessAction action;
		const char *id;
		const char *description;
		Binding defaultBinding;
		// A second key for the same command (function keys that screen
		// readers do not use, for the review cursor).
		Binding defaultAlternate = {};
	};


public:
	// Set up the default bindings and load user overrides.
	static void Load(const std::filesystem::path &file);
	// Write the current bindings (so the player has a template to edit).
	static void Save(const std::filesystem::path &file);

	// Find the action bound to the given key and SDL modifier state.
	static AccessAction Match(int32_t keycode, uint16_t sdlModifiers);
	// Thread-safe check used by the flight keyboard polling: is this key
	// currently part of a held accessibility chord, so it must not also act
	// as a flight control?
	static bool IsReservedNow(int32_t keycode);
	// Keys pressed as part of an accessibility chord stay reserved until they
	// are released, even if the modifier is released first.
	static void ChordPressed(int32_t keycode);
	static void KeyReleased(int32_t keycode);

	static const std::vector<Info> &All();
	static Binding Get(AccessAction action);
	static Binding GetAlternate(AccessAction action);
	static std::string KeyName(AccessAction action);
	static std::string Description(AccessAction action);
	// Report the accessibility bindings that clash with a game binding.
	static std::vector<std::string> Conflicts();

	// Convert an SDL modifier mask into this class's modifier bits.
	static uint16_t FromSDL(uint16_t sdlModifiers);
	static std::string BindingName(const Binding &binding);
};
