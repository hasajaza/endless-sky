/* AccessReader.h
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

#include "AccessSpeech.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class Panel;
class PlayerInfo;
class UI;



// One entry of the review cursor. The text is produced when the entry is
// spoken, so it is always current (distances, prices and so on).
class ReviewItem {
public:
	ReviewItem() = default;
	ReviewItem(std::string fixedText, std::function<bool()> activate = nullptr);
	ReviewItem(std::function<std::string()> text, std::function<bool()> activate = nullptr);

	std::string Text() const;

public:
	std::function<std::string()> text;
	// Extra detail read by the "review current" command.
	std::function<std::string()> detail;
	// Activation (Ctrl+Enter). Returns false if nothing could be done.
	std::function<bool()> activate;
};



class ReviewGroup {
public:
	std::string name;
	std::vector<ReviewItem> items;
};



// Base class for the accessible description of one panel type. A reader never
// changes game state by itself except through the same public operations the
// game's own keys and mouse clicks use.
class AccessReader {
public:
	explicit AccessReader(Panel &panel);
	virtual ~AccessReader() = default;

	// Spoken name of the screen.
	virtual std::string Title();
	// Called when the panel first becomes the active one.
	virtual void OnOpen();
	// Called when the panel becomes active again after something on top of it closed.
	virtual void OnReturn();
	// Called every frame while this panel is the active one.
	virtual void Step();
	// Called before the game handles a key. Return true to consume the key.
	virtual bool PreKey(int32_t key, uint16_t mod);
	// Called after the game handled a key.
	virtual void PostKey(int32_t key, uint16_t mod);
	// A short description of the current state, used on open and by the
	// context help command.
	virtual std::string Summary();
	// The keys that work on this screen (the game's own keys first).
	virtual std::vector<std::string> Keys();
	// Fill the review cursor. The default lists the screen's text and buttons.
	virtual void BuildReview(std::vector<ReviewGroup> &groups);
	// Whether the review list should be kept between key presses (true for
	// lists whose order is not stable, such as distance-sorted objects).
	virtual bool KeepReview() const;
	// True while the screen needs every key (for example while waiting for
	// a key to bind in the controls settings); unmodified accessibility keys
	// are then passed to the game.
	virtual bool WantsRawKeys();
	// Keys that this screen uses together with Ctrl (for example "buy 20 at
	// a time"). Ctrl plus these keys is left to the game.
	virtual std::vector<int32_t> CtrlKeysUsedByGame();

	Panel &GetPanel() const;
	// Set by the manager once OnOpen() has been called.
	bool announced = false;
	// Run callbacks scheduled with Later() (called by the manager every frame).
	void RunDeferred();


protected:
	// Helpers shared by the readers.
	static void Say(const std::string &text, SpeechPriority priority = SpeechPriority::FOCUS);
	static PlayerInfo &Player();
	// Send a key to the active UI exactly as if it had been pressed.
	static bool PressKey(int32_t key, uint16_t mod = 0);
	// Add the default screen text and button groups for the given panel.
	void AddScreenText(std::vector<ReviewGroup> &groups, bool includeButtons = true) const;
	// The interface buttons captured for this panel, as "Label, key X" strings.
	std::vector<std::string> ButtonKeys() const;
	// Run a callback after the given number of frames (for values that are
	// only updated when the screen is redrawn).
	void Later(int frames, std::function<void()> callback);


protected:
	Panel &panel;

private:
	std::vector<std::pair<int, std::function<void()>>> deferred;
};



// Create the reader for a panel (never returns nullptr).
std::unique_ptr<AccessReader> CreateAccessReader(Panel &panel);
