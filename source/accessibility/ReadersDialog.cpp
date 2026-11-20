/* ReadersDialog.cpp
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

// Readers for dialogs, conversations and hails.

#include "AccessReaders.h"

#include "AccessAudio.h"
#include "AccessContext.h"
#include "AccessFormat.h"
#include "AccessInspector.h"
#include "AccessKeys.h"
#include "AccessSpeech.h"

#include "../SDL.h"

using namespace std;

namespace {
	class DialogReader : public AccessReader {
	public:
		DialogReader(Panel &panel) : AccessReader(panel), dialog(static_cast<DialogPanel &>(panel)) {}

		string Title() override
		{
			if(AccessInspector::IsMission(dialog))
				return "Mission offer";
			return AccessInspector::AcceptsInput(dialog) ? "Input dialog" : "Dialog";
		}

		void OnOpen() override
		{
			Say(Title() + ". " + AccessInspector::Message(dialog) + " " + Controls(), SpeechPriority::FOCUS);
			lastActive = AccessInspector::ActiveButton(dialog);
		}

		void OnReturn() override
		{
			OnOpen();
		}

		string Summary() override
		{
			return AccessInspector::Message(dialog) + " " + Controls();
		}

		vector<string> Keys() override
		{
			vector<string> keys;
			if(AccessInspector::AcceptsInput(dialog))
				keys.push_back("Type to enter text; Backspace deletes");
			keys.push_back("Enter or Space presses the selected button");
			keys.push_back("Tab, Left and Right arrows choose a button");
			if(AccessInspector::IsMission(dialog))
				keys.push_back("A accepts, D declines");
			if(AccessInspector::CanCancel(dialog))
				keys.push_back("Escape cancels");
			if(AccessInspector::HasSystem(dialog))
				keys.push_back("M shows the location on the map");
			keys.push_back("Control plus Up and Down arrows read the text line by line");
			return keys;
		}

		bool PreKey(int32_t key, uint16_t mod) override
		{
			before = AccessInspector::Input(dialog);
			lastActive = AccessInspector::ActiveButton(dialog);
			return false;
		}

		void PostKey(int32_t key, uint16_t mod) override
		{
			const string &after = AccessInspector::Input(dialog);
			if(AccessInspector::AcceptsInput(dialog) && after != before)
			{
				Say(AccessText::EchoEdit(before, after));
				return;
			}
			if(AccessInspector::AcceptsInput(dialog) && AccessText::IsTypingKey(key) && after == before
					&& key != SDLK_BACKSPACE && key != SDLK_DELETE)
			{
				Say("Not allowed here.");
				return;
			}
			if(key == SDLK_BACKSPACE && after.empty() && before.empty() && AccessInspector::AcceptsInput(dialog))
			{
				Say("Empty.");
				return;
			}
			int active = AccessInspector::ActiveButton(dialog);
			if(active != lastActive || key == SDLK_TAB || key == SDLK_LEFT || key == SDLK_RIGHT)
				Say(ButtonName(active) + ", button");
			else if((key == SDLK_RETURN || key == SDLK_KP_ENTER) && AccessInspector::IsOkDisabled(dialog)
					&& IsOnStack())
			{
				const string &input = AccessInspector::Input(dialog);
				Say("The value is not valid. Text field: " + (input.empty() ? string("blank") : input) + ".");
			}
			lastActive = active;
		}

		void BuildReview(vector<ReviewGroup> &groups) override
		{
			ReviewGroup text{"Dialog text", {}};
			for(const string &paragraph : AccessText::Paragraphs(AccessInspector::Message(dialog)))
				text.items.emplace_back(paragraph);
			if(AccessInspector::AcceptsInput(dialog))
				text.items.emplace_back([this]()
				{
					const string &input = AccessInspector::Input(dialog);
					return "Text field: " + (input.empty() ? string("blank") : input);
				});
			groups.push_back(std::move(text));
			ReviewGroup buttons{"Buttons", {}};
			int count = AccessInspector::NumButtons(dialog);
			for(int i = 1; i <= count; ++i)
				buttons.items.emplace_back([this, i]() { return ButtonName(i); }, [this, i]()
				{
					// Choose the button with the game's own keys, then press it.
					int guard = 4;
					while(AccessInspector::ActiveButton(dialog) != i && guard--)
						AccessContext::PressKey(SDLK_TAB);
					return AccessContext::PressKey(SDLK_RETURN);
				});
			groups.push_back(std::move(buttons));
		}

	private:
		bool IsOnStack()
		{
			return AccessContext::TopPanel() == &panel;
		}

		string ButtonName(int index)
		{
			string name;
			if(index == 1)
				name = AccessInspector::OkText(dialog);
			else if(index == 2)
				name = AccessInspector::CancelText(dialog);
			else
				name = AccessInspector::ThirdText(dialog);
			if(index == 1 && AccessInspector::IsOkDisabled(dialog))
				name += ", unavailable";
			return name.empty() ? "OK" : name;
		}

		string Controls()
		{
			string text;
			if(AccessInspector::AcceptsInput(dialog))
			{
				const string &input = AccessInspector::Input(dialog);
				text += AccessInspector::IsNumeric(dialog) ? "Number field" : "Text field";
				text += input.empty() ? ", blank. " : ", contains " + input + ". ";
			}
			int count = AccessInspector::NumButtons(dialog);
			vector<string> names;
			for(int i = 1; i <= count; ++i)
				names.push_back(ButtonName(i));
			if(count > 1)
				text += "Buttons: " + AccessFormat::List(names) + ". Selected: "
					+ ButtonName(AccessInspector::ActiveButton(dialog)) + ".";
			else
				text += "Press Enter for " + ButtonName(1) + ".";
			if(AccessInspector::IsMission(dialog))
				text += " A to accept, D to decline.";
			return text;
		}

	private:
		DialogPanel &dialog;
		string before;
		int lastActive = 1;
	};



	class ConversationReader : public AccessReader {
	public:
		ConversationReader(Panel &panel) : AccessReader(panel), conversation(static_cast<ConversationPanel &>(panel)) {}

		string Title() override { return "Conversation"; }

		void OnOpen() override
		{
			spoken = 0;
			lastNode = -2;
			Announce(true);
		}

		void OnReturn() override
		{
			Say("Conversation. " + CurrentState(), SpeechPriority::FOCUS);
		}

		void Step() override
		{
			Announce(false);
		}

		string Summary() override
		{
			vector<string> texts = AccessInspector::Texts(conversation);
			string text = texts.empty() ? string() : texts.back() + " ";
			return text + CurrentState();
		}

		vector<string> Keys() override
		{
			if(IsNameEntry())
				return {"Type your name", "Tab switches between first and last name", "Enter confirms"};
			if(AccessInspector::Node(conversation) < 0)
				return {"Enter or D closes the conversation"};
			return {"Up and Down arrows choose a response", "Enter or the number keys 1 to 9 select it",
				"Page Up and Page Down read earlier paragraphs, Home and End jump to the first or latest",
				"M shows the map if a destination is involved"};
		}

		bool PreKey(int32_t key, uint16_t mod) override
		{
			if(IsNameEntry())
			{
				nameBefore = CurrentName();
				fieldBefore = AccessInspector::NameChoice(conversation);
				return false;
			}
			// Paragraph review (the game uses these keys only with a mouse wheel).
			if(key == SDLK_PAGEUP || key == SDLK_PAGEDOWN || key == SDLK_HOME || key == SDLK_END)
			{
				vector<string> texts = AccessInspector::Texts(conversation);
				if(texts.empty())
					return true;
				int count = static_cast<int>(texts.size());
				if(reviewIndex < 0 || reviewIndex >= count)
					reviewIndex = count - 1;
				if(key == SDLK_PAGEUP)
					reviewIndex = max(0, reviewIndex - 1);
				else if(key == SDLK_PAGEDOWN)
					reviewIndex = min(count - 1, reviewIndex + 1);
				else if(key == SDLK_HOME)
					reviewIndex = 0;
				else
					reviewIndex = count - 1;
				Say("Paragraph " + to_string(reviewIndex + 1) + " of " + to_string(count) + ". "
					+ texts[static_cast<size_t>(reviewIndex)], SpeechPriority::REQUESTED);
				return true;
			}
			lastChoice = AccessInspector::Choice(conversation);
			return false;
		}

		void PostKey(int32_t key, uint16_t mod) override
		{
			if(IsNameEntry())
			{
				string after = CurrentName();
				if(AccessInspector::NameChoice(conversation) != fieldBefore)
					Say(FieldName() + ", " + (after.empty() ? string("blank") : after));
				else if(after != nameBefore)
					Say(AccessText::EchoEdit(nameBefore, after));
				else if(key == SDLK_RETURN || key == SDLK_KP_ENTER)
					Say("Both first and last name are required.");
				else if(AccessText::IsTypingKey(key))
					Say("Not allowed.");
				return;
			}
			if((key == SDLK_UP || key == SDLK_DOWN) && AccessInspector::Node(conversation) >= 0)
			{
				int choice = AccessInspector::Choice(conversation);
				if(choice == lastChoice)
					AccessAudio::Play(AccessAudio::Cue::BOUNDARY);
				Say(ChoiceText(choice), SpeechPriority::FOCUS);
			}
		}

		void BuildReview(vector<ReviewGroup> &groups) override
		{
			ReviewGroup history{"Conversation text", {}};
			for(const string &text : AccessInspector::Texts(conversation))
				history.items.emplace_back(text);
			groups.push_back(std::move(history));
			ReviewGroup choices{"Responses", {}};
			vector<string> list = AccessInspector::Choices(conversation);
			for(size_t i = 0; i < list.size(); ++i)
				choices.items.emplace_back([this, i]() { return ChoiceText(static_cast<int>(i)); },
					[this, i]() { return AccessContext::PressKey(static_cast<int32_t>('1' + i)); });
			groups.push_back(std::move(choices));
		}

	private:
		bool IsNameEntry()
		{
			return AccessInspector::Node(conversation) >= 0 && AccessInspector::Choices(conversation).empty();
		}

		string FieldName()
		{
			return AccessInspector::NameChoice(conversation) ? "Last name" : "First name";
		}

		string CurrentName()
		{
			return AccessInspector::NameChoice(conversation) ? AccessInspector::LastName(conversation)
				: AccessInspector::FirstName(conversation);
		}

		string ChoiceText(int index)
		{
			vector<string> list = AccessInspector::Choices(conversation);
			if(index < 0 || index >= static_cast<int>(list.size()))
				return string();
			string text = "Response " + to_string(index + 1) + " of " + to_string(list.size()) + ": "
				+ list[static_cast<size_t>(index)];
			if(!AccessInspector::ChoiceIsActive(conversation, index))
				text += ". Unavailable";
			return text;
		}

		string CurrentState()
		{
			int node = AccessInspector::Node(conversation);
			if(node < 0)
				return "End of conversation. Press Enter to continue.";
			if(IsNameEntry())
				return "Enter your name. " + FieldName() + " field, " + (CurrentName().empty() ? string("blank")
					: CurrentName()) + ". Tab switches fields, Enter confirms.";
			vector<string> list = AccessInspector::Choices(conversation);
			if(list.size() == 1)
				return "One response: " + list.front() + ". Press Enter.";
			return to_string(list.size()) + " responses. " + ChoiceText(AccessInspector::Choice(conversation))
				+ ". Use Up and Down to hear the others.";
		}

		void Announce(bool force)
		{
			vector<string> texts = AccessInspector::Texts(conversation);
			int node = AccessInspector::Node(conversation);
			if(!force && texts.size() == spoken && node == lastNode)
				return;
			string text;
			for(size_t i = spoken; i < texts.size(); ++i)
				text += texts[i] + "\n";
			spoken = texts.size();
			lastNode = node;
			reviewIndex = -1;
			Say(text + CurrentState(), SpeechPriority::FOCUS);
		}

	private:
		ConversationPanel &conversation;
		size_t spoken = 0;
		int lastNode = -2;
		int lastChoice = 0;
		int reviewIndex = -1;
		string nameBefore;
		int fieldBefore = 0;
	};



	class HailReader : public AccessReader {
	public:
		HailReader(Panel &panel) : AccessReader(panel), hail(static_cast<HailPanel &>(panel)) {}

		string Title() override { return "Hailing " + AccessInspector::HailHeader(hail); }

		void OnOpen() override
		{
			lastMessage = AccessInspector::HailMessage(hail);
			Say(Title() + ". " + lastMessage + " " + AccessFormat::List(ButtonKeys()), SpeechPriority::FOCUS);
		}

		void Step() override
		{
			string message = AccessInspector::HailMessage(hail);
			if(message != lastMessage)
			{
				lastMessage = message;
				Say(message, SpeechPriority::FOCUS);
			}
		}

		string Summary() override
		{
			return AccessInspector::HailMessage(hail);
		}

		vector<string> Keys() override
		{
			vector<string> keys = ButtonKeys();
			keys.push_back("D, Enter or Escape ends the conversation");
			return keys;
		}

	private:
		HailPanel &hail;
		string lastMessage;
	};
}



unique_ptr<AccessReader> CreateDialogReader(Panel &panel, const string &id)
{
	if(id == "DialogPanel" || id == "ShipNameDialogPanel" || id == "OptionalInputDialogPanel")
		return make_unique<DialogReader>(panel);
	if(id == "ConversationPanel")
		return make_unique<ConversationReader>(panel);
	if(id == "HailPanel")
		return make_unique<HailReader>(panel);
	return nullptr;
}
