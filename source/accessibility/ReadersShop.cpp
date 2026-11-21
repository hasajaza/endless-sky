/* ReadersShop.cpp
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

// Readers for the shipyard and outfitter.

#include "AccessReaders.h"

#include "AccessContext.h"
#include "AccessFormat.h"
#include "AccessInspector.h"
#include "AccessSpeech.h"

#include "../Account.h"
#include "../CargoHold.h"
#include "../CategoryList.h"
#include "../GameData.h"
#include "../Hardpoint.h"
#include "../Outfit.h"
#include "../Planet.h"
#include "../PlayerInfo.h"
#include "../SDL.h"
#include "../Ship.h"

using namespace std;

namespace {
	// Join label/value pairs from an info display ("shields: 1,200").
	string Pairs(const vector<string> &labels, const vector<string> &values, size_t limit = 100)
	{
		vector<string> parts;
		for(size_t i = 0; i < labels.size() && i < values.size() && parts.size() < limit; ++i)
		{
			string label = labels[i];
			while(!label.empty() && (label.back() == ':' || label.back() == ' '))
				label.pop_back();
			if(label.empty() && values[i].empty())
				continue;
			parts.push_back(values[i].empty() ? label : label + " " + values[i]);
		}
		return AccessFormat::List(parts);
	}

	class ShopReader : public AccessReader {
	public:
		ShopReader(Panel &panel) : AccessReader(panel), shop(static_cast<ShopPanel &>(panel)) {}

		string Title() override
		{
			return AccessInspector::IsOutfitter(shop) ? "Outfitter" : "Shipyard";
		}

		void OnOpen() override
		{
			Say(Title() + ". " + AccessFormat::Credits(Player().Accounts().Credits()) + ". "
				+ SelectedShips() + ". " + Selection(false)
				+ (AccessInspector::IsOutfitter(shop) ? " Press F2 for keys."
					: " Space reads a ship's values one at a time. Press F2 for keys."), SpeechPriority::FOCUS);
		}

		void OnReturn() override
		{
			// A buy or sell key may have opened a dialog (ship name, confirmation,
			// "not enough credits"). Report what it changed now that it closed.
			++returnCount;
			string result;
			if(pendingKey)
			{
				result = Result(pendingKey);
				pendingKey = 0;
				if(result == "Nothing changed.")
					result.clear();
			}
			if(!result.empty())
				Say(result + " " + Selection(false), SpeechPriority::FOCUS);
			else
				Say(Title() + ". " + AccessFormat::Credits(Player().Accounts().Credits()) + ". " + Selection(false),
					SpeechPriority::FOCUS);
		}

		string Summary() override
		{
			return AccessFormat::Credits(Player().Accounts().Credits()) + ". " + SelectedShips() + ". "
				+ Selection(false);
		}

		vector<string> Keys() override
		{
			vector<string> keys = {"Arrow keys move through the items",
				AccessInspector::IsOutfitter(shop)
					? "Space reads the full specifications and description of the selected outfit"
					: "Space reads the ship's values one at a time: shields, hull, speed, cargo and so on, "
						"compared with your own ship; Shift+Space goes back. Moving to another ship "
						"reads the same value for it",
				"Tab switches between the store and your ships",
				"Page Up, Page Down, Home and End scroll", "F searches by name"};
			if(AccessInspector::IsOutfitter(shop))
			{
				keys.push_back("B buys and installs on the selected ships");
				keys.push_back("S sells, from cargo or storage first, then from the selected ships");
				keys.push_back("I installs from cargo or storage, U uninstalls");
				keys.push_back("C buys into cargo or moves from storage to cargo, R moves to storage");
			}
			else
			{
				keys.push_back("B buys the selected ship");
				keys.push_back("S sells the selected ships of yours, R or U sells them but keeps their outfits in storage");
			}
			keys.push_back("Shift, Control and Alt multiply the quantity by 5, 20 and 500");
			keys.push_back("Digits select escort groups; K parks or unparks escorts");
			keys.push_back("Control plus Left arrow goes to the review list Details of the selection, with the full "
				"specifications");
			keys.push_back("L, D or Escape leaves");
			return keys;
		}

		vector<int32_t> CtrlKeysUsedByGame() override
		{
			// Control multiplies the quantity by 20; Control plus a digit
			// stores an escort group.
			return {'b', 's', 'r', 'u', 'i', 'c'};
		}

		bool PreKey(int32_t key, uint16_t mod) override
		{
			if(key == ' ' && !(mod & (KMOD_CTRL | KMOD_ALT | KMOD_GUI)))
			{
				// Shipyard: Space steps through the ship's values one at a
				// time, Shift+Space goes back. Outfitter: all details at once.
				if(AccessInspector::IsOutfitter(shop))
					Say(FullDetails(), SpeechPriority::REQUESTED);
				else
					Say(StepStat((mod & KMOD_SHIFT) ? -1 : 1), SpeechPriority::REQUESTED);
				return true;
			}
			creditsBefore = Player().Accounts().Credits();
			shipsBefore = Player().Ships().size();
			const Outfit *outfit = AccessInspector::SelectedOutfit(shop);
			countsBefore = outfit ? Counts(outfit) : string();
			sidebarBefore = AccessInspector::SidebarActive(shop);
			return false;
		}

		void PostKey(int32_t key, uint16_t mod) override
		{
			if(key == SDLK_TAB)
			{
				Later(1, [this]()
				{
					Say(AccessInspector::SidebarActive(shop) ? "Your ships. " + SelectedShips()
						: "Store. " + Selection(false));
				});
				return;
			}
			if(key == SDLK_UP || key == SDLK_DOWN || key == SDLK_LEFT || key == SDLK_RIGHT
					|| (key >= '0' && key <= '9'))
			{
				Later(1, [this]()
				{
					Say(AccessInspector::SidebarActive(shop) ? SelectedShips() : Selection(false));
				});
				return;
			}
			if(key == 'b' || key == 's' || key == 'i' || key == 'u' || key == 'c' || key == 'r' || key == 'k')
			{
				pendingKey = key;
				int generation = returnCount;
				Later(1, [this, generation]()
				{
					// If a dialog opened, OnReturn reports the result when it closes.
					if(generation != returnCount || !pendingKey)
						return;
					Say(Result(pendingKey));
					pendingKey = 0;
				});
				return;
			}
		}

		void BuildReview(vector<ReviewGroup> &groups) override
		{
			bool outfitter = AccessInspector::IsOutfitter(shop);
			const auto &catalog = AccessInspector::Catalog(shop);
			for(const auto &category : AccessInspector::Categories(shop))
			{
				auto it = catalog.find(category.Name());
				if(it == catalog.end())
					continue;
				ReviewGroup group{category.Name(), {}};
				for(const string &name : it->second)
				{
					if(!AccessInspector::HasItem(shop, name))
						continue;
					if(outfitter)
					{
						const Outfit *outfit = GameData::Outfits().Get(name);
						group.items.emplace_back([this, outfit]() { return OutfitLine(outfit, true); }, [this, outfit]()
						{
							AccessInspector::SelectOutfitItem(shop, outfit);
							AccessInspector::ShopCheckSelection(shop);
							Later(1, [this]() { Say(Selection(false), SpeechPriority::REQUESTED); });
							return true;
						});
						group.items.back().detail = [this, outfit]() { return OutfitDetails(outfit); };
					}
					else
					{
						const Ship *ship = GameData::Ships().Get(name);
						group.items.emplace_back([this, ship]() { return ShipLine(ship); }, [this, ship]()
						{
							AccessInspector::SelectShipModel(shop, ship);
							AccessInspector::ShopCheckSelection(shop);
							Later(1, [this]() { Say(Selection(false), SpeechPriority::REQUESTED); });
							return true;
						});
					}
				}
				groups.push_back(std::move(group));
			}

			ReviewGroup mine{"Your ships here", {}};
			const Planet *here = Player().GetPlanet();
			for(const shared_ptr<Ship> &ship : Player().Ships())
			{
				if(ship->IsParked() && ship->GetPlanet() != here)
					continue;
				if(ship->GetPlanet() != here || ship->IsDestroyed())
					continue;
				Ship *raw = ship.get();
				mine.items.emplace_back([this, raw]()
				{
					string text = raw->GivenName() + ", " + raw->DisplayModelName();
					if(raw == Player().Flagship())
						text += ", flagship";
					if(raw->IsParked())
						text += ", parked";
					if(AccessInspector::PlayerShips(shop).contains(raw))
						text += ", selected";
					return text;
				}, [this, raw]()
				{
					AccessInspector::SelectPlayerShip(shop, raw);
					Later(1, [this]() { Say(SelectedShips(), SpeechPriority::REQUESTED); });
					return true;
				});
			}
			groups.push_back(std::move(mine));

			ReviewGroup details{"Details of the selection", {}};
			for(const string &line : DetailLines())
				details.items.emplace_back(line);
			groups.push_back(std::move(details));
		}

	private:
		string Counts(const Outfit *outfit)
		{
			vector<string> parts;
			Ship *ship = AccessInspector::PlayerShip(shop);
			if(ship)
				parts.push_back(to_string(ship->OutfitCount(outfit)) + " installed on " + ship->GivenName());
			int cargo = Player().Cargo().Get(outfit);
			int storage = Player().Storage().Get(outfit);
			if(cargo)
				parts.push_back(to_string(cargo) + " in cargo");
			if(storage)
				parts.push_back(to_string(storage) + " in storage");
			return AccessFormat::List(parts);
		}

		string OutfitLine(const Outfit *outfit, bool brief)
		{
			if(!outfit)
				return string();
			string text = outfit->DisplayName() + ", " + AccessFormat::Credits(outfit->Cost());
			string counts = Counts(outfit);
			if(!counts.empty())
				text += ", " + counts;
			if(AccessInspector::SelectedOutfit(shop) == outfit)
				text += ", selected";
			return text;
		}

		string OutfitDetails(const Outfit *outfit)
		{
			string text = outfit->Category();
			if(AccessInspector::SelectedOutfit(shop) == outfit)
			{
				const OutfitInfoDisplay &info = AccessInspector::OutfitInfo(shop);
				text += ". " + Pairs(AccessInspector::RequirementLabels(info), AccessInspector::RequirementValues(info))
					+ ". " + Pairs(AccessInspector::AttributeLabels(info), AccessInspector::AttributeValues(info));
			}
			else
				text += ". Select it to hear its full specifications.";
			return text;
		}

		string ShipLine(const Ship *ship)
		{
			if(!ship)
				return string();
			string text = ship->DisplayModelName() + ", " + AccessFormat::Credits(ship->Cost());
			if(AccessInspector::SelectedShip(shop) == ship)
				text += ", selected";
			return text;
		}

		string SelectedShips()
		{
			const auto &ships = AccessInspector::PlayerShips(shop);
			Ship *ship = AccessInspector::PlayerShip(shop);
			if(!ship)
				return "No ship of yours selected";
			string text = "Your ship " + ship->GivenName() + ", " + ship->DisplayModelName();
			if(ships.size() > 1)
				text += ", and " + AccessFormat::Plural(static_cast<int64_t>(ships.size() - 1), "other ship")
					+ " selected";
			if(AccessInspector::IsOutfitter(shop))
			{
				const Outfit &attributes = ship->Attributes();
				text += ". Outfit space free " + AccessFormat::Integer(attributes.Get("outfit space"))
					+ ", weapon space " + AccessFormat::Integer(attributes.Get("weapon capacity"))
					+ ", engine space " + AccessFormat::Integer(attributes.Get("engine capacity"));
			}
			return text;
		}

		string Selection(bool full)
		{
			if(AccessInspector::IsOutfitter(shop))
			{
				const Outfit *outfit = AccessInspector::SelectedOutfit(shop);
				if(!outfit)
					return "No outfit selected. Use the arrow keys to choose one.";
				string text = OutfitLine(outfit, false) + ". " + outfit->Category() + ".";
				int64_t license = AccessInspector::LicenseCost(shop, outfit);
				if(license)
					text += " Requires a license costing " + AccessFormat::Credits(license) + ".";
				if(full)
					text += " " + OutfitDetails(outfit);
				return text;
			}
			const Ship *ship = AccessInspector::SelectedShip(shop);
			if(!ship)
				return "No ship selected. Use the arrow keys to choose one.";
			string text = ship->DisplayModelName();
			const string &category = ship->BaseAttributes().Category();
			if(!category.empty())
				text += ", " + category;
			text += ", " + AccessFormat::Credits(ship->Cost());
			string position = Position(ship->TrueModelName());
			if(!position.empty())
				text += ", " + position;
			int64_t license = AccessInspector::LicenseCost(shop, &ship->Attributes());
			if(license > 0)
				text += ". Requires a license costing " + AccessFormat::Credits(license);
			else if(license < 0)
				text += ". Requires a special license";
			text += ".";
			// Keep the value the player was listening to, so moving to the
			// next ship compares the same value directly.
			if(statIndex >= 0 && !full)
				text += " " + StatText(*ship, statIndex);
			return text;
		}

		// One value of a ship, by name, for the step-by-step reading.
		struct ShipStat {
			string name;
			string (*value)(const Ship &);
		};

		static const vector<ShipStat> &Stats()
		{
			static const vector<ShipStat> stats = {
				{"Shields", [](const Ship &ship)
				{
					const Outfit &a = ship.Attributes();
					double regen = (a.Get("shield generation") + a.Get("delayed shield generation"))
						* (1. + a.Get("shield generation multiplier"));
					string text = AccessFormat::Integer(ship.MaxShields());
					if(regen > 0.)
						text += ", recharging " + AccessFormat::Integer(60. * regen) + " per second";
					return text;
				}},
				{"Hull", [](const Ship &ship)
				{
					const Outfit &a = ship.Attributes();
					double repair = (a.Get("hull repair rate") + a.Get("delayed hull repair rate"))
						* (1. + a.Get("hull repair multiplier"));
					string text = AccessFormat::Integer(ship.MaxHull());
					if(repair > 0.)
						text += ", repairing " + AccessFormat::Integer(60. * repair) + " per second";
					return text;
				}},
				{"Max speed", [](const Ship &ship)
				{
					const Outfit &a = ship.Attributes();
					double thrust = a.Get("thrust") ? a.Get("thrust") : a.Get("afterburner thrust");
					return ship.Drag() > 0. ? AccessFormat::Integer(60. * thrust / ship.Drag()) : string("0");
				}},
				{"Acceleration", [](const Ship &ship)
				{
					const Outfit &a = ship.Attributes();
					double thrust = a.Get("thrust") ? a.Get("thrust") : a.Get("afterburner thrust");
					double mass = a.Mass() / (1. + a.Get("inertia reduction"));
					return mass > 0. ? AccessFormat::Integer(3600. * thrust * (1. + a.Get("acceleration multiplier"))
						/ mass) + " with no cargo" : string("0");
				}},
				{"Turning", [](const Ship &ship)
				{
					const Outfit &a = ship.Attributes();
					double mass = a.Mass() / (1. + a.Get("inertia reduction"));
					return mass > 0. ? AccessFormat::Integer(60. * a.Get("turn") * (1. + a.Get("turn multiplier"))
						/ mass) + " with no cargo" : string("0");
				}},
				{"Cargo space", [](const Ship &ship)
				{
					return AccessFormat::Integer(ship.Attributes().Get("cargo space")) + " tons";
				}},
				{"Bunks", [](const Ship &ship)
				{
					return AccessFormat::Integer(ship.Attributes().Get("bunks")) + ", crew needed "
						+ AccessFormat::Integer(ship.RequiredCrew());
				}},
				{"Fuel", [](const Ship &ship)
				{
					return AccessFormat::Integer(ship.MaxFuel());
				}},
				{"Mass", [](const Ship &ship)
				{
					return AccessFormat::Integer(ship.Attributes().Mass()) + " tons with no cargo";
				}},
				{"Free outfit space", [](const Ship &ship)
				{
					const Outfit &a = ship.Attributes();
					return AccessFormat::Integer(a.Get("outfit space")) + ", of which weapon capacity "
						+ AccessFormat::Integer(a.Get("weapon capacity")) + " and engine capacity "
						+ AccessFormat::Integer(a.Get("engine capacity"));
				}},
				{"Weapons", [](const Ship &ship)
				{
					int guns = 0, gunsUsed = 0, turrets = 0, turretsUsed = 0;
					for(const Hardpoint &hardpoint : ship.Weapons())
					{
						bool used = hardpoint.GetOutfit();
						if(hardpoint.IsTurret())
						{
							++turrets;
							turretsUsed += used;
						}
						else
						{
							++guns;
							gunsUsed += used;
						}
					}
					return AccessFormat::Plural(guns, "gun port") + " (" + to_string(gunsUsed) + " armed), "
						+ AccessFormat::Plural(turrets, "turret mount") + " (" + to_string(turretsUsed) + " armed)";
				}},
				{"Installed outfits", [](const Ship &ship)
				{
					vector<string> items;
					for(const auto &it : ship.Outfits())
						if(it.second > 0)
							items.push_back(it.second == 1 ? it.first->DisplayName()
								: to_string(it.second) + " " + it.first->DisplayName());
					return items.empty() ? string("none") : AccessFormat::List(items);
				}},
				{"Description", [](const Ship &ship)
				{
					string text;
					for(const string &paragraph : AccessText::Paragraphs(ship.Description()))
						text += AccessFormat::TrimPeriod(paragraph) + ". ";
					return text.empty() ? string("none") : text;
				}},
			};
			return stats;
		}

		// The ship the values are read for: the selected ship of yours when
		// that list is active, otherwise the selected ship in the store.
		const Ship *StatShip()
		{
			if(AccessInspector::SidebarActive(shop))
				return AccessInspector::PlayerShip(shop);
			return AccessInspector::SelectedShip(shop);
		}

		string StatText(const Ship &ship, int index)
		{
			const ShipStat &stat = Stats()[static_cast<size_t>(index)];
			string text = stat.name + ": " + stat.value(ship);
			// Compare a ship for sale with the ship you fly now.
			const Ship *flagship = Player().Flagship();
			if(flagship && &ship != flagship && !AccessInspector::SidebarActive(shop)
					&& stat.name != "Description" && stat.name != "Installed outfits")
				text = AccessFormat::TrimPeriod(text) + ". Your " + flagship->DisplayModelName() + ": "
					+ stat.value(*flagship);
			return AccessFormat::TrimPeriod(text) + ".";
		}

		string StepStat(int step)
		{
			const Ship *ship = StatShip();
			if(!ship)
				return "No ship selected.";
			int count = static_cast<int>(Stats().size());
			statIndex = (statIndex < 0) ? (step > 0 ? 0 : count - 1) : (statIndex + step + count) % count;
			return to_string(statIndex + 1) + " of " + to_string(count) + ", " + StatText(*ship, statIndex);
		}

		// "2 of 3": where the item is in the store's list.
		string Position(const string &name)
		{
			int index = 0;
			int count = 0;
			const auto &catalog = AccessInspector::Catalog(shop);
			for(const auto &category : AccessInspector::Categories(shop))
			{
				auto it = catalog.find(category.Name());
				if(it == catalog.end())
					continue;
				for(const string &item : it->second)
					if(AccessInspector::HasItem(shop, item))
					{
						++count;
						if(item == name)
							index = count;
					}
			}
			return index ? to_string(index) + " of " + to_string(count) : string();
		}

		// Everything the shipyard shows about a ship model, grouped so that it
		// reads as sentences: specifications, energy and heat, installed
		// outfits by category, then the description.
		string ShipFullDetails(const Ship &ship)
		{
			auto clean = [](string text)
			{
				size_t start = text.find_first_not_of(' ');
				text = (start == string::npos) ? string() : text.substr(start);
				while(!text.empty() && (text.back() == ':' || text.back() == ' '))
					text.pop_back();
				size_t pos;
				while((pos = text.find("::")) != string::npos)
					text.erase(pos, 1);
				return text;
			};
			const ShipInfoDisplay &info = AccessInspector::ShipInfo(shop);
			vector<string> sentences;

			vector<string> specs;
			const auto &al = AccessInspector::AttributeLabels(info);
			const auto &av = AccessInspector::AttributeValues(info);
			for(size_t i = 0; i < al.size() && i < av.size(); ++i)
			{
				string label = clean(al[i]);
				string value = clean(av[i]);
				if(label.empty() || value.empty() || label == "cost")
					continue;
				specs.push_back(label + " " + value);
			}
			if(!specs.empty())
				sentences.push_back("Specifications: " + AccessFormat::List(specs));

			vector<string> energy;
			for(const string &line : AccessInspector::EnergyHeatLines(info))
			{
				string text = clean(line);
				// The same list carries the model and category header.
				if(text.empty() || text.starts_with("model") || text.starts_with("category"))
					continue;
				energy.push_back(text);
			}
			if(!energy.empty())
				sentences.push_back("Energy and heat per second: " + AccessFormat::List(energy));

			// Outfit list: a label with no value starts a category.
			const auto &ol = AccessInspector::OutfitLabels(info);
			const auto &ov = AccessInspector::OutfitValues(info);
			string header;
			vector<string> items;
			auto flush = [&]()
			{
				if(!items.empty())
					sentences.push_back((header.empty() ? string("Installed") : "Installed " + header) + ": "
						+ AccessFormat::List(items));
				items.clear();
			};
			for(size_t i = 0; i < ol.size() && i < ov.size(); ++i)
			{
				string label = clean(ol[i]);
				string value = clean(ov[i]);
				if(label.empty())
					continue;
				if(value.empty())
				{
					flush();
					header = label;
				}
				else
					items.push_back(value == "1" ? label : value + " " + label);
			}
			flush();

			for(const string &paragraph : AccessText::Paragraphs(ship.Description()))
				if(!clean(paragraph).empty())
					sentences.push_back(paragraph);

			string text;
			for(const string &sentence : sentences)
				text += AccessFormat::TrimPeriod(sentence) + ". ";
			return text;
		}

		// Everything the screen shows about the selection, read by Space.
		string FullDetails()
		{
			if(AccessInspector::IsOutfitter(shop))
			{
				const Outfit *outfit = AccessInspector::SelectedOutfit(shop);
				if(!outfit)
					return "No outfit selected.";
			}
			else if(!AccessInspector::SelectedShip(shop))
				return "No ship selected.";
			else
				return ShipFullDetails(*AccessInspector::SelectedShip(shop));
			string text;
			for(const string &line : DetailLines())
			{
				string clean = line;
				size_t start = clean.find_first_not_of(' ');
				clean = (start == string::npos) ? string() : clean.substr(start);
				if(clean.empty())
					continue;
				text += AccessFormat::TrimPeriod(clean) + ". ";
			}
			return text;
		}

		vector<string> DetailLines()
		{
			vector<string> lines;
			if(AccessInspector::IsOutfitter(shop))
			{
				const OutfitInfoDisplay &info = AccessInspector::OutfitInfo(shop);
				const auto &rl = AccessInspector::RequirementLabels(info);
				const auto &rv = AccessInspector::RequirementValues(info);
				for(size_t i = 0; i < rl.size() && i < rv.size(); ++i)
					if(!rl[i].empty() || !rv[i].empty())
						lines.push_back(rl[i] + " " + rv[i]);
				const auto &al = AccessInspector::AttributeLabels(info);
				const auto &av = AccessInspector::AttributeValues(info);
				for(size_t i = 0; i < al.size() && i < av.size(); ++i)
					if(!al[i].empty() || !av[i].empty())
						lines.push_back(al[i] + " " + av[i]);
				string description = AccessInspector::DescriptionText(info);
				for(const string &paragraph : AccessText::Paragraphs(description))
					lines.push_back(paragraph);
			}
			else
			{
				const ShipInfoDisplay &info = AccessInspector::ShipInfo(shop);
				const auto &al = AccessInspector::AttributeLabels(info);
				const auto &av = AccessInspector::AttributeValues(info);
				for(size_t i = 0; i < al.size() && i < av.size(); ++i)
					if(!al[i].empty() || !av[i].empty())
						lines.push_back(al[i] + " " + av[i]);
				for(const string &line : AccessInspector::EnergyHeatLines(info))
					lines.push_back(line);
				const auto &ol = AccessInspector::OutfitLabels(info);
				const auto &ov = AccessInspector::OutfitValues(info);
				for(size_t i = 0; i < ol.size() && i < ov.size(); ++i)
					if(!ol[i].empty() || !ov[i].empty())
						lines.push_back(ol[i] + " " + ov[i]);
				const auto &sl = AccessInspector::SaleLabels(info);
				const auto &sv = AccessInspector::SaleValues(info);
				for(size_t i = 0; i < sl.size() && i < sv.size(); ++i)
					if(!sl[i].empty() || !sv[i].empty())
						lines.push_back(sl[i] + " " + sv[i]);
				for(const string &paragraph : AccessText::Paragraphs(AccessInspector::DescriptionText(info)))
					lines.push_back(paragraph);
			}
			if(lines.empty())
				lines.push_back("Nothing selected.");
			return lines;
		}

		string Result(int32_t key)
		{
			int64_t credits = Player().Accounts().Credits();
			int64_t change = credits - creditsBefore;
			if(key == 'k')
				return "Parking changed. " + SelectedShips() + ".";
			string text;
			if(change < 0)
				text = "Spent " + AccessFormat::Credits(-change) + ". ";
			else if(change > 0)
				text = "Received " + AccessFormat::Credits(change) + ". ";
			if(AccessInspector::IsOutfitter(shop))
			{
				const Outfit *outfit = AccessInspector::SelectedOutfit(shop);
				string counts = outfit ? Counts(outfit) : string();
				if(!change && counts == countsBefore)
					return "Nothing changed.";
				return text + (outfit ? outfit->DisplayName() + ": " + (counts.empty() ? "none owned" : counts) + ". "
					: string()) + AccessFormat::Credits(credits) + " left.";
			}
			size_t ships = Player().Ships().size();
			if(ships > shipsBefore)
				text += "You now own " + AccessFormat::Plural(static_cast<int64_t>(ships), "ship") + ". ";
			else if(ships < shipsBefore)
				text += "Ship sold. You own " + AccessFormat::Plural(static_cast<int64_t>(ships), "ship") + ". ";
			else if(!change)
				return "Nothing changed.";
			return text + AccessFormat::Credits(credits) + " left.";
		}

	private:
		ShopPanel &shop;
		int64_t creditsBefore = 0;
		size_t shipsBefore = 0;
		string countsBefore;
		bool sidebarBefore = false;
		int32_t pendingKey = 0;
		// Which value Space last read (-1: none yet).
		int statIndex = -1;
		int returnCount = 0;
	};
}



unique_ptr<AccessReader> CreateShopReader(Panel &panel, const string &id)
{
	if(id == "ShipyardPanel" || id == "OutfitterPanel" || id == "ShopPanel")
		return make_unique<ShopReader>(panel);
	return nullptr;
}
