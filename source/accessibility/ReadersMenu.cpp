/* ReadersMenu.cpp
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

// Readers for the main menu, pilot loading, new pilot scenarios, preferences
// and gamerules.

#include "AccessReaders.h"

#include "AccessCapture.h"
#include "AccessContext.h"
#include "AccessFormat.h"
#include "AccessInspector.h"
#include "AccessSpeech.h"

#include "../Account.h"
#include "../Command.h"
#include "../Information.h"
#include "../Planet.h"
#include "../PilotProfile.h"
#include "../PlayerInfo.h"
#include "../SavedGame.h"
#include "../Ship.h"
#include "../StartConditions.h"
#include "../System.h"
#include "../audio/Audio.h"
#include "../audio/SoundCategory.h"
#include "../text/Format.h"

#include <cmath>

using namespace std;

namespace {
	string PilotSummary(PlayerInfo &player)
	{
		if(!player.IsLoaded())
			return "No pilot loaded.";
		vector<string> parts;
		parts.push_back("Pilot " + player.FirstName() + " " + player.LastName());
		if(player.IsDead())
			parts.push_back("dead");
		if(player.Flagship())
			parts.push_back("flagship " + player.Flagship()->GivenName());
		if(player.GetPlanet())
			parts.push_back("on " + player.GetPlanet()->DisplayName());
		if(player.GetSystem())
			parts.push_back("in the " + player.GetSystem()->DisplayName() + " system");
		parts.push_back(player.GetDate().ToString());
		parts.push_back(AccessFormat::Credits(player.Accounts().Credits()));
		return AccessFormat::List(parts) + ".";
	}



	class MenuReader : public AccessReader {
	public:
		using AccessReader::AccessReader;

		string Title() override { return "Main menu"; }

		string Summary() override
		{
			string text = PilotSummary(Player());
			vector<string> keys = ButtonKeys();
			if(!keys.empty())
				text += " " + AccessFormat::List(keys) + ".";
			return text;
		}

		vector<string> Keys() override
		{
			vector<string> keys = ButtonKeys();
			keys.push_back("Space pauses the credits, Up and Down change their speed");
			return keys;
		}
	};



	class LoadReader : public AccessReader {
	public:
		LoadReader(Panel &panel) : AccessReader(panel), load(static_cast<LoadPanel &>(panel)) {}

		string Title() override { return "Load and save"; }

		string Summary() override
		{
			const auto &pilots = AccessInspector::Pilots(load);
			string text = AccessFormat::Plural(static_cast<int64_t>(pilots.size()), "pilot") + ". ";
			text += Selection(true);
			return text;
		}

		vector<string> Keys() override
		{
			return {"Left arrow: pilot list. Right arrow: saved games of the selected pilot",
				"Up and Down arrows choose", "L or E load the selected game", "N new pilot",
				"A add a snapshot of the current game", "D delete the selected pilot",
				"Shift R delete the selected snapshot", "G gamerules of the selected pilot",
				"O open the saves folder", "B or Escape go back"};
		}

		void PostKey(int32_t key, uint16_t mod) override
		{
			if(key == SDLK_UP || key == SDLK_DOWN)
				Say(Selection(false));
			else if(key == SDLK_LEFT)
				Say("Pilots. " + Selection(false));
			else if(key == SDLK_RIGHT)
				Say("Saved games. " + Selection(false));
		}

		void BuildReview(vector<ReviewGroup> &groups) override
		{
			ReviewGroup pilots{"Pilots", {}};
			for(const auto &pilot : AccessInspector::Pilots(load))
			{
				string line = pilot->Identifier() + ", " + AccessFormat::Plural(
					static_cast<int64_t>(pilot->Files().size()), "saved game");
				if(pilot->IsLocked())
					line += ", locked";
				pilots.items.emplace_back(line);
			}
			groups.push_back(std::move(pilots));
			AddScreenText(groups);
		}

	private:
		string Selection(bool both)
		{
			const auto &pilot = AccessInspector::SelectedPilot(load);
			if(!pilot)
				return "No pilot selected. Press N to create a new pilot.";
			const auto &pilots = AccessInspector::Pilots(load);
			size_t index = 0;
			for(size_t i = 0; i < pilots.size(); ++i)
				if(pilots[i] == pilot)
					index = i;
			string pilotText = "Pilot " + pilot->Identifier() + ", " + to_string(index + 1) + " of "
				+ to_string(pilots.size()) + ", " + AccessFormat::Plural(static_cast<int64_t>(pilot->Files().size()),
					"saved game");
			if(pilot->IsLocked())
				pilotText += ", locked";
			string file = AccessInspector::SelectedFile(load);
			string fileText;
			if(!file.empty())
			{
				size_t pos = file.find('~') + 1;
				string name = file.size() > pos + 4 ? file.substr(pos, file.size() - 4 - pos) : file;
				const SavedGame &info = AccessInspector::LoadedInfo(load);
				const auto &files = pilot->Files();
				size_t fileIndex = 0;
				for(size_t i = 0; i < files.size(); ++i)
					if(files[i].first == file)
						fileIndex = i;
				fileText = "Save " + name + ", " + to_string(fileIndex + 1) + " of " + to_string(files.size());
				if(!info.IsEmpty())
				{
					fileText += ". " + info.Name() + ", " + info.GetDate() + ", " + info.Credits() + " credits";
					if(!info.GetPlanet().empty())
						fileText += ", on " + info.GetPlanet();
					if(!info.GetSystem().empty())
						fileText += ", in " + info.GetSystem();
					if(!info.ShipName().empty())
						fileText += ", flagship " + info.ShipName();
					if(!info.GetPlayTime().empty())
						fileText += ", played " + info.GetPlayTime();
				}
			}
			bool side = AccessInspector::SideHasFocus(load);
			if(both)
				return pilotText + ". " + fileText + (side ? ". Focus on pilot list." : ". Focus on saved games.");
			return side ? pilotText + (fileText.empty() ? "" : ". " + fileText) : (fileText.empty() ? "No saved games"
				: fileText);
		}

	private:
		LoadPanel &load;
	};



	class StartReader : public AccessReader {
	public:
		StartReader(Panel &panel) : AccessReader(panel), start(static_cast<StartConditionsPanel &>(panel)) {}

		string Title() override { return "New pilot: choose a starting scenario"; }

		string Summary() override
		{
			return Scenario();
		}

		vector<string> Keys() override
		{
			return {"Up and Down arrows or Page Up and Page Down choose a scenario",
				"Enter or S start with the selected scenario", "G gamerules", "B or Escape go back"};
		}

		void PostKey(int32_t key, uint16_t mod) override
		{
			if(key == SDLK_UP || key == SDLK_DOWN || key == SDLK_PAGEUP || key == SDLK_PAGEDOWN)
				Later(1, [this]() { Say(Scenario()); });
		}

	private:
		string Scenario()
		{
			if(!AccessInspector::HasScenario(start))
				return "No scenarios available.";
			const Information &info = AccessInspector::ScenarioInfo(start);
			string text = info.GetString("name") + ", " + to_string(AccessInspector::ScenarioIndex(start) + 1)
				+ " of " + to_string(AccessInspector::ScenarioCount(start)) + ".";
			if(!info.HasCondition("unlocked start"))
				text += " Locked.";
			vector<string> parts;
			if(!info.GetString("planet").empty())
				parts.push_back("starts on " + info.GetString("planet"));
			if(!info.GetString("system").empty())
				parts.push_back("in the " + info.GetString("system") + " system");
			if(!info.GetString("date").empty())
				parts.push_back(info.GetString("date"));
			if(!info.GetString("credits").empty())
				parts.push_back(info.GetString("credits") + " credits");
			if(!info.GetString("debt").empty())
				parts.push_back("debt " + info.GetString("debt"));
			if(!parts.empty())
				text += " " + AccessFormat::Sentence(AccessFormat::List(parts)) + ".";
			string description = AccessInspector::ScenarioDescription(start);
			if(description.empty())
				description = info.GetString("description");
			if(!description.empty())
				text += " " + description;
			return text;
		}

	private:
		StartConditionsPanel &start;
	};



	class PreferencesReader : public AccessReader {
	public:
		PreferencesReader(Panel &panel) : AccessReader(panel), prefs(static_cast<PreferencesPanel &>(panel)) {}

		string Title() override { return "Preferences"; }

		string Summary() override
		{
			return PageName() + ". " + Current();
		}

		vector<string> Keys() override
		{
			return {"C controls, S settings, P plugins, A audio pages",
				"Up and Down arrows choose an entry", "Enter changes it; on the controls page Enter waits for the new key",
				"X or Delete clears a key binding", "N or Page Up and R or Page Down change page",
				"On the audio page, Left and Right arrows change the selected volume",
				"B or Escape go back"};
		}

		bool WantsRawKeys() override
		{
			return AccessInspector::PreferencesEditing(prefs) >= 0;
		}

		bool PreKey(int32_t key, uint16_t mod) override
		{
			// The audio page has only mouse-operated volume bars in the game;
			// give them keyboard control with the same effect as a click.
			if(AccessInspector::PreferencesPage(prefs) != 'a')
				return false;
			static const vector<pair<string, SoundCategory>> BARS = {
				{"Master volume", SoundCategory::MASTER}, {"Music", SoundCategory::MUSIC},
				{"Interface", SoundCategory::UI}, {"Anti-missile", SoundCategory::ANTI_MISSILE},
				{"Weapons", SoundCategory::WEAPON}, {"Engines", SoundCategory::ENGINE},
				{"Afterburner", SoundCategory::AFTERBURNER}, {"Jump", SoundCategory::JUMP},
				{"Explosions", SoundCategory::EXPLOSION}, {"Scanning", SoundCategory::SCAN},
				{"Environment", SoundCategory::ENVIRONMENT}, {"Alerts", SoundCategory::ALERT}};
			int count = static_cast<int>(BARS.size());
			if(key == SDLK_UP || key == SDLK_DOWN)
				audioIndex = (audioIndex + (key == SDLK_DOWN ? 1 : -1) + count) % count;
			else if(key == SDLK_LEFT || key == SDLK_RIGHT)
			{
				SoundCategory category = BARS[static_cast<size_t>(audioIndex)].second;
				double volume = Audio::Volume(category) + (key == SDLK_RIGHT ? .1 : -.1);
				volume = round(max(0., min(1., volume)) * 10.) / 10.;
				Audio::SetVolume(volume, category);
				Audio::Play(Audio::Get("warder"), category);
			}
			else
				return false;
			const auto &bar = BARS[static_cast<size_t>(audioIndex)];
			Say(bar.first + ", " + AccessFormat::Percent(Audio::Volume(bar.second)));
			return true;
		}

		void PostKey(int32_t key, uint16_t mod) override
		{
			char page = AccessInspector::PreferencesPage(prefs);
			if(page == 'a')
			{
				if(key == 'a')
					Say("Audio. Up and Down choose a volume, Left and Right change it.");
				return;
			}
			bool pageChange = key == 'c' || key == 's' || key == 'p' || key == 'n' || key == 'r'
				|| key == SDLK_PAGEUP || key == SDLK_PAGEDOWN;
			if(wasEditing)
			{
				wasEditing = false;
				Later(1, [this]() { Say("Bound. " + Current()); });
				return;
			}
			if(AccessInspector::PreferencesEditing(prefs) >= 0)
			{
				wasEditing = true;
				Say("Press the new key for " + Current());
				return;
			}
			if(pageChange)
				Later(2, [this]() { Say(PageName() + ". " + Current()); });
			else if(key == SDLK_UP || key == SDLK_DOWN)
				Later(1, [this]() { Say(Current()); });
			else if(key == SDLK_RETURN || key == 'x' || key == SDLK_DELETE)
				Later(2, [this]() { Say(Current()); });
		}

		void BuildReview(vector<ReviewGroup> &groups) override
		{
			AddScreenText(groups);
		}

	private:
		string PageName()
		{
			switch(AccessInspector::PreferencesPage(prefs))
			{
				case 'c': return "Controls page " + to_string(AccessInspector::ControlsPage(prefs) + 1);
				case 's': return "Settings page " + to_string(AccessInspector::SettingsPage(prefs) + 1);
				case 'p': return "Plugins";
				case 'a': return "Audio";
				default: return "Preferences";
			}
		}

		string Current()
		{
			int selected = AccessInspector::PreferencesSelected(prefs);
			char page = AccessInspector::PreferencesPage(prefs);
			if(page == 'c')
			{
				vector<Command> commands = AccessInspector::ControlZones(prefs);
				if(selected < 0 || selected >= static_cast<int>(commands.size()))
					return string();
				const Command &command = commands[static_cast<size_t>(selected)];
				string text = command.Description() + ": " + (command.HasBinding()
					? AccessFormat::FriendlyKey(command.KeyName()) : "unassigned");
				if(command.HasConflict())
					text += ", conflicts with another command";
				return text;
			}
			// Read the row as it is displayed (label and current value).
			string line = AccessText::TextIn(panel, AccessInspector::PreferencesRow(prefs, selected));
			if(page == 'p')
			{
				string name = AccessInspector::PreferencesSelectedPlugin(prefs);
				return line.empty() ? name : line;
			}
			return line.empty() ? AccessInspector::PreferencesSelectedItem(prefs) : line;
		}

	private:
		PreferencesPanel &prefs;
		bool wasEditing = false;
		int audioIndex = 0;
	};



	class GamerulesReader : public AccessReader {
	public:
		GamerulesReader(Panel &panel) : AccessReader(panel), rules(static_cast<GamerulesPanel &>(panel)) {}

		string Title() override { return "Gamerules"; }

		string Summary() override
		{
			return PageName() + ". " + Current();
		}

		vector<string> Keys() override
		{
			return {"G rules page, P presets page", "Up and Down arrows choose", "Enter changes or selects",
				"X or Delete resets a rule", "N or Page Up and R or Page Down change page", "B or Escape go back"};
		}

		void PostKey(int32_t key, uint16_t mod) override
		{
			if(key == 'g' || key == 'p' || key == 'n' || key == 'r' || key == SDLK_PAGEUP || key == SDLK_PAGEDOWN)
				Later(2, [this]() { Say(PageName() + ". " + Current()); });
			else if(key == SDLK_UP || key == SDLK_DOWN)
				Later(1, [this]() { Say(Current()); });
			else if(key == SDLK_RETURN || key == 'x' || key == SDLK_DELETE)
				Later(2, [this]() { Say(Current()); });
		}

	private:
		string PageName()
		{
			if(AccessInspector::GamerulesPage(rules) == 'p')
				return "Presets";
			return "Rules page " + to_string(AccessInspector::GamerulesPageNumber(rules) + 1);
		}

		string Current()
		{
			int selected = AccessInspector::GamerulesSelected(rules);
			string line = AccessText::TextIn(panel, AccessInspector::GamerulesRow(rules, selected));
			if(line.empty())
				line = AccessInspector::GamerulesPage(rules) == 'p' ? AccessInspector::SelectedPreset(rules)
					: AccessInspector::GamerulesSelectedItem(rules);
			return line;
		}

	private:
		GamerulesPanel &rules;
	};
}



unique_ptr<AccessReader> CreateMenuReader(Panel &panel, const string &id)
{
	if(id == "MenuPanel")
		return make_unique<MenuReader>(panel);
	if(id == "LoadPanel")
		return make_unique<LoadReader>(panel);
	if(id == "StartConditionsPanel")
		return make_unique<StartReader>(panel);
	if(id == "PreferencesPanel")
		return make_unique<PreferencesReader>(panel);
	if(id == "GamerulesPanel")
		return make_unique<GamerulesReader>(panel);
	return nullptr;
}
