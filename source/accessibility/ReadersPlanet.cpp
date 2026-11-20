/* ReadersPlanet.cpp
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

// Readers for the planet screen and its services: spaceport, trading, bank
// and hiring.

#include "AccessReaders.h"

#include "AccessCapture.h"
#include "AccessContext.h"
#include "AccessFormat.h"
#include "AccessInspector.h"
#include "AccessSpeech.h"

#include "../Account.h"
#include "../CargoHold.h"
#include "../GameData.h"
#include "../Information.h"
#include "../Mortgage.h"
#include "../Planet.h"
#include "../PlayerInfo.h"
#include "../Port.h"
#include "../SDL.h"
#include "../Ship.h"
#include "../System.h"
#include "../Trade.h"

using namespace std;

namespace {
	class PlanetReader : public AccessReader {
	public:
		PlanetReader(Panel &panel) : AccessReader(panel), planetPanel(static_cast<PlanetPanel &>(panel)) {}

		string Title() override
		{
			const Planet &planet = AccessInspector::GetPlanet(planetPanel);
			return "Landed on " + planet.DisplayName();
		}

		void OnOpen() override
		{
			const Planet &planet = AccessInspector::GetPlanet(planetPanel);
			string text = Title() + ", " + planet.Noun();
			if(planet.GetSystem())
				text += ", " + planet.GetSystem()->DisplayName() + " system";
			text += ". ";
			if(!planet.CanUseServices())
				text += "Services are not available to you here. ";
			text += Services() + ". Up and Down arrows choose, Enter selects. ";
			text += planet.Description().ToString();
			Say(text, SpeechPriority::FOCUS);
		}

		void OnReturn() override
		{
			Say(Title() + ". " + Services(), SpeechPriority::FOCUS);
		}

		string Summary() override
		{
			return Services() + ". " + AccessFlight_Status();
		}

		vector<string> Keys() override
		{
			vector<string> keys = ButtonKeys();
			keys.push_back("Up and Down arrows move through these options and Enter selects one");
			keys.push_back("I player info, M map, slash message log");
			return keys;
		}

		bool PreKey(int32_t key, uint16_t mod) override
		{
			// The planet screen itself does not use these keys.
			if(mod & (KMOD_CTRL | KMOD_ALT | KMOD_SHIFT))
				return false;
			return NavigateButtons(key);
		}

		void BuildReview(vector<ReviewGroup> &groups) override
		{
			ReviewGroup description{"Description", {}};
			const Planet &planet = AccessInspector::GetPlanet(planetPanel);
			for(const string &paragraph : AccessText::Paragraphs(planet.Description().ToString()))
				description.items.emplace_back(paragraph);
			groups.push_back(std::move(description));
			AddScreenText(groups);
		}

	private:
		string Services()
		{
			vector<string> keys = ButtonKeys();
			if(keys.empty())
				return "No services";
			return "Options: " + AccessFormat::List(keys);
		}

		static string AccessFlight_Status()
		{
			const Ship *flagship = Player().Flagship();
			if(!flagship)
				return "You have no ship that can depart.";
			return AccessFormat::Credits(Player().Accounts().Credits()) + ". Flagship " + flagship->GivenName() + ".";
		}

	private:
		PlanetPanel &planetPanel;
	};



	class SpaceportReader : public AccessReader {
	public:
		SpaceportReader(Panel &panel) : AccessReader(panel), port(static_cast<SpaceportPanel &>(panel)) {}

		string Title() override { return "Spaceport"; }

		void OnOpen() override
		{
			lastNews = News();
			Say("Spaceport. " + AccessInspector::SpaceportDescription(port) + " " + lastNews, SpeechPriority::FOCUS);
		}

		void Step() override
		{
			string news = News();
			if(news != lastNews)
			{
				lastNews = news;
				if(!news.empty())
					Say(news, SpeechPriority::FOCUS);
			}
		}

		string Summary() override
		{
			return AccessInspector::SpaceportDescription(port) + " " + News();
		}

		vector<string> Keys() override
		{
			return {"P again shows new news", "Other planet keys switch to the other services"};
		}

		void BuildReview(vector<ReviewGroup> &groups) override
		{
			ReviewGroup text{"Spaceport", {}};
			for(const string &paragraph : AccessText::Paragraphs(AccessInspector::SpaceportDescription(port)))
				text.items.emplace_back(paragraph);
			string news = News();
			if(!news.empty())
				text.items.emplace_back(news);
			groups.push_back(std::move(text));
		}

	private:
		string News()
		{
			if(!AccessInspector::HasNews(port))
				return string();
			string name = AccessInspector::NewsInfo(port).GetString("name");
			return "News. " + name + " " + AccessInspector::NewsText(port);
		}

	private:
		SpaceportPanel &port;
		string lastNews;
	};



	class TradingReader : public AccessReader {
	public:
		TradingReader(Panel &panel) : AccessReader(panel), trading(static_cast<TradingPanel &>(panel)) {}

		string Title() override { return "Trading"; }

		void OnOpen() override
		{
			Say("Trading. " + Summary(), SpeechPriority::FOCUS);
		}

		void OnReturn() override
		{
			Say("Trading. " + Row(Player().MapColoring()), SpeechPriority::FOCUS);
		}

		string Summary() override
		{
			const CargoHold &cargo = Player().Cargo();
			string text = AccessFormat::Credits(Player().Accounts().Credits()) + ", "
				+ AccessFormat::Integer(cargo.Free()) + " tons of cargo space free. ";
			if(cargo.CommoditiesSize())
				text += "Carrying " + AccessFormat::Plural(cargo.CommoditiesSize(), "ton") + " of commodities. ";
			return text + Row(Player().MapColoring());
		}

		vector<string> Keys() override
		{
			vector<string> keys = {"Up and Down arrows choose a commodity",
				"Plus, Enter or Space buy one ton; Minus, Backspace or Delete sell one ton",
				"Hold Shift for 5, Control for 20, Alt for 500 at a time",
				"U or Shift B buy all you can", "E or Shift S sell all commodities"};
			if(AccessInspector::CanSellOutfits(trading))
				keys.push_back("F or Shift O sell the outfits in your cargo hold");
			keys.push_back("N or Shift M sell minerals from mining");
			if(AccessInspector::CanStoreOutfits(trading))
				keys.push_back("R move outfits from cargo to storage");
			keys.push_back("M map, other planet keys switch services");
			return keys;
		}

		vector<int32_t> CtrlKeysUsedByGame() override
		{
			// Control multiplies the quantity bought or sold by 20.
			return {SDLK_RETURN, SDLK_SPACE, SDLK_EQUALS, SDLK_PLUS, SDLK_KP_PLUS, SDLK_MINUS, SDLK_KP_MINUS,
				SDLK_BACKSPACE, SDLK_DELETE};
		}

		bool PreKey(int32_t key, uint16_t mod) override
		{
			creditsBefore = Player().Accounts().Credits();
			cargoBefore = Player().Cargo().Used();
			int row = Player().MapColoring();
			heldBefore = row >= 0 && row < static_cast<int>(GameData::Commodities().size())
				? Player().Cargo().Get(GameData::Commodities()[static_cast<size_t>(row)].name) : 0;
			return false;
		}

		void PostKey(int32_t key, uint16_t mod) override
		{
			int row = Player().MapColoring();
			if(key == SDLK_UP || key == SDLK_DOWN)
			{
				Say(Row(row));
				return;
			}
			int64_t credits = Player().Accounts().Credits();
			int64_t spent = creditsBefore - credits;
			int used = Player().Cargo().Used();
			bool isTrade = key == SDLK_EQUALS || key == SDLK_KP_PLUS || key == SDLK_PLUS || key == SDLK_RETURN
				|| key == SDLK_SPACE || key == SDLK_MINUS || key == SDLK_KP_MINUS || key == SDLK_BACKSPACE
				|| key == SDLK_DELETE || key == 'u' || key == 'e'
				|| ((key == 'b' || key == 's') && (mod & KMOD_SHIFT));
			if(!isTrade && key != 'n' && key != 'f' && key != 'r' && key != 'm' && key != 'o')
				return;
			if(!spent && used == cargoBefore)
			{
				if(isTrade)
					Say(FailureReason(key, mod, row));
				return;
			}
			string name = row >= 0 && row < static_cast<int>(GameData::Commodities().size())
				? GameData::Commodities()[static_cast<size_t>(row)].name : string("cargo");
			int held = Player().Cargo().Get(name);
			string text;
			if(spent > 0)
				text = "Bought " + AccessFormat::Plural(held - heldBefore, "ton") + " of " + name + " for "
					+ AccessFormat::Credits(spent);
			else if(held < heldBefore)
				text = "Sold " + AccessFormat::Plural(heldBefore - held, "ton") + " of " + name + " for "
					+ AccessFormat::Credits(-spent);
			else
				text = "Sold for " + AccessFormat::Credits(-spent);
			text += ". You hold " + AccessFormat::Plural(held, "ton") + " of " + name + ", "
				+ AccessFormat::Credits(credits) + ", " + AccessFormat::Integer(Player().Cargo().Free())
				+ " tons free.";
			if(AccessInspector::TonsSold(trading) && spent < 0)
			{
				int64_t profit = AccessInspector::Profit(trading);
				text += " Total sold this visit " + AccessFormat::Plural(AccessInspector::TonsSold(trading), "ton")
					+ ", " + (profit >= 0 ? "profit " : "loss ") + AccessFormat::Credits(profit >= 0 ? profit : -profit)
					+ ".";
			}
			Say(text);
		}

		void BuildReview(vector<ReviewGroup> &groups) override
		{
			ReviewGroup rows{"Commodities", {}};
			const auto &commodities = GameData::Commodities();
			for(size_t i = 0; i < commodities.size(); ++i)
				rows.items.emplace_back([this, i]() { return Row(static_cast<int>(i)); }, [this, i]()
				{
					Player().SetMapColoring(static_cast<int>(i));
					Say("Selected " + Row(static_cast<int>(i)), SpeechPriority::REQUESTED);
					return true;
				});
			groups.push_back(std::move(rows));
			AddScreenText(groups);
		}

	private:
		string Row(int row)
		{
			const auto &commodities = GameData::Commodities();
			if(row < 0 || row >= static_cast<int>(commodities.size()))
				return "No commodity selected.";
			const Trade::Commodity &commodity = commodities[static_cast<size_t>(row)];
			const System *system = Player().GetSystem();
			int price = system ? system->Trade(commodity.name) : 0;
			int hold = Player().Cargo().Get(commodity.name);
			string text = commodity.name + ", ";
			if(!price)
				text += "not for sale here";
			else
			{
				text += AccessFormat::Credits(price) + " per ton";
				static const char *LEVEL[5] = {"very low", "low", "medium", "high", "very high"};
				int level = price - commodity.low;
				if(level < 0)
					level = 0;
				else if(level >= commodity.high - commodity.low)
					level = 4;
				else
					level = (5 * level) / (commodity.high - commodity.low);
				text += ", " + string(LEVEL[level]) + " price";
			}
			if(hold)
			{
				text += ", you hold " + AccessFormat::Plural(hold, "ton");
				int basis = static_cast<int>(Player().GetBasis(commodity.name));
				if(basis && price && basis != price)
					text += ", " + string(price > basis ? "profit " : "loss ")
						+ AccessFormat::Credits(price > basis ? price - basis : basis - price) + " per ton";
			}
			if(price)
			{
				int64_t affordable = min<int64_t>(Player().Cargo().Free(), Player().Accounts().Credits() / price);
				text += ", you can buy " + AccessFormat::Integer(static_cast<double>(affordable));
			}
			return text + ".";
		}

		string FailureReason(int32_t key, uint16_t mod, int row)
		{
			const auto &commodities = GameData::Commodities();
			if(row < 0 || row >= static_cast<int>(commodities.size()))
				return "Select a commodity first.";
			const string &name = commodities[static_cast<size_t>(row)].name;
			const System *system = Player().GetSystem();
			int price = system ? system->Trade(name) : 0;
			bool selling = key == SDLK_MINUS || key == SDLK_KP_MINUS || key == SDLK_BACKSPACE || key == SDLK_DELETE
				|| key == 'e' || (key == 's' && (mod & KMOD_SHIFT));
			if(!price)
				return name + " is not traded here.";
			if(selling)
				return key == 'e' || key == 's' ? "You have no commodities to sell here." : "You have no " + name
					+ " to sell.";
			if(Player().Cargo().Free() <= 0)
				return "Your cargo hold is full.";
			if(Player().Accounts().Credits() < price)
				return "You cannot afford " + name + ".";
			return "Nothing was traded.";
		}

	private:
		TradingPanel &trading;
		int64_t creditsBefore = 0;
		int cargoBefore = 0;
		int heldBefore = 0;
	};



	class BankReader : public AccessReader {
	public:
		BankReader(Panel &panel) : AccessReader(panel), bank(static_cast<BankPanel &>(panel)) {}

		string Title() override { return "Bank"; }

		void OnOpen() override
		{
			Say("Bank. " + Summary(), SpeechPriority::FOCUS);
		}

		string Summary() override
		{
			const Account &account = Player().Accounts();
			vector<string> parts;
			parts.push_back(AccessFormat::Credits(account.Credits()));
			parts.push_back("net worth " + AccessFormat::Credits(account.NetWorth()));
			parts.push_back("credit score " + to_string(account.CreditScore()));
			parts.push_back(AccessFormat::Plural(static_cast<int64_t>(account.Mortgages().size()), "debt"));
			if(account.CrewSalariesOwed())
				parts.push_back("overdue crew salaries " + AccessFormat::Credits(account.CrewSalariesOwed()));
			if(account.MaintenanceDue())
				parts.push_back("overdue maintenance " + AccessFormat::Credits(account.MaintenanceDue()));
			return AccessFormat::List(parts) + ". " + Row();
		}

		vector<string> Keys() override
		{
			return {"Up and Down arrows choose a debt or the new mortgage row",
				"Enter pays extra on the selected debt, or applies for a mortgage",
				"A pays all debts you can afford and any overdue bills"};
		}

		bool PreKey(int32_t key, uint16_t mod) override
		{
			creditsBefore = Player().Accounts().Credits();
			return false;
		}

		void PostKey(int32_t key, uint16_t mod) override
		{
			if(key == SDLK_UP || key == SDLK_DOWN)
				Say(Row());
			else if(key == 'a')
			{
				int64_t paid = creditsBefore - Player().Accounts().Credits();
				Say(paid ? "Paid " + AccessFormat::Credits(paid) + ". " + Summary() : "Nothing could be paid.");
			}
		}

		void OnReturn() override
		{
			Say("Bank. " + Summary(), SpeechPriority::FOCUS);
		}

		void BuildReview(vector<ReviewGroup> &groups) override
		{
			ReviewGroup debts{"Debts", {}};
			for(const Mortgage &mortgage : Player().Accounts().Mortgages())
				debts.items.emplace_back(Describe(mortgage));
			groups.push_back(std::move(debts));
			AddScreenText(groups);
		}

	private:
		static string Describe(const Mortgage &mortgage)
		{
			return mortgage.Type() + ", principal " + AccessFormat::Credits(mortgage.Principal()) + ", interest "
				+ mortgage.Interest() + ", " + AccessFormat::Plural(mortgage.Term(), "day") + " left, payment "
				+ AccessFormat::Credits(mortgage.Payment()) + " per day";
		}

		string Row()
		{
			int row = AccessInspector::SelectedRow(bank);
			int rows = AccessInspector::MortgageRows(bank);
			const auto &mortgages = Player().Accounts().Mortgages();
			if(row < rows && row < static_cast<int>(mortgages.size()))
				return "Selected: " + Describe(mortgages[static_cast<size_t>(row)])
					+ ". Press Enter to pay extra.";
			if(row < rows)
				return "Selected: other debts. Press Enter to pay extra.";
			int64_t qualify = AccessInspector::Qualify(bank);
			if(qualify)
				return "Selected: new mortgage. You qualify for up to " + AccessFormat::Credits(qualify)
					+ ". Press Enter to apply.";
			return "You do not qualify for a new mortgage.";
		}

	private:
		BankPanel &bank;
		int64_t creditsBefore = 0;
	};



	class HiringReader : public AccessReader {
	public:
		HiringReader(Panel &panel) : AccessReader(panel), hiring(static_cast<HiringPanel &>(panel)) {}

		string Title() override { return "Hire crew"; }

		void OnOpen() override
		{
			Say("Hire crew. " + Summary(), SpeechPriority::FOCUS);
		}

		string Summary() override
		{
			const Ship *flagship = Player().Flagship();
			if(!flagship)
				return "You have no flagship.";
			string text = "Flagship crew " + to_string(flagship->Crew()) + ", " + to_string(flagship->RequiredCrew())
				+ " required, " + to_string(static_cast<int>(flagship->Attributes().Get("bunks"))) + " bunks. ";
			text += "You can hire " + to_string(AccessInspector::MaxHire(hiring)) + " and fire "
				+ to_string(AccessInspector::MaxFire(hiring)) + ". ";
			text += "Daily crew salaries " + AccessFormat::Credits(Player().Salaries()) + ".";
			return text;
		}

		vector<string> Keys() override
		{
			return {"H, Plus, Enter or Space hire one crew member", "F, Minus, Backspace or Delete fire one",
				"Hold Shift, Control or Alt for more at a time"};
		}

		vector<int32_t> CtrlKeysUsedByGame() override
		{
			return {'h', 'f', SDLK_RETURN, SDLK_SPACE, SDLK_EQUALS, SDLK_PLUS, SDLK_KP_PLUS, SDLK_MINUS,
				SDLK_KP_MINUS, SDLK_BACKSPACE, SDLK_DELETE};
		}

		void PostKey(int32_t key, uint16_t mod) override
		{
			if(key == 'h' || key == 'f' || key == SDLK_EQUALS || key == SDLK_PLUS || key == SDLK_KP_PLUS
					|| key == SDLK_RETURN || key == SDLK_SPACE || key == SDLK_MINUS || key == SDLK_KP_MINUS
					|| key == SDLK_BACKSPACE || key == SDLK_DELETE)
				Later(2, [this]() { Say(Summary()); });
		}

	private:
		HiringPanel &hiring;
	};
}



unique_ptr<AccessReader> CreatePlanetReader(Panel &panel, const string &id)
{
	if(id == "PlanetPanel")
		return make_unique<PlanetReader>(panel);
	if(id == "SpaceportPanel")
		return make_unique<SpaceportReader>(panel);
	if(id == "TradingPanel")
		return make_unique<TradingReader>(panel);
	if(id == "BankPanel")
		return make_unique<BankReader>(panel);
	if(id == "HiringPanel")
		return make_unique<HiringReader>(panel);
	return nullptr;
}
