/* AccessKeys.cpp
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

#include "AccessKeys.h"

#include "../Command.h"
#include "../DataFile.h"
#include "../DataNode.h"
#include "../DataWriter.h"
#include "../Files.h"
#include "../SDL.h"

#include <map>
#include <mutex>
#include <set>

using namespace std;

namespace {
	using Binding = AccessKeys::Binding;
	constexpr uint16_t C = AccessKeys::CTRL;
	constexpr uint16_t S = AccessKeys::SHIFT;

	const vector<AccessKeys::Info> INFO = {
		{AccessAction::CONTEXT_HELP, "context help",
			"Describe this screen; press again to hear its keys one at a time", {SDLK_F2, 0}},
		{AccessAction::CONTEXT_HELP_PREVIOUS, "context help previous",
			"Go back to the previous key of this screen", {SDLK_F2, S}},
		{AccessAction::KEY_REFERENCE, "key reference",
			"Load the full accessibility key reference into the review cursor", {SDLK_F2, C}},
		{AccessAction::REPEAT, "repeat", "Repeat the last announcement", {'r', C}},
		{AccessAction::HISTORY_BACK, "history back", "Speech history: previous announcement", {SDLK_PAGEUP, C}},
		{AccessAction::HISTORY_FORWARD, "history forward", "Speech history: next announcement", {SDLK_PAGEDOWN, C}},
		{AccessAction::CYCLE_ENGINE, "speech engine", "Switch speech output (automatic, NVDA, JAWS, SAPI...)",
			{SDLK_F12, C}},
		{AccessAction::VERBOSITY, "verbosity", "Change automatic announcement verbosity", {SDLK_F11, C}},
		{AccessAction::TOGGLE_CUES, "audio cues", "Turn accessibility audio cues on or off", {SDLK_F10, C}},
		{AccessAction::TOGGLE_CHATTER, "ship chatter",
			"Turn spoken chatter from other ships on or off (it stays in the message log)", {SDLK_F9, C}},
		{AccessAction::TOGGLE_ARRIVAL_PAUSE, "arrival pause",
			"Turn on or off pausing the game each time you arrive in a new system", {'p', C}},
		{AccessAction::TOGGLE_MOTION, "motion announcements",
			"Turn on or off spoken heading while turning, full speed and stopped",
			{'o', static_cast<uint16_t>(C | S)}},
		{AccessAction::LOCATION, "location", "Current location", {'l', C}},
		{AccessAction::STATUS, "status", "Flagship status", {'s', C}},
		{AccessAction::TARGET, "target", "Current target", {'t', C}},
		{AccessAction::DESTINATION, "destination", "Destination, route and fuel", {'d', C}},
		{AccessAction::NEARBY, "nearby", "Nearby planets and stations", {'n', C}},
		{AccessAction::HOSTILES, "hostiles", "Nearby hostile ships", {'h', C}},
		{AccessAction::FLEET, "fleet", "Fleet and nearby friendly ships", {'f', C}},
		{AccessAction::MISSIONS, "missions", "Active missions (repeat to go to the next one)", {'m', C}},
		{AccessAction::CARGO, "cargo", "Cargo, passengers and credits", {'k', C}},
		{AccessAction::MESSAGES, "messages", "Recent game messages (repeat for older ones)", {'e', C}},
		{AccessAction::WEAPONS, "weapons", "Weapons, ammunition, energy and heat", {'g', C}},
		{AccessAction::MOTION, "motion", "Heading, speed and drift", {'o', C}},
		{AccessAction::BEACON, "beacon", "Toggle the audio beacon toward the current target or destination",
			{'b', C}},
		{AccessAction::REVIEW_PREVIOUS, "review previous", "Review cursor: previous item", {SDLK_UP, C},
			{SDLK_F5, 0}},
		{AccessAction::REVIEW_NEXT, "review next", "Review cursor: next item", {SDLK_DOWN, C}, {SDLK_F6, 0}},
		{AccessAction::REVIEW_FIRST, "review first", "Review cursor: first item", {SDLK_HOME, C}, {SDLK_F5, S}},
		{AccessAction::REVIEW_LAST, "review last", "Review cursor: last item", {SDLK_END, C}, {SDLK_F6, S}},
		{AccessAction::REVIEW_PREVIOUS_GROUP, "review previous group", "Review cursor: previous list or category",
			{SDLK_LEFT, C}, {SDLK_F7, 0}},
		{AccessAction::REVIEW_NEXT_GROUP, "review next group", "Review cursor: next list or category",
			{SDLK_RIGHT, C}, {SDLK_F8, 0}},
		{AccessAction::REVIEW_ACTIVATE, "review activate", "Review cursor: activate or select the item",
			{SDLK_RETURN, C}, {SDLK_F9, 0}},
		{AccessAction::REVIEW_CURRENT, "review current", "Review cursor: read the current item in detail",
			{SDLK_SPACE, C}, {SDLK_F4, 0}},
	};

	mutex bindingMutex;
	map<AccessAction, Binding> bindings;
	map<AccessAction, Binding> alternates;
	// Keys that are bound with the Ctrl modifier.
	set<int32_t> ctrlKeys;
	// Keys currently held down after being used in an accessibility chord.
	set<int32_t> heldChordKeys;

	void Rebuild()
	{
		ctrlKeys.clear();
		for(const auto &it : bindings)
			if(it.second.modifiers & C)
				ctrlKeys.insert(it.second.keycode);
		for(const auto &it : alternates)
			if(it.second.modifiers & C)
				ctrlKeys.insert(it.second.keycode);
	}

	uint16_t ParseModifier(const string &token)
	{
		if(token == "ctrl" || token == "control")
			return AccessKeys::CTRL;
		if(token == "shift")
			return AccessKeys::SHIFT;
		if(token == "alt")
			return AccessKeys::ALT;
		return 0;
	}
}



void AccessKeys::Load(const filesystem::path &file)
{
	lock_guard<mutex> lock(bindingMutex);
	bindings.clear();
	alternates.clear();
	for(const Info &info : INFO)
	{
		bindings[info.action] = info.defaultBinding;
		alternates[info.action] = info.defaultAlternate;
	}

	if(!file.empty() && Files::Exists(file))
	{
		DataFile data(file);
		int version = 1;
		for(const DataNode &node : data)
			if(node.Token(0) == "keys version" && node.Size() >= 2)
				version = static_cast<int>(node.Value(1));
		for(const DataNode &node : data)
		{
			// Format: "<action id>" [ctrl] [shift] [alt] <key name or code>
			if(node.Size() < 2)
				continue;
			for(const Info &info : INFO)
			{
				bool isAlternate = node.Token(0) == string(info.id) + " alternate";
				if(node.Token(0) == info.id || isAlternate)
				{
					map<AccessAction, Binding> &target = isAlternate ? alternates : bindings;
					Binding binding;
					for(int i = 1; i < node.Size(); ++i)
					{
						uint16_t modifier = ParseModifier(node.Token(i));
						if(modifier)
							binding.modifiers |= modifier;
						else if(node.IsNumber(i))
							binding.keycode = static_cast<int32_t>(node.Value(i));
						else
							binding.keycode = SDL_GetKeyFromName(node.Token(i).c_str());
					}
					if(binding.keycode)
						target[info.action] = binding;
					else if(node.Token(node.Size() - 1) == "none")
						target[info.action] = Binding();
				}
			}
		}
		// Version 1 files (before 1.0.6) saved Shift+F2 for the key reference,
		// which is now "previous key"; move it to the new default.
		if(version < 2)
		{
			Binding &reference = bindings[AccessAction::KEY_REFERENCE];
			if(reference.keycode == SDLK_F2 && reference.modifiers == S)
			{
				reference = Binding{SDLK_F2, C};
				bindings[AccessAction::CONTEXT_HELP_PREVIOUS] = Binding{SDLK_F2, S};
			}
		}
	}
	Rebuild();
}



void AccessKeys::Save(const filesystem::path &file)
{
	DataWriter out(file);
	out.WriteComment("Endless Sky accessibility key bindings.");
	out.WriteComment("Format: \"action\" [ctrl] [shift] [alt] \"key name\". Use \"none\" to disable a key.");
	out.WriteComment("Key names are SDL key names, for example \"F2\", \"Page Up\", \"Return\", \"r\".");
	out.Write("keys version", 2);
	lock_guard<mutex> lock(bindingMutex);
	auto write = [&out](const string &id, const Binding &binding)
	{
		out.WriteToken(id);
		if(binding.modifiers & CTRL)
			out.WriteToken("ctrl");
		if(binding.modifiers & SHIFT)
			out.WriteToken("shift");
		if(binding.modifiers & ALT)
			out.WriteToken("alt");
		out.WriteToken(binding.keycode ? SDL_GetKeyName(binding.keycode) : "none");
		out.Write();
	};
	for(const Info &info : INFO)
	{
		write(info.id, bindings[info.action]);
		if(alternates[info.action].keycode || info.defaultAlternate.keycode)
			write(string(info.id) + " alternate", alternates[info.action]);
	}
}



AccessAction AccessKeys::Match(int32_t keycode, uint16_t sdlModifiers)
{
	uint16_t mods = FromSDL(sdlModifiers);
	lock_guard<mutex> lock(bindingMutex);
	for(const auto *table : {&bindings, &alternates})
		for(const auto &it : *table)
		{
			if(!it.second.keycode || it.second.keycode != keycode || it.second.modifiers != mods)
				continue;
			// An unmodified accessibility key yields to any game command the
			// player has bound to the same key.
			if(!(mods & (CTRL | ALT)))
			{
				Command command(keycode);
				if(command)
					continue;
			}
			return it.first;
		}
	return AccessAction::NONE;
}



bool AccessKeys::IsReservedNow(int32_t keycode)
{
	bool ctrl = SDL_GetModState() & KMOD_CTRL;
	lock_guard<mutex> lock(bindingMutex);
	if(heldChordKeys.contains(keycode))
		return true;
	return ctrl && ctrlKeys.contains(keycode);
}



void AccessKeys::ChordPressed(int32_t keycode)
{
	lock_guard<mutex> lock(bindingMutex);
	heldChordKeys.insert(keycode);
}



void AccessKeys::KeyReleased(int32_t keycode)
{
	lock_guard<mutex> lock(bindingMutex);
	heldChordKeys.erase(keycode);
}



const vector<AccessKeys::Info> &AccessKeys::All()
{
	return INFO;
}



AccessKeys::Binding AccessKeys::Get(AccessAction action)
{
	lock_guard<mutex> lock(bindingMutex);
	auto it = bindings.find(action);
	return it == bindings.end() ? Binding() : it->second;
}



AccessKeys::Binding AccessKeys::GetAlternate(AccessAction action)
{
	lock_guard<mutex> lock(bindingMutex);
	auto it = alternates.find(action);
	return it == alternates.end() ? Binding() : it->second;
}



string AccessKeys::KeyName(AccessAction action)
{
	string name = BindingName(Get(action));
	Binding alternate = GetAlternate(action);
	if(alternate.keycode)
		name += " or " + BindingName(alternate);
	return name;
}



string AccessKeys::Description(AccessAction action)
{
	for(const Info &info : INFO)
		if(info.action == action)
			return info.description;
	return string();
}



vector<string> AccessKeys::Conflicts()
{
	vector<string> result;
	for(const Info &info : INFO)
		for(const Binding &binding : {Get(info.action), GetAlternate(info.action)})
		{
			if(!binding.keycode || (binding.modifiers & (CTRL | ALT)))
				continue;
			Command command(binding.keycode);
			if(command)
				result.push_back(BindingName(binding) + " is used by the game for " + command.Description()
					+ ", so it does not work for " + info.description + ".");
		}
	return result;
}



uint16_t AccessKeys::FromSDL(uint16_t sdlModifiers)
{
	uint16_t result = 0;
	if(sdlModifiers & KMOD_CTRL)
		result |= CTRL;
	if(sdlModifiers & KMOD_SHIFT)
		result |= SHIFT;
	if(sdlModifiers & KMOD_ALT)
		result |= ALT;
	return result;
}



string AccessKeys::BindingName(const Binding &binding)
{
	if(!binding.keycode)
		return "unassigned";
	string name;
	if(binding.modifiers & CTRL)
		name += "Control+";
	if(binding.modifiers & SHIFT)
		name += "Shift+";
	if(binding.modifiers & ALT)
		name += "Alt+";
	string key = SDL_GetKeyName(binding.keycode);
	if(key.empty())
		key = "key " + to_string(binding.keycode);
	return name + key;
}
