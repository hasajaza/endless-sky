/* Accessibility.cpp
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

#include "Accessibility.h"

#include "AccessAudio.h"
#include "AccessCapture.h"
#include "AccessContext.h"
#include "AccessCrash.h"
#include "AccessFlight.h"
#include "AccessFormat.h"
#include "AccessInspector.h"
#include "AccessKeys.h"
#include "AccessReader.h"
#include "AccessSpeech.h"

#include "../Command.h"
#include "../DataFile.h"
#include "../DataNode.h"
#include "../DataWriter.h"
#include "../Engine.h"
#include "../Files.h"
#include "../MainPanel.h"
#include "../Panel.h"
#include "../PlayerInfo.h"
#include "../SDL.h"
#include "../UI.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <chrono>
#include <deque>
#include <map>
#include <mutex>

using namespace std;

namespace {
	PlayerInfo *player = nullptr;
	UI *menuUI = nullptr;
	UI *gameUI = nullptr;
	bool initialized = false;

	// One reader per panel currently on either stack.
	// The weak pointer detects a panel that was deleted and whose address was
	// reused by a new panel of a different type.
	struct ReaderEntry {
		weak_ptr<Panel> owner;
		unique_ptr<AccessReader> reader;
	};
	map<const Panel *, ReaderEntry> readers;
	const Panel *lastTop = nullptr;
	// A newly shown panel is announced once it has been drawn (so that its
	// buttons are known), or after a few frames at most.
	Panel *pendingOpen = nullptr;
	bool pendingIsReturn = false;
	int pendingOpenFrames = 0;
	// The panel that was active when the most recent key was pressed.
	const Panel *keyPanel = nullptr;

	// Review cursor.
	vector<ReviewGroup> review;
	string reviewTitle;
	const Panel *reviewPanel = nullptr;
	weak_ptr<Panel> reviewOwner;
	bool reviewIsCustom = false;
	int reviewGroup = 0;
	int reviewItem = -1;
	chrono::steady_clock::time_point reviewTime;

	// Commands waiting until it is safe to read the engine and until the
	// player has released the modifier keys.
	deque<AccessAction> pendingActions;
	bool pendingActivate = false;

	bool lastFastForward = false;
	// Frames until the settings are saved (after an engine change completes).
	int pendingSave = 0;
	bool audioCues = true;
	// Speak the chatter of other ships ("Merchant ship ...: ...").
	bool shipChatter = true;
	int missionIndex = -1;
	int messageIndex = 0;
	chrono::steady_clock::time_point lastMessageCommand;

	mutex messageMutex;
	deque<string> recentMessages;

	filesystem::path SettingsPath()
	{
		return Files::Config() / "accessibility.txt";
	}

	filesystem::path KeysPath()
	{
		return Files::Config() / "accessibility keys.txt";
	}

	void LoadSettings()
	{
		if(!Files::Exists(SettingsPath()))
			return;
		DataFile file(SettingsPath());
		for(const DataNode &node : file)
		{
			if(node.Size() < 2)
				continue;
			const string &key = node.Token(0);
			if(key == "verbosity")
				AccessSpeech::SetVerbosity(static_cast<int>(node.Value(1)));
			else if(key == "audio cues")
				audioCues = node.Value(1) != 0.;
			else if(key == "ship chatter")
				shipChatter = node.Value(1) != 0.;
			else if(key == "arrival pause")
				AccessFlight::SetArrivalPause(node.Value(1) != 0.);
			else if(key == "cue volume")
				AccessAudio::SetVolume(node.Value(1));
			else if(key == "sapi fallback")
				AccessSpeech::SetSapiFallback(node.Value(1) != 0.);
			else if(key == "speech engine")
				AccessSpeech::SetForcedEngine(static_cast<int>(node.Value(1)));
		}
	}

	shared_ptr<Panel> FindShared(const Panel *panel)
	{
		for(UI *ui : {menuUI, gameUI})
			if(ui && !ui->IsEmpty())
			{
				for(const auto &it : ui->Stack())
					if(it.get() == panel)
						return it;
				shared_ptr<Panel> top = ui->Top();
				if(top.get() == panel)
					return top;
			}
		return nullptr;
	}

	AccessReader *LiveReader(const Panel *panel)
	{
		auto it = readers.find(panel);
		if(it == readers.end())
			return nullptr;
		if(it->second.owner.lock().get() != panel)
		{
			readers.erase(it);
			return nullptr;
		}
		return it->second.reader.get();
	}

	AccessReader *ReaderFor(Panel *panel)
	{
		if(!panel)
			return nullptr;
		AccessReader *existing = LiveReader(panel);
		if(existing)
			return existing;
		shared_ptr<Panel> shared = FindShared(panel);
		if(!shared)
			return nullptr;
		ReaderEntry entry{shared, CreateAccessReader(*panel)};
		auto result = readers.emplace(panel, std::move(entry));
		return result.first->second.reader.get();
	}

	bool IsOnStack(const Panel *panel)
	{
		for(UI *ui : {menuUI, gameUI})
			if(ui)
				for(const auto &it : ui->Stack())
					if(it.get() == panel)
						return true;
		return false;
	}

	void CollectReaders()
	{
		for(auto it = readers.begin(); it != readers.end(); )
		{
			bool alive = it->second.owner.lock().get() == it->first;
			if(!alive || (!IsOnStack(it->first) && it->first != lastTop))
				it = readers.erase(it);
			else
				++it;
		}
	}

	MainPanel *FindMain()
	{
		if(!gameUI || gameUI->IsEmpty())
			return nullptr;
		shared_ptr<Panel> root = gameUI->Root();
		if(!root || string(root->AccessibilityId()) != "MainPanel")
			return nullptr;
		return static_cast<MainPanel *>(root.get());
	}

	// Make sure the engine is idle before reading it from outside FlightStep.
	void SettleEngine()
	{
		MainPanel *main = FindMain();
		if(main)
			AccessInspector::GetEngine(*main).Wait();
	}

	// Context help is a short list: the first press describes the screen,
	// every further press reads the next key, and the previous-key command
	// goes back. It starts over when the screen changes.
	const Panel *helpPanel = nullptr;
	int helpIndex = -1;
	vector<string> helpKeys;

	string HelpSummary(AccessReader *reader)
	{
		vector<string> parts;
		if(reader)
		{
			parts.push_back(reader->Title());
			parts.push_back(reader->Summary());
		}
		parts.push_back(AccessFormat::Plural(static_cast<int64_t>(helpKeys.size()), "key")
			+ ". Press " + AccessKeys::KeyName(AccessAction::CONTEXT_HELP) + " to hear them one at a time, and "
			+ AccessKeys::KeyName(AccessAction::CONTEXT_HELP_PREVIOUS) + " to go back");
		string result;
		for(const string &part : parts)
			if(!part.empty())
				result += AccessFormat::TrimPeriod(part) + ". ";
		return result;
	}

	string ContextHelp(int step)
	{
		Panel *top = AccessContext::TopPanel();
		AccessReader *reader = ReaderFor(top);
		if(top != helpPanel || (step > 0 && helpIndex < -1))
		{
			bool restart = (top == helpPanel);
			helpPanel = top;
			helpKeys = reader ? reader->Keys() : vector<string>();
			helpKeys.push_back(AccessKeys::KeyName(AccessAction::KEY_REFERENCE)
				+ " loads every key, including the accessibility and game keys, into the review list");
			helpIndex = -1;
			if(step > 0 || !restart)
				return HelpSummary(reader);
		}
		int count = static_cast<int>(helpKeys.size());
		if(step < 0 && helpIndex == -2)
			helpIndex = count;
		helpIndex += step;
		if(helpIndex >= count)
		{
			helpIndex = -2;
			return "End of the list. Press " + AccessKeys::KeyName(AccessAction::CONTEXT_HELP)
				+ " to start again.";
		}
		if(helpIndex < 0)
		{
			helpIndex = -1;
			return HelpSummary(reader);
		}
		return AccessFormat::TrimPeriod(helpKeys[helpIndex]) + ". " + to_string(helpIndex + 1) + " of "
			+ to_string(count) + ".";
	}

	vector<ReviewGroup> KeyReference()
	{
		vector<ReviewGroup> groups;
		AccessReader *reader = ReaderFor(AccessContext::TopPanel());
		if(reader)
		{
			ReviewGroup screen;
			screen.name = "Keys for " + reader->Title();
			for(const string &line : reader->Keys())
				screen.items.emplace_back(line);
			if(!screen.items.empty())
				groups.push_back(std::move(screen));
		}

		ReviewGroup access;
		access.name = "Accessibility keys";
		for(const AccessKeys::Info &info : AccessKeys::All())
			access.items.emplace_back(string(info.description) + ": " + AccessKeys::KeyName(info.action));
		groups.push_back(std::move(access));

		// The game's own commands, with the keys currently bound to them.
		static const vector<Command> COMMANDS = {
			Command::FORWARD, Command::LEFT, Command::RIGHT, Command::BACK, Command::AUTOSTEER,
			Command::AFTERBURNER, Command::PRIMARY, Command::SECONDARY, Command::SELECT, Command::TURRET_TRACKING,
			Command::LAND, Command::BOARD, Command::HAIL, Command::SCAN, Command::JUMP, Command::FLEET_JUMP,
			Command::TARGET, Command::NEAREST, Command::NEAREST_ASTEROID, Command::DEPLOY, Command::CLOAK,
			Command::MAP, Command::INFO, Command::MESSAGE_LOG, Command::FULLSCREEN, Command::FASTFORWARD,
			Command::HELP, Command::PAUSE, Command::MENU, Command::FIGHT, Command::HOLD_FIRE, Command::GATHER,
			Command::HOLD_POSITION, Command::AMMO, Command::HARVEST, Command::SCAN_ORDER};
		ReviewGroup game;
		game.name = "Game keys (from your keyboard settings)";
		for(const Command &command : COMMANDS)
		{
			string name = command.HasBinding() ? AccessFormat::FriendlyKey(command.KeyName()) : "unassigned";
			game.items.emplace_back(command.Description() + ": " + name);
		}
		game.items.emplace_back("Stop your ship: Shift plus " + AccessFormat::FriendlyKey(Command::BACK.KeyName()));
		game.items.emplace_back("Fleet jump: Shift plus " + AccessFormat::FriendlyKey(Command::JUMP.KeyName()));
		game.items.emplace_back("Select escort group: 0 to 9; Control plus a digit stores the selected escorts");
		game.items.emplace_back("Zoom: plus and minus");
		groups.push_back(std::move(game));
		return groups;
	}

	void SpeakReviewItem(bool withGroupName)
	{
		if(review.empty())
		{
			AccessSpeech::Say("Nothing to review on this screen.", SpeechPriority::REQUESTED);
			return;
		}
		reviewGroup = max(0, min(static_cast<int>(review.size()) - 1, reviewGroup));
		ReviewGroup &group = review[static_cast<size_t>(reviewGroup)];
		if(group.items.empty())
		{
			AccessSpeech::Say(group.name + ": empty.", SpeechPriority::REQUESTED);
			return;
		}
		reviewItem = max(0, min(static_cast<int>(group.items.size()) - 1, reviewItem));
		string text = group.items[static_cast<size_t>(reviewItem)].Text();
		if(withGroupName)
			text = group.name + ", " + to_string(group.items.size()) + " items. " + text;
		AccessSpeech::Say(text, SpeechPriority::REQUESTED);
	}

	void RebuildReview(bool force)
	{
		Panel *top = AccessContext::TopPanel();
		AccessReader *reader = ReaderFor(top);
		bool samePanel = (top == reviewPanel && reviewOwner.lock().get() == top);
		if(reviewIsCustom && samePanel && !force)
			return;
		bool keep = reader && reader->KeepReview() && samePanel && !force
			&& chrono::steady_clock::now() - reviewTime < chrono::seconds(20);
		if(keep && !review.empty())
			return;

		vector<ReviewGroup> groups;
		if(reader)
			reader->BuildReview(groups);
		// Drop empty groups, but keep at least one.
		groups.erase(remove_if(groups.begin(), groups.end(),
			[](const ReviewGroup &g) { return g.items.empty(); }), groups.end());
		if(!samePanel)
		{
			reviewGroup = 0;
			reviewItem = -1;
		}
		review = std::move(groups);
		reviewPanel = top;
		reviewOwner = FindShared(top);
		reviewIsCustom = false;
		reviewTime = chrono::steady_clock::now();
	}

	void Review(AccessAction action)
	{
		RebuildReview(false);
		if(review.empty())
		{
			AccessAudio::Play(AccessAudio::Cue::BOUNDARY);
			AccessSpeech::Say("Nothing to review on this screen.", SpeechPriority::REQUESTED);
			return;
		}
		reviewGroup = max(0, min(static_cast<int>(review.size()) - 1, reviewGroup));
		int count = static_cast<int>(review[static_cast<size_t>(reviewGroup)].items.size());
		bool boundary = false;
		bool announceGroup = false;
		switch(action)
		{
			case AccessAction::REVIEW_NEXT:
				if(reviewItem + 1 >= count)
					boundary = true;
				else
					++reviewItem;
				announceGroup = reviewItem == 0;
				break;
			case AccessAction::REVIEW_PREVIOUS:
				if(reviewItem <= 0)
				{
					boundary = reviewItem == 0;
					reviewItem = 0;
				}
				else
					--reviewItem;
				break;
			case AccessAction::REVIEW_FIRST:
				// Refresh the list: the first item of a fresh snapshot.
				RebuildReview(true);
				reviewItem = 0;
				announceGroup = true;
				break;
			case AccessAction::REVIEW_LAST:
				reviewItem = count - 1;
				break;
			case AccessAction::REVIEW_NEXT_GROUP:
			case AccessAction::REVIEW_PREVIOUS_GROUP:
			{
				int direction = action == AccessAction::REVIEW_NEXT_GROUP ? 1 : -1;
				int size = static_cast<int>(review.size());
				reviewGroup = (reviewGroup + direction + size) % size;
				reviewItem = 0;
				announceGroup = true;
				// Lists are refreshed when changing groups (distances change, and
				// groups may appear or disappear); keep the chosen group by name.
				if(!reviewIsCustom)
				{
					string wanted = review[static_cast<size_t>(reviewGroup)].name;
					int keepGroup = reviewGroup;
					RebuildReview(true);
					reviewGroup = min(keepGroup, static_cast<int>(review.size()) - 1);
					for(size_t i = 0; i < review.size(); ++i)
						if(review[i].name == wanted)
							reviewGroup = static_cast<int>(i);
				}
				break;
			}
			case AccessAction::REVIEW_CURRENT:
			{
				if(reviewItem < 0)
					reviewItem = 0;
				ReviewGroup &group = review[static_cast<size_t>(reviewGroup)];
				if(group.items.empty())
					break;
				reviewItem = min(reviewItem, static_cast<int>(group.items.size()) - 1);
				const ReviewItem &item = group.items[static_cast<size_t>(reviewItem)];
				string text = AccessFormat::TrimPeriod(item.Text());
				if(item.detail)
				{
					string detail = AccessFormat::TrimPeriod(item.detail());
					if(!detail.empty() && detail != text)
						text += ". " + detail;
				}
				text += ". Item " + to_string(reviewItem + 1) + " of " + to_string(group.items.size())
					+ " in " + group.name;
				if(item.activate)
					text += ". Press " + AccessKeys::KeyName(AccessAction::REVIEW_ACTIVATE) + " to activate";
				AccessSpeech::Say(text, SpeechPriority::REQUESTED);
				return;
			}
			default:
				break;
		}
		if(boundary)
			AccessAudio::Play(AccessAudio::Cue::BOUNDARY);
		SpeakReviewItem(announceGroup);
	}

	void ActivateReviewItem()
	{
		if(reviewPanel != AccessContext::TopPanel() || reviewOwner.lock().get() != reviewPanel)
		{
			AccessSpeech::Say("The screen has changed. Review it again before activating.", SpeechPriority::REQUESTED);
			review.clear();
			return;
		}
		if(review.empty() || reviewGroup >= static_cast<int>(review.size()))
		{
			AccessSpeech::Say("Nothing selected in the review cursor.", SpeechPriority::REQUESTED);
			return;
		}
		ReviewGroup &group = review[static_cast<size_t>(reviewGroup)];
		if(reviewItem < 0 || reviewItem >= static_cast<int>(group.items.size()))
		{
			AccessSpeech::Say("Nothing selected in the review cursor.", SpeechPriority::REQUESTED);
			return;
		}
		ReviewItem &item = group.items[static_cast<size_t>(reviewItem)];
		if(!item.activate)
		{
			AccessSpeech::Say("This item cannot be activated.", SpeechPriority::REQUESTED);
			return;
		}
		if(!item.activate())
			AccessAudio::Play(AccessAudio::Cue::BOUNDARY);
	}

	string MissionsReport()
	{
		if(!player || !player->IsLoaded())
			return "No pilot is loaded.";
		vector<const Mission *> visible;
		for(const Mission &mission : player->Missions())
			if(mission.IsVisible())
				visible.push_back(&mission);
		if(visible.empty())
			return "You have no active missions.";
		missionIndex = (missionIndex + 1) % static_cast<int>(visible.size());
		const Mission &mission = *visible[static_cast<size_t>(missionIndex)];
		string text = mission.DisplayName() + ", mission " + to_string(missionIndex + 1) + " of "
			+ to_string(visible.size()) + ". ";
		text += AccessFlight::MissionSummary(mission);
		return text;
	}

	string MessagesReport()
	{
		vector<string> messages = AccessContext::RecentMessages();
		if(messages.empty())
			return "No recent messages.";
		auto now = chrono::steady_clock::now();
		if(now - lastMessageCommand > chrono::seconds(8))
			messageIndex = 0;
		lastMessageCommand = now;
		if(messageIndex >= static_cast<int>(messages.size()))
		{
			messageIndex = 0;
			return "End of recent messages. " + messages.front();
		}
		string text = messages[static_cast<size_t>(messageIndex)];
		if(messageIndex == 0)
			text = "Latest: " + text;
		++messageIndex;
		return text;
	}

	// Run an information or review command. The engine must be idle.
	void Execute(AccessAction action)
	{
		switch(action)
		{
			case AccessAction::LOCATION:
				AccessSpeech::Say(AccessFlight::Location(), SpeechPriority::REQUESTED);
				break;
			case AccessAction::STATUS:
				AccessSpeech::Say(AccessFlight::Status(), SpeechPriority::REQUESTED);
				break;
			case AccessAction::TARGET:
				AccessSpeech::Say(AccessFlight::Target(), SpeechPriority::REQUESTED);
				break;
			case AccessAction::DESTINATION:
				AccessSpeech::Say(AccessFlight::Destination(), SpeechPriority::REQUESTED);
				break;
			case AccessAction::NEARBY:
				AccessSpeech::Say(AccessFlight::Nearby(), SpeechPriority::REQUESTED);
				break;
			case AccessAction::HOSTILES:
				AccessSpeech::Say(AccessFlight::Hostiles(), SpeechPriority::REQUESTED);
				break;
			case AccessAction::FLEET:
				AccessSpeech::Say(AccessFlight::Fleet(), SpeechPriority::REQUESTED);
				break;
			case AccessAction::MISSIONS:
				AccessSpeech::Say(MissionsReport(), SpeechPriority::REQUESTED);
				break;
			case AccessAction::CARGO:
				AccessSpeech::Say(AccessFlight::Cargo(), SpeechPriority::REQUESTED);
				break;
			case AccessAction::MESSAGES:
				AccessSpeech::Say(MessagesReport(), SpeechPriority::REQUESTED);
				break;
			case AccessAction::WEAPONS:
				AccessSpeech::Say(AccessFlight::Weapons(), SpeechPriority::REQUESTED);
				break;
			case AccessAction::MOTION:
				AccessSpeech::Say(AccessFlight::Motion(), SpeechPriority::REQUESTED);
				break;
			case AccessAction::BEACON:
				AccessSpeech::Say(AccessFlight::ToggleBeacon(), SpeechPriority::REQUESTED);
				break;
			case AccessAction::REVIEW_PREVIOUS:
			case AccessAction::REVIEW_NEXT:
			case AccessAction::REVIEW_FIRST:
			case AccessAction::REVIEW_LAST:
			case AccessAction::REVIEW_PREVIOUS_GROUP:
			case AccessAction::REVIEW_NEXT_GROUP:
			case AccessAction::REVIEW_CURRENT:
				Review(action);
				break;
			case AccessAction::REVIEW_ACTIVATE:
				ActivateReviewItem();
				break;
			case AccessAction::CONTEXT_HELP:
				AccessSpeech::Say(ContextHelp(1), SpeechPriority::REQUESTED);
				break;
			case AccessAction::CONTEXT_HELP_PREVIOUS:
				AccessSpeech::Say(ContextHelp(-1), SpeechPriority::REQUESTED);
				break;
			case AccessAction::KEY_REFERENCE:
				AccessContext::LoadReview("Key reference", KeyReference());
				AccessSpeech::Say("Key reference loaded into the review cursor. Use "
					+ AccessKeys::KeyName(AccessAction::REVIEW_NEXT) + " and "
					+ AccessKeys::KeyName(AccessAction::REVIEW_PREVIOUS) + " to read it, and "
					+ AccessKeys::KeyName(AccessAction::REVIEW_NEXT_GROUP) + " to change section.",
					SpeechPriority::REQUESTED);
				break;
			default:
				break;
		}
	}

	bool NeedsEngine(AccessAction action)
	{
		switch(action)
		{
			case AccessAction::REPEAT:
			case AccessAction::HISTORY_BACK:
			case AccessAction::HISTORY_FORWARD:
			case AccessAction::CYCLE_ENGINE:
			case AccessAction::VERBOSITY:
			case AccessAction::TOGGLE_CUES:
			case AccessAction::TOGGLE_CHATTER:
			case AccessAction::TOGGLE_ARRIVAL_PAUSE:
				return false;
			default:
				return true;
		}
	}

	bool ModifiersHeld()
	{
		return SDL_GetModState() & (KMOD_CTRL | KMOD_SHIFT | KMOD_ALT | KMOD_GUI);
	}

	void RunPending()
	{
		if(pendingActions.empty() && !pendingActivate)
			return;
		SettleEngine();
		while(!pendingActions.empty())
		{
			AccessAction action = pendingActions.front();
			pendingActions.pop_front();
			Execute(action);
		}
		// Activation waits until the modifiers are released, because some game
		// actions read the modifier keys (for example to buy 20 at a time).
		if(pendingActivate && !ModifiersHeld())
		{
			pendingActivate = false;
			Execute(AccessAction::REVIEW_ACTIVATE);
		}
	}

	void HandleAction(AccessAction action)
	{
		switch(action)
		{
			case AccessAction::REPEAT:
				AccessSpeech::RepeatLast();
				return;
			case AccessAction::HISTORY_BACK:
				AccessSpeech::ReviewHistory(-1);
				return;
			case AccessAction::HISTORY_FORWARD:
				AccessSpeech::ReviewHistory(1);
				return;
			case AccessAction::CYCLE_ENGINE:
				AccessSpeech::CycleEngine();
				pendingSave = 30;
				return;
			case AccessAction::VERBOSITY:
			{
				int level = (AccessSpeech::Verbosity() + 1) % 3;
				AccessSpeech::SetVerbosity(level);
				static const char *NAMES[3] = {"minimal: only alerts and important events",
					"normal", "verbose: all game messages and more detail"};
				AccessSpeech::Say(string("Verbosity ") + NAMES[level], SpeechPriority::REQUESTED);
				AccessContext::SaveSettings();
				return;
			}
			case AccessAction::TOGGLE_CUES:
				audioCues = !audioCues;
				if(audioCues && !AccessAudio::IsAvailable())
					AccessSpeech::Say("Audio cues on, but the sound system could not be started, so they cannot be "
						"heard.", SpeechPriority::REQUESTED);
				else
					AccessSpeech::Say(audioCues ? "Audio cues on: sounds for shield and hull hits, a chime when your "
						"target comes into weapon range, and the beacon (Control+B)." : "Audio cues off.",
						SpeechPriority::REQUESTED);
				AccessContext::SaveSettings();
				return;
			case AccessAction::TOGGLE_CHATTER:
				shipChatter = !shipChatter;
				AccessSpeech::Say(shipChatter ? "Ship chatter on." : "Ship chatter off. It is still in the message log.",
					SpeechPriority::REQUESTED);
				AccessContext::SaveSettings();
				return;
			case AccessAction::TOGGLE_ARRIVAL_PAUSE:
				AccessFlight::SetArrivalPause(!AccessFlight::ArrivalPause());
				AccessSpeech::Say(AccessFlight::ArrivalPause()
					? "Pause on arrival on: the game pauses in each new system. Press P to continue."
					: "Pause on arrival off.", SpeechPriority::REQUESTED);
				AccessContext::SaveSettings();
				return;
			case AccessAction::REVIEW_ACTIVATE:
				pendingActivate = true;
				return;
			default:
				break;
		}
		if(NeedsEngine(action))
			pendingActions.push_back(action);
	}

	chrono::steady_clock::time_point startTime = chrono::steady_clock::now();

	chrono::steady_clock::time_point lastFocusAnnouncement;

	void AnnounceFocusGained()
	{
		// The window gains focus when the game starts; that is not news. Focus
		// may also bounce several times in a row (screen readers, alt-tab).
		auto now = chrono::steady_clock::now();
		if(now - startTime < chrono::seconds(5) || now - lastFocusAnnouncement < chrono::seconds(3))
			return;
		Panel *top = AccessContext::TopPanel();
		if(top && string(top->AccessibilityId()) == "GameLoadingPanel")
			return;
		lastFocusAnnouncement = now;
		AccessReader *reader = ReaderFor(AccessContext::TopPanel());
		string text = "Endless Sky";
		if(reader)
			text += ", " + reader->Title();
		AccessSpeech::Say(text, SpeechPriority::FOCUS);
	}
}



// AccessContext implementation.
PlayerInfo &AccessContext::Player()
{
	return *player;
}



UI &AccessContext::ActiveUI()
{
	return menuUI->IsEmpty() ? *gameUI : *menuUI;
}



UI &AccessContext::MenuUI()
{
	return *menuUI;
}



UI &AccessContext::GameUI()
{
	return *gameUI;
}



MainPanel *AccessContext::Main()
{
	return FindMain();
}



Engine *AccessContext::GetEngine()
{
	MainPanel *main = FindMain();
	return main ? &AccessInspector::GetEngine(*main) : nullptr;
}



bool AccessContext::InFlight()
{
	MainPanel *main = FindMain();
	return main && menuUI->IsEmpty() && gameUI->Top().get() == main;
}



Panel *AccessContext::TopPanel()
{
	if(!menuUI || !gameUI)
		return nullptr;
	UI &ui = ActiveUI();
	if(ui.IsEmpty())
		return nullptr;
	// Skip decorative overlays (for example the menu animation) that do not
	// take the keyboard: the panel below them is the one being used.
	const auto &stack = ui.Stack();
	shared_ptr<Panel> top = ui.Top();
	if(top && !*top->AccessibilityId() && !top->TrapAllEvents())
		for(auto it = stack.rbegin(); it != stack.rend(); ++it)
			if(*(*it)->AccessibilityId() || (*it)->TrapAllEvents())
				return it->get();
	return top.get();
}



bool AccessContext::PressKey(int32_t key, uint16_t mod)
{
	SDL_Event event{};
	event.type = SDL_KEYDOWN;
#ifdef ES_USE_SDL3
	event.key.key = key;
	event.key.mod = mod;
	event.key.down = true;
#else
	event.key.keysym.sym = key;
	event.key.keysym.mod = mod;
	event.key.state = SDL_PRESSED;
#endif
	event.key.repeat = 0;
	Panel *before = TopPanel();
	keyPanel = before;
	bool handled = ActiveUI().Handle(event);
	Accessibility::AfterKey(key, mod);
	return handled;
}



void AccessContext::LoadReview(const string &title, vector<ReviewGroup> groups)
{
	review = std::move(groups);
	reviewTitle = title;
	reviewPanel = TopPanel();
	reviewOwner = FindShared(reviewPanel);
	reviewIsCustom = true;
	reviewGroup = 0;
	reviewItem = -1;
}



bool AccessContext::AudioCues()
{
	return audioCues && AccessAudio::IsAvailable();
}



void AccessContext::SetAudioCues(bool on)
{
	audioCues = on;
}



vector<string> AccessContext::RecentMessages()
{
	lock_guard<mutex> lock(messageMutex);
	return vector<string>(recentMessages.begin(), recentMessages.end());
}



void AccessContext::SaveSettings()
{
	DataWriter out(SettingsPath());
	out.WriteComment("Endless Sky accessibility settings.");
	out.Write("verbosity", AccessSpeech::Verbosity());
	out.Write("audio cues", audioCues ? 1 : 0);
	out.Write("cue volume", AccessAudio::Volume());
	out.Write("ship chatter", shipChatter ? 1 : 0);
	out.Write("arrival pause", AccessFlight::ArrivalPause() ? 1 : 0);
	out.Write("sapi fallback", AccessSpeech::UseSapiFallback() ? 1 : 0);
	out.WriteComment("Speech engine: -1 = automatic; otherwise the Universal Speech engine number.");
	out.Write("speech engine", AccessSpeech::ForcedEngine());
}



// Accessibility (game hooks).
void Accessibility::SetTestMode()
{
	AccessFlight::SetArrivalPause(false);
}



void Accessibility::Init(PlayerInfo &playerInfo, UI &menuPanels, UI &gamePanels)
{
	player = &playerInfo;
	menuUI = &menuPanels;
	gameUI = &gamePanels;

	filesystem::path executableDirectory;
	{
#ifdef ES_USE_SDL3
		const char *base = SDL_GetBasePath();
		if(base)
			executableDirectory = base;
#else
		char *base = SDL_GetBasePath();
		if(base)
		{
			executableDirectory = base;
			SDL_free(base);
		}
#endif
	}
	AccessCrash::Install(Files::Config() / "crash report.txt");
	LoadSettings();
	AccessSpeech::Init(executableDirectory, Files::Config() / "accessibility log.txt");
	AccessKeys::Load(KeysPath());
	// Rewrite the file so that it always lists every command (the player's own
	// choices were loaded first and are kept).
	AccessKeys::Save(KeysPath());
	AccessAudio::Init();
	initialized = true;

	AccessSpeech::Say("Endless Sky with accessibility. Loading. Press "
		+ AccessKeys::KeyName(AccessAction::CONTEXT_HELP) + " at any time for help.", SpeechPriority::IMPORTANT);
	for(const string &conflict : AccessKeys::Conflicts())
		AccessSpeech::Say(conflict, SpeechPriority::IMPORTANT);
}



void Accessibility::Quit()
{
	if(!initialized)
		return;
	readers.clear();
	review.clear();
	AccessAudio::Quit();
	AccessSpeech::Quit();
	initialized = false;
}



bool Accessibility::HandleEvent(const SDL_Event &event)
{
	if(!initialized)
		return false;

#ifdef ES_USE_SDL3
	if(event.type == SDL_EVENT_WINDOW_FOCUS_GAINED)
	{
		AnnounceFocusGained();
		return false;
	}
#else
	if(event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
	{
		AnnounceFocusGained();
		return false;
	}
#endif
	if(event.type == SDL_KEYUP)
	{
#ifdef ES_USE_SDL3
		AccessKeys::KeyReleased(event.key.key);
#else
		AccessKeys::KeyReleased(event.key.keysym.sym);
#endif
		return false;
	}
	if(event.type != SDL_KEYDOWN)
		return false;

#ifdef ES_USE_SDL3
	int32_t key = event.key.key;
	uint16_t mod = event.key.mod;
#else
	int32_t key = event.key.keysym.sym;
	uint16_t mod = event.key.keysym.mod;
#endif
	keyPanel = AccessContext::TopPanel();

	AccessAction action = AccessKeys::Match(key, mod);
	{
		AccessReader *top = ReaderFor(AccessContext::TopPanel());
		if(top && top->WantsRawKeys() && !(mod & (KMOD_CTRL | KMOD_ALT)))
			action = AccessAction::NONE;
		if(top && (mod & KMOD_CTRL))
		{
			vector<int32_t> used = top->CtrlKeysUsedByGame();
			if(find(used.begin(), used.end(), key) != used.end())
				action = AccessAction::NONE;
		}
	}
	if(action != AccessAction::NONE)
	{
		if(mod & KMOD_CTRL)
			AccessKeys::ChordPressed(key);
		// Holding a key repeats only the review cursor movements; every other
		// command runs once per press.
		bool isMovement = action == AccessAction::REVIEW_NEXT || action == AccessAction::REVIEW_PREVIOUS
			|| action == AccessAction::HISTORY_BACK || action == AccessAction::HISTORY_FORWARD;
		if(event.key.repeat && !isMovement)
			return true;
		HandleAction(action);
		return true;
	}

	AccessReader *reader = ReaderFor(AccessContext::TopPanel());
	if(reader && reader->PreKey(key, mod))
		return true;
	return false;
}



void Accessibility::AfterKey(int32_t key, uint16_t mod)
{
	if(!initialized)
		return;
	if(keyPanel && IsOnStack(keyPanel))
	{
		AccessReader *reader = LiveReader(keyPanel);
		if(reader)
			reader->PostKey(key, mod);
	}
	keyPanel = nullptr;
}



void Accessibility::Step(bool isFastForward)
{
	if(!initialized)
		return;

	Panel *top = AccessContext::TopPanel();
	if(top != lastTop)
	{
		AccessCrash::SetContext(top ? top->AccessibilityId() : "none");
		bool known = LiveReader(top) != nullptr;
		const Panel *previous = lastTop;
		lastTop = top;
		CollectReaders();
		if(top)
		{
			ReaderFor(top);
			// Leaving a custom review list (the key reference) on screen change.
			if(reviewIsCustom && reviewPanel != top)
				reviewIsCustom = false;
			pendingOpen = top;
			pendingIsReturn = known && previous;
			pendingOpenFrames = 0;
		}
		else
			pendingOpen = nullptr;
	}
	if(pendingOpen && pendingOpen == top)
	{
		const vector<const Panel *> drawn = AccessCapture::Panels();
		bool wasDrawn = find(drawn.begin(), drawn.end(), top) != drawn.end();
		if(wasDrawn || ++pendingOpenFrames > 3)
		{
			AccessReader *reader = ReaderFor(top);
			pendingOpen = nullptr;
			if(pendingIsReturn && reader->announced)
				reader->OnReturn();
			else
				reader->OnOpen();
			reader->announced = true;
		}
	}
	else if(top && !pendingOpen)
	{
		AccessReader *reader = ReaderFor(top);
		if(reader)
		{
			reader->RunDeferred();
			reader->Step();
		}
	}

	if(isFastForward != lastFastForward)
	{
		lastFastForward = isFastForward;
		AccessSpeech::Say(isFastForward ? "Fast forward on." : "Fast forward off.", SpeechPriority::FOCUS);
	}

	RunPending();
	if(pendingSave && !--pendingSave)
		AccessContext::SaveSettings();
}



void Accessibility::FlightStep(Engine &engine, const list<ShipEvent> &events, bool isActive)
{
	if(!initialized)
		return;
	AccessFlight::Step(engine, events, isActive);
}



namespace {
	// How often each long game message has been spoken this session.
	map<string, int> spokenCount;
}



void Accessibility::OnMessage(const string &text, const string &category)
{
	if(!initialized || text.empty())
		return;
	{
		lock_guard<mutex> lock(messageMutex);
		if(recentMessages.empty() || recentMessages.front() != text)
		{
			recentMessages.push_front(text);
			if(recentMessages.size() > 100)
				recentMessages.pop_back();
		}
	}

	if(AccessFlight::OnGameMessage(text))
		return;
	// Ship and planet chatter: "<government> ship "<name>": <text>".
	if(!shipChatter && category == "normal" && text.find(" ship \"") != string::npos
			&& text.find("\": ") != string::npos)
		return;
	// Long normal messages (first-time help repeated on every arrival) are
	// spoken twice per session; they stay in the message log (Control+E).
	if(category == "normal" && text.size() >= 60)
	{
		lock_guard<mutex> lock(messageMutex);
		if(++spokenCount[text] > 2)
			return;
	}

	int verbosity = AccessSpeech::Verbosity();
	bool important = (category == "high" || category == "high no repeat" || category == "high duplicating"
		|| category == "info");
	if(category == "daily")
	{
		if(verbosity >= 1)
			AccessSpeech::Say(text, SpeechPriority::INFO);
		return;
	}
	if(important)
		AccessSpeech::Say(text, SpeechPriority::IMPORTANT);
	else if(category == "low")
	{
		if(verbosity >= 2)
			AccessSpeech::Say(text, SpeechPriority::INFO);
	}
	else if(verbosity >= 1)
		AccessSpeech::Say(text, SpeechPriority::INFO);
}



// ReviewItem and AccessReader base implementation.
ReviewItem::ReviewItem(string fixedText, function<bool()> activate)
	: text([fixedText]() { return fixedText; }), activate(std::move(activate))
{
}



ReviewItem::ReviewItem(function<string()> text, function<bool()> activate)
	: text(std::move(text)), activate(std::move(activate))
{
}



string ReviewItem::Text() const
{
	return text ? text() : string();
}



AccessReader::AccessReader(Panel &panel)
	: panel(panel)
{
}



string AccessReader::Title()
{
	string id = panel.AccessibilityId();
	return id.empty() ? "Screen" : id;
}



void AccessReader::OnOpen()
{
	string summary = Summary();
	Say(Title() + (summary.empty() ? "" : ". " + summary), SpeechPriority::FOCUS);
}



void AccessReader::OnReturn()
{
	Say(Title(), SpeechPriority::FOCUS);
}



void AccessReader::Step()
{
}



bool AccessReader::PreKey(int32_t key, uint16_t mod)
{
	return false;
}



void AccessReader::PostKey(int32_t key, uint16_t mod)
{
}



string AccessReader::Summary()
{
	return string();
}



vector<string> AccessReader::Keys()
{
	return ButtonKeys();
}



void AccessReader::BuildReview(vector<ReviewGroup> &groups)
{
	AddScreenText(groups);
}



bool AccessReader::KeepReview() const
{
	return false;
}



bool AccessReader::WantsRawKeys()
{
	return false;
}



vector<int32_t> AccessReader::CtrlKeysUsedByGame()
{
	return {};
}



Panel &AccessReader::GetPanel() const
{
	return panel;
}



void AccessReader::RunDeferred()
{
	if(deferred.empty())
		return;
	vector<function<void()>> ready;
	for(auto it = deferred.begin(); it != deferred.end(); )
	{
		if(--it->first <= 0)
		{
			ready.push_back(std::move(it->second));
			it = deferred.erase(it);
		}
		else
			++it;
	}
	for(auto &callback : ready)
		callback();
}



void AccessReader::Later(int frames, function<void()> callback)
{
	deferred.emplace_back(frames, std::move(callback));
}



string AccessReader::ButtonLabel(int index, int count, const string &text, char key, bool active) const
{
	string label = text;
	while(label.size() > 3 && label.ends_with("..."))
		label.resize(label.size() - 3);
	if(!active)
		label += ", unavailable";
	if(key)
		label += ", " + string(1, static_cast<char>(toupper(static_cast<unsigned char>(key))));
	return label + ", " + to_string(index + 1) + " of " + to_string(count);
}



bool AccessReader::NavigateButtons(int32_t key)
{
	if(key != SDLK_UP && key != SDLK_DOWN && key != SDLK_HOME && key != SDLK_END && key != SDLK_RETURN
			&& key != SDLK_KP_ENTER)
		return false;
	// Buttons in reading order, without duplicates.
	vector<AccessCapture::Item> buttons;
	for(const AccessCapture::Item &item : AccessCapture::Buttons(&panel))
	{
		bool duplicate = false;
		for(const AccessCapture::Item &other : buttons)
			duplicate |= (other.text == item.text && other.key == item.key);
		if(!duplicate && item.key)
			buttons.push_back(item);
	}
	// The buttons stay in the order the screen's interface definition lists
	// them, which is the order the game designers intended.
	if(buttons.empty())
		return false;
	int count = static_cast<int>(buttons.size());
	if(key == SDLK_RETURN || key == SDLK_KP_ENTER)
	{
		if(buttonIndex < 0 || buttonIndex >= count)
		{
			Say("Use the Up and Down arrows to choose an option first.");
			return true;
		}
		const AccessCapture::Item &button = buttons[static_cast<size_t>(buttonIndex)];
		if(!button.isActive)
		{
			AccessAudio::Play(AccessAudio::Cue::BOUNDARY);
			Say(button.text + " is unavailable right now.");
			return true;
		}
		// Press the button's own key, exactly as if it had been typed.
		AccessContext::PressKey(static_cast<unsigned char>(button.key));
		return true;
	}
	int previous = buttonIndex;
	if(key == SDLK_HOME)
		buttonIndex = 0;
	else if(key == SDLK_END)
		buttonIndex = count - 1;
	else if(buttonIndex < 0 || buttonIndex >= count)
		buttonIndex = key == SDLK_DOWN ? 0 : count - 1;
	else
		buttonIndex = max(0, min(count - 1, buttonIndex + (key == SDLK_DOWN ? 1 : -1)));
	if(previous == buttonIndex)
		AccessAudio::Play(AccessAudio::Cue::BOUNDARY);
	const AccessCapture::Item &button = buttons[static_cast<size_t>(buttonIndex)];
	Say(ButtonLabel(buttonIndex, count, button.text, button.key, button.isActive));
	return true;
}



void AccessReader::Say(const string &text, SpeechPriority priority)
{
	AccessSpeech::Say(text, priority);
}



PlayerInfo &AccessReader::Player()
{
	return AccessContext::Player();
}



bool AccessReader::PressKey(int32_t key, uint16_t mod)
{
	return AccessContext::PressKey(key, mod);
}



void AccessReader::AddScreenText(vector<ReviewGroup> &groups, bool includeButtons) const
{
	ReviewGroup text;
	text.name = "Screen text";
	ReviewGroup buttons;
	buttons.name = "Buttons";
	for(const AccessCapture::Item &item : AccessCapture::Lines(&panel))
	{
		if(item.isButton && includeButtons)
		{
			char key = item.key;
			bool active = item.isActive;
			string label = item.text + (active ? "" : ", unavailable");
			if(key)
				label += ", key " + string(1, static_cast<char>(toupper(static_cast<unsigned char>(key))));
			buttons.items.emplace_back(label, [key, active]()
			{
				if(!active)
				{
					AccessSpeech::Say("That button is unavailable right now.", SpeechPriority::REQUESTED);
					return false;
				}
				return key ? AccessContext::PressKey(key) : false;
			});
		}
		else if(!item.isButton)
			text.items.emplace_back(item.text);
	}
	if(!text.items.empty())
		groups.push_back(std::move(text));
	if(!buttons.items.empty())
		groups.push_back(std::move(buttons));
}



vector<string> AccessReader::ButtonKeys() const
{
	vector<string> result;
	vector<string> seen;
	for(const AccessCapture::Item &item : AccessCapture::Buttons(&panel))
	{
		if(!item.key || find(seen.begin(), seen.end(), item.text) != seen.end())
			continue;
		seen.push_back(item.text);
		string line = string(1, static_cast<char>(toupper(static_cast<unsigned char>(item.key)))) + " "
			+ item.text;
		if(!item.isActive)
			line += " (unavailable)";
		result.push_back(line);
	}
	return result;
}
