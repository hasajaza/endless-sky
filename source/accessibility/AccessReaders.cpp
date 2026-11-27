/* AccessReaders.cpp
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

#include "AccessReaders.h"

#include "AccessCapture.h"
#include "AccessContext.h"
#include "AccessFlight.h"
#include "AccessFormat.h"
#include "AccessKeys.h"
#include "AccessSpeech.h"

#include "../Command.h"
#include "../Panel.h"
#include "../Rectangle.h"
#include "../PlayerInfo.h"
#include "../Ship.h"
#include "../SDL.h"
#include "../System.h"

#include <algorithm>
#include <cmath>

using namespace std;

namespace {
	// Readers for screens without dedicated support: the generic screen text.
	class GenericReader : public AccessReader {
	public:
		using AccessReader::AccessReader;

		string Title() override
		{
			string id = panel.AccessibilityId();
			if(id == "GameLoadingPanel")
				return "Loading";
			if(id.empty())
				return "Window";
			// "ShipNameDialogPanel" -> "Ship name dialog".
			string result;
			for(char c : id.substr(0, id.size() - (id.ends_with("Panel") ? 5 : 0)))
			{
				if(isupper(static_cast<unsigned char>(c)) && !result.empty())
					result += ' ';
				result += result.empty() ? c : static_cast<char>(tolower(static_cast<unsigned char>(c)));
			}
			return result;
		}

		void OnOpen() override
		{
			string id = panel.AccessibilityId();
			// The loading screen says nothing new; the welcome text covers it.
			if(id == "GameLoadingPanel")
				return;
			vector<AccessCapture::Item> lines = AccessCapture::Lines(&panel);
			string text = Title();
			for(size_t i = 0; i < lines.size() && i < 6; ++i)
				text += ". " + lines[i].text;
			Say(text);
		}

		string Summary() override
		{
			vector<AccessCapture::Item> lines = AccessCapture::Lines(&panel);
			string text;
			for(size_t i = 0; i < lines.size() && i < 10; ++i)
				text += lines[i].text + ". ";
			return text;
		}
	};


	// Flight (the main panel).
	class FlightReader : public AccessReader {
	public:
		using AccessReader::AccessReader;

		string Title() override
		{
			const System *system = Player().GetSystem();
			return system ? "Flying in " + system->DisplayName() : "Flight";
		}

		void OnOpen() override
		{
			Say(Title(), SpeechPriority::FOCUS);
		}

		void OnReturn() override
		{
			Say(Title(), SpeechPriority::FOCUS);
		}

		string Summary() override
		{
			return AccessFlight::Status();
		}

		vector<string> Keys() override
		{
			auto key = [](const Command &command)
			{
				return command.HasBinding() ? AccessFormat::FriendlyKey(command.KeyName()) : string("unassigned");
			};
			return {
				key(Command::FORWARD) + " thrust, " + key(Command::LEFT) + " and " + key(Command::RIGHT) + " turn, "
					+ key(Command::BACK) + " reverse or turn around",
				key(Command::AUTOSTEER) + " hold to steer toward your target",
				key(Command::LAND) + " land, repeat to choose another planet",
				key(Command::JUMP) + " hyperspace jump",
				key(Command::TARGET) + " next ship, " + key(Command::NEAREST) + " nearest hostile, "
					+ key(Command::NEAREST_ASTEROID) + " nearest asteroid",
				key(Command::PRIMARY) + " fire primary, " + key(Command::SECONDARY) + " fire secondary, "
					+ key(Command::SELECT) + " choose secondary weapon",
				key(Command::BOARD) + " board, " + key(Command::HAIL) + " hail, " + key(Command::SCAN) + " scan",
				key(Command::MAP) + " map, " + key(Command::INFO) + " player info, " + key(Command::MESSAGE_LOG)
					+ " message log",
				"Control plus L location, S status, T target, D destination, N nearby planets, H hostiles, "
					"F fleet, M missions, K cargo, G weapons, O motion, E messages, B beacon",
				"Control plus Up and Down arrows browse nearby objects; Control plus Left and Right change "
					"category; Control plus Enter selects the object"
			};
		}

		void BuildReview(vector<ReviewGroup> &groups) override
		{
			AccessFlight::BuildReview(groups);
		}

		void PostKey(int32_t key, uint16_t mod) override
		{
			// The game's escort group keys say nothing; the selection changes
			// in the next engine step, so speak a few frames later.
			if(key < '0' || key > '9' || (mod & KMOD_ALT))
				return;
			int group = key - '0';
			bool shift = mod & KMOD_SHIFT;
			bool control = mod & (KMOD_CTRL | KMOD_GUI);
			Later(3, [group, shift, control]() { AccessFlight::AnnounceGroup(group, shift, control); });
		}

		bool KeepReview() const override
		{
			return true;
		}
	};
}



unique_ptr<AccessReader> CreateAccessReader(Panel &panel)
{
	string id = panel.AccessibilityId();
	if(id == "MainPanel")
		return make_unique<FlightReader>(panel);
	for(auto factory : {CreateMenuReader, CreateDialogReader, CreatePlanetReader, CreateShopReader,
			CreateMapReader, CreateInfoReader})
	{
		unique_ptr<AccessReader> reader = factory(panel, id);
		if(reader)
			return reader;
	}
	return make_unique<GenericReader>(panel);
}



string AccessText::LineAt(const Panel &panel, double y, double tolerance)
{
	string best;
	double bestDistance = tolerance;
	for(const AccessCapture::Item &item : AccessCapture::Lines(&panel))
	{
		double distance = fabs(item.y - y);
		if(distance <= bestDistance)
		{
			bestDistance = distance;
			best = item.text;
		}
	}
	return best;
}



string AccessText::TextIn(const Panel &panel, const Rectangle &area)
{
	if(area.Width() <= 0. || area.Height() <= 0.)
		return string();
	vector<AccessCapture::Item> items;
	for(const AccessCapture::Item &item : AccessCapture::Raw(&panel))
		if(item.x >= area.Left() - 2. && item.x <= area.Right() && item.y >= area.Top() - 6.
				&& item.y <= area.Bottom())
			items.push_back(item);
	sort(items.begin(), items.end(), [](const AccessCapture::Item &a, const AccessCapture::Item &b)
	{
		return fabs(a.y - b.y) > 6. ? a.y < b.y : a.x < b.x;
	});
	string result;
	for(const AccessCapture::Item &item : items)
		result += (result.empty() ? "" : ", ") + item.text;
	return result;
}



vector<string> AccessText::Paragraphs(const string &text)
{
	vector<string> result;
	string current;
	for(size_t i = 0; i < text.size(); ++i)
	{
		char c = text[i];
		if(c == '\n')
		{
			if(!current.empty() && current.find_first_not_of(" \t") != string::npos)
				result.push_back(AccessSpeech::Clean(current));
			current.clear();
		}
		else
			current += c;
	}
	if(!current.empty() && current.find_first_not_of(" \t") != string::npos)
		result.push_back(AccessSpeech::Clean(current));
	return result;
}



bool AccessText::IsTypingKey(int32_t key)
{
	return (key >= ' ' && key <= '~') || key == SDLK_BACKSPACE || key == SDLK_DELETE;
}



string AccessText::EchoEdit(const string &before, const string &after)
{
	auto charName = [](char c) -> string
	{
		if(c == ' ')
			return "space";
		if(c == '.')
			return "dot";
		if(c == '-')
			return "dash";
		// Names must be typed with the right capitals, so say which letters are.
		if(c >= 'A' && c <= 'Z')
			return "cap " + string(1, c);
		return string(1, c);
	};
	if(after.size() == before.size() + 1 && after.compare(0, before.size(), before) == 0)
		return charName(after.back());
	if(before.size() == after.size() + 1 && before.compare(0, after.size(), after) == 0)
		return "deleted " + charName(before.back());
	if(after.empty() && !before.empty())
		return "cleared";
	if(after != before)
		return after.empty() ? "blank" : after;
	return string();
}
