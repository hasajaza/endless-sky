/* ReadersInfo.cpp
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

// Readers for the player and ship information screens, the logbook, the
// message log and boarding.

#include "AccessReaders.h"

#include "AccessCapture.h"
#include "AccessContext.h"
#include "AccessFlight.h"
#include "AccessFormat.h"
#include "AccessInspector.h"
#include "AccessSpeech.h"

#include "../Account.h"
#include "../CaptureOdds.h"
#include "../CargoHold.h"
#include "../Government.h"
#include "../Hardpoint.h"
#include "../InfoPanelState.h"
#include "../Message.h"
#include "../Messages.h"
#include "../Outfit.h"
#include "../PlayerInfo.h"
#include "../SDL.h"
#include "../Ship.h"
#include "../System.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>

using namespace std;

namespace {
	string ShipSummary(const Ship &ship)
	{
		PlayerInfo &player = AccessContext::Player();
		string text = ship.GivenName() + ", " + ship.DisplayModelName();
		if(&ship == player.Flagship())
			text += ", flagship";
		if(ship.IsParked())
			text += ", parked";
		if(ship.IsDisabled())
			text += ", disabled";
		text += ", shields " + AccessFormat::Percent(ship.ShieldFraction()) + ", hull "
			+ AccessFormat::Percent(ship.HullFraction());
		text += ", crew " + to_string(ship.Crew()) + " of " + to_string(ship.RequiredCrew()) + " required";
		if(ship.GetSystem())
			text += ", in " + ship.GetSystem()->DisplayName();
		return text;
	}



	class PlayerInfoReader : public AccessReader {
	public:
		PlayerInfoReader(Panel &panel) : AccessReader(panel), info(static_cast<PlayerInfoPanel &>(panel)) {}

		string Title() override { return "Player information"; }

		void OnOpen() override
		{
			PlayerInfo &player = Player();
			string text = "Player information. " + player.FirstName() + " " + player.LastName() + ". "
				+ AccessFormat::Credits(player.Accounts().Credits()) + ", net worth "
				+ AccessFormat::Credits(player.Accounts().NetWorth()) + ". "
				+ AccessFormat::Plural(static_cast<int64_t>(AccessInspector::InfoState(info).Ships().size()), "ship")
				+ ". " + Selected() + " Press S or Enter for ship details, F2 for keys.";
			Say(text, SpeechPriority::FOCUS);
		}

		string Summary() override
		{
			return Selected();
		}

		vector<string> Keys() override
		{
			return {"Up and Down arrows select a ship; Shift extends the selection",
				"Control Shift Up and Down move the selected ships in the fleet order",
				"While landed: K or Shift P parks or unparks the selected ships, A parks or unparks all ships, "
					"C parks or unparks the ships in this system",
				"V saves a sorted order as the fleet order",
				"S or Enter open the ship information screen", "Page Up, Page Down, Home and End scroll",
				"I, D or Escape close", "Control plus Down arrow reviews all the information on the screen"};
		}

		void PostKey(int32_t key, uint16_t mod) override
		{
			if(mod & (KMOD_CTRL | KMOD_GUI | KMOD_ALT))
			{
				if(key == SDLK_UP || key == SDLK_DOWN)
					Say(Selected());
				return;
			}
			bool shift = mod & KMOD_SHIFT;
			const InfoPanelState &state = AccessInspector::InfoState(info);
			if(key == SDLK_UP || key == SDLK_DOWN)
				Say(Selected());
			else if(key == 'k' || key == 'a' || key == 'c' || (key == 'p' && shift))
			{
				if(!state.CanEdit())
					Say("Ships can be parked only while landed.");
				else
					Later(1, [this, key]() { Say(ParkResult(key)); });
			}
			else if(key == 'v' && state.CanEdit())
				Say("Fleet order saved.");
		}

		void BuildReview(vector<ReviewGroup> &groups) override
		{
			ReviewGroup ships{"Ships", {}};
			for(const shared_ptr<Ship> &ship : AccessInspector::InfoState(info).Ships())
				ships.items.emplace_back(ShipSummary(*ship));
			groups.push_back(std::move(ships));
			// The screen draws the pilot column beside the fleet table, so its
			// lines mix both. Keep only the pilot column, left of the table.
			vector<AccessCapture::Item> raw = AccessCapture::Raw(&panel);
			double tableLeft = numeric_limits<double>::max();
			for(const AccessCapture::Item &item : raw)
				if(item.text == "ship")
					tableLeft = min(tableLeft, item.x);
			map<long, vector<const AccessCapture::Item *>> rows;
			for(const AccessCapture::Item &item : raw)
				if(item.x < tableLeft - 5. && !item.text.empty())
					rows[lround(item.y)].push_back(&item);
			ReviewGroup pilot{"Pilot", {}};
			set<string> seen;
			for(auto &[y, items] : rows)
			{
				sort(items.begin(), items.end(), [](const auto *a, const auto *b) { return a->x < b->x; });
				string line;
				for(const AccessCapture::Item *item : items)
				{
					if(line.empty())
						line = item->text;
					else
						line += (line.back() == ':' ? " " : ", ") + item->text;
				}
				if(line.find("Player Info") != string::npos || line == "Ship Info" || !seen.insert(line).second)
					continue;
				pilot.items.emplace_back(line);
			}
			if(!pilot.items.empty())
				groups.push_back(std::move(pilot));
		}

	private:
		string ParkResult(int32_t key)
		{
			const InfoPanelState &state = AccessInspector::InfoState(info);
			const vector<shared_ptr<Ship>> &ships = state.Ships();
			if(key == 'k' || key == 'p')
			{
				vector<string> parked;
				vector<string> active;
				for(int index : state.AllSelected())
				{
					if(index < 0 || index >= static_cast<int>(ships.size()))
						continue;
					const Ship &ship = *ships[static_cast<size_t>(index)];
					(ship.IsParked() ? parked : active).push_back(ship.GivenName());
				}
				string text;
				if(!parked.empty())
					text += "Parked: " + AccessFormat::List(parked) + ". ";
				if(!active.empty())
					text += "Active: " + AccessFormat::List(active) + ".";
				return text.empty() ? string("No ship selected.") : text;
			}
			int parked = 0;
			for(const shared_ptr<Ship> &ship : ships)
				parked += ship->IsParked();
			return to_string(parked) + " of " + to_string(ships.size()) + " ships parked. Parked ships stay "
				"here and do not follow you.";
		}

		string Selected()
		{
			const InfoPanelState &state = AccessInspector::InfoState(info);
			int index = state.SelectedIndex();
			if(index < 0 || index >= static_cast<int>(state.Ships().size()))
				return "No ship selected.";
			string text = ShipSummary(*state.Ships()[static_cast<size_t>(index)]) + ". Ship "
				+ to_string(index + 1) + " of " + to_string(state.Ships().size());
			if(state.AllSelected().size() > 1)
				text += ". " + to_string(state.AllSelected().size()) + " ships selected";
			return text + ".";
		}

	private:
		PlayerInfoPanel &info;
	};



	class ShipInfoReader : public AccessReader {
	public:
		ShipInfoReader(Panel &panel) : AccessReader(panel), shipInfo(static_cast<ShipInfoPanel &>(panel)) {}

		string Title() override { return "Ship information"; }

		void OnOpen() override
		{
			Say("Ship information. " + Current() + " Press F2 for keys, Control plus Down arrow for details.",
				SpeechPriority::FOCUS);
		}

		string Summary() override
		{
			return Current();
		}

		vector<string> Keys() override
		{
			return {"Left, Right, Up and Down arrows, or P and N, go to the previous or next ship",
				"Tab and Shift Tab choose an item of cargo, or an outfit that can be jettisoned",
				"C or J jettisons the chosen item (in space only); with nothing chosen, all the cargo",
				"W and Shift W go through the weapon hardpoints",
				"Space on a hardpoint picks up its weapon; Space on another hardpoint of the same kind swaps "
					"them (while landed)",
				"R renames the ship", "Shift P or K parks or unparks it", "Shift D disowns it",
				"L logbook, M map", "I returns to player information", "D or Escape close"};
		}

		bool PreKey(int32_t key, uint16_t mod) override
		{
			if(mod & (KMOD_CTRL | KMOD_GUI | KMOD_ALT))
				return false;
			bool shift = mod & KMOD_SHIFT;
			if(key == SDLK_TAB)
			{
				StepItem(shift ? -1 : 1);
				return true;
			}
			if(key == 'w')
			{
				StepHardpoint(shift ? -1 : 1);
				return true;
			}
			if(key == ' ' && hardpoint >= 0)
			{
				PickOrSwap();
				return true;
			}
			return false;
		}

		void PostKey(int32_t key, uint16_t mod) override
		{
			if(key == SDLK_LEFT || key == SDLK_RIGHT || key == SDLK_UP || key == SDLK_DOWN || key == 'n'
					|| (key == 'p' && !(mod & KMOD_SHIFT)))
			{
				// Another ship: start the item and hardpoint lists again.
				item = -1;
				hardpoint = -1;
				picked = -1;
				AccessInspector::SelectDump(shipInfo, string(), nullptr, nullptr);
				Later(1, [this]() { Say(Current()); });
			}
			else if(key == 'k' || key == 'P' || (key == 'p' && (mod & KMOD_SHIFT)))
				Later(1, [this]() { Say(Current()); });
		}

		void BuildReview(vector<ReviewGroup> &groups) override
		{
			const ShipInfoDisplay &display = AccessInspector::Info(shipInfo);
			ReviewGroup attributes{"Attributes", {}};
			const auto &labels = AccessInspector::AttributeLabels(display);
			const auto &values = AccessInspector::AttributeValues(display);
			for(size_t i = 0; i < labels.size() && i < values.size(); ++i)
				if(!labels[i].empty() || !values[i].empty())
					attributes.items.emplace_back(labels[i] + " " + values[i]);
			groups.push_back(std::move(attributes));
			ReviewGroup energy{"Energy and heat", {}};
			for(const string &line : AccessInspector::EnergyHeatLines(display))
				energy.items.emplace_back(line);
			groups.push_back(std::move(energy));

			const Ship *ship = CurrentShip();
			if(ship)
			{
				ReviewGroup outfits{"Installed outfits", {}};
				for(const auto &it : ship->Outfits())
					if(it.second)
						outfits.items.emplace_back((it.second > 1 ? to_string(it.second) + " " : string())
							+ it.first->DisplayName() + ", " + it.first->Category());
				groups.push_back(std::move(outfits));

				ReviewGroup cargo{"Cargo", {}};
				for(const auto &it : ship->Cargo().Commodities())
					if(it.second)
						cargo.items.emplace_back(AccessFormat::Plural(it.second, "ton") + " of " + it.first);
				for(const auto &it : ship->Cargo().Outfits())
					if(it.second)
						cargo.items.emplace_back(to_string(it.second) + " " + it.first->DisplayName());
				if(ship->Cargo().Passengers())
					cargo.items.emplace_back(AccessFormat::Plural(ship->Cargo().Passengers(), "passenger"));
				groups.push_back(std::move(cargo));
			}
			AddScreenText(groups);
		}

	private:
		const Ship *CurrentShip()
		{
			const InfoPanelState &state = AccessInspector::InfoState(shipInfo);
			int index = state.SelectedIndex();
			if(index < 0 || index >= static_cast<int>(state.Ships().size()))
				return state.Ships().empty() ? nullptr : state.Ships().front().get();
			return state.Ships()[static_cast<size_t>(index)].get();
		}

		string Current()
		{
			const Ship *ship = CurrentShip();
			if(!ship)
				return "No ship.";
			string text = ShipSummary(*ship) + ". Cargo " + AccessFormat::Integer(ship->Cargo().Used()) + " of "
				+ AccessFormat::Integer(ship->Cargo().Size()) + " tons";
			vector<string> contents;
			for(const Item &it : Items(*ship))
				if(!it.installed)
					contents.push_back(it.text);
			if(ship->Cargo().Passengers())
				contents.push_back(AccessFormat::Plural(ship->Cargo().Passengers(), "passenger"));
			if(!contents.empty())
				text += ": " + AccessFormat::List(contents);
			text += ". Outfit space free " + AccessFormat::Integer(ship->Attributes().Get("outfit space")) + ".";
			// While landed the game keeps all commodities in one fleet hold.
			if(Player().GetPlanet() && Player().Cargo().Used() > 0)
				text += " While landed, your cargo is kept for the whole fleet: "
					+ AccessFormat::Plural(Player().Cargo().Used(), "ton") + "; it goes back into the ships "
					"when you take off.";
			return text;
		}

		// Something on the ship that can be jettisoned.
		struct Item {
			string text;
			string commodity;
			const Outfit *plunder = nullptr;
			const Outfit *outfit = nullptr;
			bool installed = false;
		};

		vector<Item> Items(const Ship &ship)
		{
			vector<Item> items;
			for(const auto &it : ship.Cargo().Commodities())
				if(it.second > 0)
					items.push_back({AccessFormat::Plural(it.second, "ton") + " of " + it.first, it.first});
			for(const auto &it : ship.Cargo().Outfits())
				if(it.second > 0)
				{
					Item entry;
					entry.text = to_string(it.second) + " " + (it.second == 1 ? it.first->DisplayName()
						: it.first->PluralName()) + " in cargo";
					entry.plunder = it.first;
					items.push_back(entry);
				}
			for(const auto &it : ship.Outfits())
				if(it.second > 0 && it.first->Get("can jettison"))
				{
					Item entry;
					entry.text = to_string(it.second) + " " + (it.second == 1 ? it.first->DisplayName()
						: it.first->PluralName()) + ", installed";
					entry.outfit = it.first;
					entry.installed = true;
					items.push_back(entry);
				}
			return items;
		}

		Ship *EditableShip()
		{
			const InfoPanelState &state = AccessInspector::InfoState(shipInfo);
			int index = state.SelectedIndex();
			if(state.Ships().empty())
				return nullptr;
			if(index < 0 || index >= static_cast<int>(state.Ships().size()))
				index = 0;
			return state.Ships()[static_cast<size_t>(index)].get();
		}

		void StepItem(int direction)
		{
			const Ship *ship = CurrentShip();
			if(!ship)
				return;
			vector<Item> items = Items(*ship);
			if(items.empty())
			{
				Say("This ship carries nothing that can be jettisoned.", SpeechPriority::FOCUS);
				return;
			}
			int count = static_cast<int>(items.size());
			item = item < 0 ? (direction > 0 ? 0 : count - 1) : (item + direction + count) % count;
			const Item &chosen = items[static_cast<size_t>(item)];
			AccessInspector::SelectDump(shipInfo, chosen.commodity, chosen.plunder, chosen.outfit);
			string text = chosen.text + ", " + to_string(item + 1) + " of " + to_string(count) + ".";
			if(!chosen.commodity.empty() || chosen.plunder || chosen.outfit)
			{
				if(AccessInspector::InfoState(shipInfo).CanEdit())
					text += " Jettison works only in space.";
				else if(AccessInspector::CanDump(shipInfo))
					text += " C to jettison.";
			}
			Say(text, SpeechPriority::FOCUS);
		}

		string HardpointText(const Ship &ship, int index)
		{
			const vector<Hardpoint> &points = ship.Weapons();
			const Hardpoint &point = points[static_cast<size_t>(index)];
			// Number guns and turrets separately, as the screen groups them.
			int number = 0;
			int total = 0;
			for(int i = 0; i < static_cast<int>(points.size()); ++i)
				if(points[static_cast<size_t>(i)].IsTurret() == point.IsTurret())
				{
					++total;
					if(i <= index)
						++number;
				}
			const Point &where = point.GetPoint();
			string side = where.Y() < 0. ? "front" : "rear";
			if(where.X() < -1.)
				side += " left";
			else if(where.X() > 1.)
				side += " right";
			else
				side += " center";
			const Outfit *weapon = point.GetOutfit();
			return string(point.IsTurret() ? "Turret " : "Gun ") + to_string(number) + " of " + to_string(total)
				+ ", " + side + ": " + (weapon ? weapon->DisplayName() : string("empty"));
		}

		void StepHardpoint(int direction)
		{
			const Ship *ship = CurrentShip();
			if(!ship)
				return;
			int count = static_cast<int>(ship->Weapons().size());
			if(!count)
			{
				Say("This ship has no weapon hardpoints.", SpeechPriority::FOCUS);
				return;
			}
			hardpoint = hardpoint < 0 ? (direction > 0 ? 0 : count - 1) : (hardpoint + direction + count) % count;
			string text = HardpointText(*ship, hardpoint) + ".";
			if(picked >= 0)
				text += picked == hardpoint ? " Picked up; Space puts it back." : " Space to swap here.";
			Say(text, SpeechPriority::FOCUS);
		}

		void PickOrSwap()
		{
			Ship *ship = EditableShip();
			if(!ship || hardpoint >= static_cast<int>(ship->Weapons().size()))
				return;
			const InfoPanelState &state = AccessInspector::InfoState(shipInfo);
			if(!state.CanEdit() || ship->GetSystem() != Player().GetSystem() || ship->IsDisabled())
			{
				Say("Weapons can be moved only while landed, on a ship that is with you and not disabled.",
					SpeechPriority::FOCUS);
				return;
			}
			const vector<Hardpoint> &points = ship->Weapons();
			if(picked < 0)
			{
				picked = hardpoint;
				const Outfit *weapon = points[static_cast<size_t>(picked)].GetOutfit();
				Say("Picked up " + (weapon ? weapon->DisplayName() : string("the empty slot")) + ". W to choose "
					"another " + (points[static_cast<size_t>(picked)].IsTurret() ? "turret" : "gun")
					+ ", Space to swap. Space here again to cancel.", SpeechPriority::FOCUS);
				return;
			}
			if(picked == hardpoint)
			{
				picked = -1;
				Say("Cancelled.", SpeechPriority::FOCUS);
				return;
			}
			if(points[static_cast<size_t>(picked)].IsTurret() != points[static_cast<size_t>(hardpoint)].IsTurret())
			{
				Say("A gun and a turret cannot be swapped. Choose another "
					+ string(points[static_cast<size_t>(picked)].IsTurret() ? "turret." : "gun."),
					SpeechPriority::FOCUS);
				return;
			}
			AccessInspector::ClearDrag(shipInfo);
			ship->GetArmament().Swap(static_cast<unsigned>(picked), static_cast<unsigned>(hardpoint));
			string text = "Swapped. " + HardpointText(*ship, hardpoint) + ". " + HardpointText(*ship, picked) + ".";
			picked = -1;
			Say(text, SpeechPriority::FOCUS);
		}

	private:
		ShipInfoPanel &shipInfo;
		int item = -1;
		int hardpoint = -1;
		int picked = -1;
	};



	class MessageLogReader : public AccessReader {
	public:
		using AccessReader::AccessReader;

		string Title() override { return "Message log"; }

		void OnOpen() override
		{
			const auto &log = Messages::GetLog();
			index = 0;
			string text = "Message log, " + AccessFormat::Plural(static_cast<int64_t>(log.size()), "message") + ".";
			if(!log.empty())
				text += " Latest: " + log.front().first;
			text += " Up and Down arrows read messages, Home and End jump.";
			Say(text, SpeechPriority::FOCUS);
		}

		vector<string> Keys() override
		{
			return {"Up arrow older message, Down arrow newer message", "Home oldest, End newest",
				"I toggles showing only important messages", "C clears the log", "D or Escape close"};
		}

		bool PreKey(int32_t key, uint16_t mod) override
		{
			const auto &log = Messages::GetLog();
			if(log.empty())
				return false;
			int count = static_cast<int>(log.size());
			if(key == SDLK_UP)
				index = min(count - 1, index + 1);
			else if(key == SDLK_DOWN)
				index = max(0, index - 1);
			else if(key == SDLK_HOME)
				index = count - 1;
			else if(key == SDLK_END)
				index = 0;
			else
				return false;
			Say(to_string(index + 1) + ": " + log[static_cast<size_t>(index)].first);
			// Let the game scroll its view as usual.
			return false;
		}

		void BuildReview(vector<ReviewGroup> &groups) override
		{
			ReviewGroup log{"Messages, newest first", {}};
			for(const auto &it : Messages::GetLog())
				log.items.emplace_back(it.first);
			groups.push_back(std::move(log));
		}

	private:
		int index = 0;
	};



	class LogbookReader : public AccessReader {
	public:
		LogbookReader(Panel &panel) : AccessReader(panel), logbook(static_cast<LogbookPanel &>(panel)) {}

		string Title() override { return "Logbook"; }

		void OnOpen() override
		{
			Say("Logbook. " + PageSummary() + " Up and Down choose a date or category; Control plus Down arrow "
				"reads the entries.", SpeechPriority::FOCUS);
		}

		vector<string> Keys() override
		{
			return {"Up and Down arrows choose a date or category", "Page Up, Page Down, Home and End scroll",
				"Control plus arrows read the entries", "D or Escape close"};
		}

		void PostKey(int32_t key, uint16_t mod) override
		{
			if(key == SDLK_UP || key == SDLK_DOWN)
				Later(1, [this]() { Say(PageSummary()); });
		}

		string Summary() override
		{
			return PageSummary();
		}

		// The entries themselves, not the screen text: the logbook's map would
		// otherwise fill the review with system names.
		void BuildReview(vector<ReviewGroup> &groups) override
		{
			ReviewGroup entries{PageName(), {}};
			for(const auto &[heading, text] : AccessInspector::LogbookPage(logbook))
				entries.items.emplace_back(heading + ": " + text);
			if(entries.items.empty())
				entries.items.emplace_back("No entries on this page.");
			groups.push_back(std::move(entries));
		}

	private:
		// Special pages repeat the category as the page name.
		string PageName()
		{
			const string selection = AccessInspector::LogbookSelection(logbook);
			size_t half = selection.size() / 2;
			if(selection.size() % 2 == 1 && selection.substr(0, half) == selection.substr(half + 1))
				return selection.substr(0, half);
			return selection;
		}

		string PageSummary()
		{
			vector<pair<string, string>> page = AccessInspector::LogbookPage(logbook);
			string text = PageName() + ", " + AccessFormat::Plural(static_cast<int64_t>(page.size()), "entry", "entries");
			if(!page.empty())
				text += ". " + page.front().first + ": " + page.front().second;
			if(!text.empty() && text.back() != '.')
				text += ".";
			if(page.size() > 1)
				text += " Control plus Down arrow for the other entries.";
			return text;
		}

	private:
		LogbookPanel &logbook;
	};



	class BoardingReader : public AccessReader {
	public:
		BoardingReader(Panel &panel) : AccessReader(panel), boarding(static_cast<BoardingPanel &>(panel)) {}

		string Title() override { return "Boarding"; }

		void OnOpen() override
		{
			lastMessages = AccessInspector::BoardingMessages(boarding);
			const shared_ptr<Ship> &victim = AccessInspector::Victim(boarding);
			string text = "Boarding";
			if(victim)
				text += " " + AccessFlight::DescribeShip(*victim, true);
			text += ". " + AccessFormat::Plural(AccessInspector::PlunderCount(boarding), "item") + " to plunder. "
				+ Selected() + " " + Options();
			Say(text, SpeechPriority::FOCUS);
		}

		void Step() override
		{
			const string &messages = AccessInspector::BoardingMessages(boarding);
			if(messages != lastMessages)
			{
				string added = messages.size() > lastMessages.size()
					&& messages.compare(0, lastMessages.size(), lastMessages) == 0
					? messages.substr(lastMessages.size()) : messages;
				lastMessages = messages;
				Say(added, SpeechPriority::FOCUS);
			}
			bool capturing = AccessInspector::IsCapturing(boarding);
			if(capturing != wasCapturing)
			{
				wasCapturing = capturing;
				if(capturing)
					Say("Capture attempt. " + Odds() + " " + Options(), SpeechPriority::FOCUS);
			}
		}

		string Summary() override
		{
			return Selected() + " " + (AccessInspector::IsCapturing(boarding) ? Odds() + " " : string()) + Options();
		}

		vector<string> Keys() override
		{
			return {"Up and Down arrows choose an item, Page Up, Page Down, Home and End move faster",
				"T takes the selected item", "C attempts to capture the ship",
				"During a capture: A attacks, D defends", "X or D leaves when possible"};
		}

		void PostKey(int32_t key, uint16_t mod) override
		{
			if(key == SDLK_UP || key == SDLK_DOWN || key == SDLK_PAGEUP || key == SDLK_PAGEDOWN || key == SDLK_HOME
					|| key == SDLK_END)
				Say(Selected());
			else if(key == 't')
				Later(1, [this]() { Say(Selected() + " " + AccessFormat::Integer(Player().Cargo().Free())
					+ " tons of cargo space free."); });
			else if(key == 'a' || key == 'd' || key == 'D')
				Later(1, [this]()
				{
					if(AccessInspector::IsCapturing(boarding))
						Say(Odds());
				});
		}

		void BuildReview(vector<ReviewGroup> &groups) override
		{
			ReviewGroup plunder{"Plunder", {}};
			int count = AccessInspector::PlunderCount(boarding);
			for(int i = 0; i < count; ++i)
				plunder.items.emplace_back(AccessInspector::PlunderLine(boarding, i));
			groups.push_back(std::move(plunder));
			AddScreenText(groups);
		}

	private:
		string Selected()
		{
			int index = AccessInspector::BoardingSelected(boarding);
			int count = AccessInspector::PlunderCount(boarding);
			if(index < 0 || index >= count)
				return "Nothing selected.";
			string text = AccessFormat::TrimPeriod(AccessInspector::PlunderLine(boarding, index)) + ". "
				+ to_string(index + 1) + " of " + to_string(count) + ".";
			if(!AccessInspector::CanTake(boarding))
				text += " " + AccessInspector::CannotTakeReason(boarding);
			return text;
		}

		string Odds()
		{
			const shared_ptr<Ship> &you = AccessInspector::You(boarding);
			const shared_ptr<Ship> &victim = AccessInspector::Victim(boarding);
			if(!you || !victim)
				return string();
			int yours = you->Crew();
			int theirs = victim->Crew();
			string text = "Your crew " + to_string(yours) + ", their crew " + to_string(theirs) + ".";
			double attack = AccessInspector::AttackOdds(boarding).Odds(yours, theirs);
			double defend = 1. - AccessInspector::DefenseOdds(boarding).Odds(theirs, yours);
			text += " Odds if you attack " + AccessFormat::Percent(attack) + ", if you defend "
				+ AccessFormat::Percent(defend) + ".";
			return text;
		}

		string Options()
		{
			vector<string> options;
			if(AccessInspector::CanTake(boarding))
				options.push_back("T take");
			if(AccessInspector::CanCapture(boarding))
				options.push_back("C capture");
			if(AccessInspector::CanAttack(boarding))
				options.push_back("A attack, D defend");
			if(AccessInspector::CanExit(boarding))
				options.push_back("X leave");
			return options.empty() ? string() : "Options: " + AccessFormat::List(options) + ".";
		}

	private:
		BoardingPanel &boarding;
		string lastMessages;
		bool wasCapturing = false;
	};
}



unique_ptr<AccessReader> CreateInfoReader(Panel &panel, const string &id)
{
	if(id == "PlayerInfoPanel")
		return make_unique<PlayerInfoReader>(panel);
	if(id == "ShipInfoPanel")
		return make_unique<ShipInfoReader>(panel);
	if(id == "MessageLogPanel")
		return make_unique<MessageLogReader>(panel);
	if(id == "LogbookPanel")
		return make_unique<LogbookReader>(panel);
	if(id == "BoardingPanel")
		return make_unique<BoardingReader>(panel);
	return nullptr;
}
