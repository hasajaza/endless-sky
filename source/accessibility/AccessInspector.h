/* AccessInspector.h
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

#include "../AI.h"
#include "../BankPanel.h"
#include "../BoardingPanel.h"
#include "../Conversation.h"
#include "../ConversationPanel.h"
#include "../DialogPanel.h"
#include "../Engine.h"
#include "../GamerulesPanel.h"
#include "../HailPanel.h"
#include "../HiringPanel.h"
#include "../ItemInfoDisplay.h"
#include "../LoadPanel.h"
#include "../LogbookPanel.h"
#include "../MainPanel.h"
#include "../MapDetailPanel.h"
#include "../MapPanel.h"
#include "../MapPlanetCard.h"
#include "../MapSalesPanel.h"
#include "../MenuPanel.h"
#include "../MessageLogPanel.h"
#include "../MissionPanel.h"
#include "../OutfitInfoDisplay.h"
#include "../OutfitterPanel.h"
#include "../Panel.h"
#include "../PlanetPanel.h"
#include "../PlayerInfoPanel.h"
#include "../PreferencesPanel.h"
#include "../ShipInfoDisplay.h"
#include "../ShipInfoPanel.h"
#include "../ShipyardPanel.h"
#include "../ShopPanel.h"
#include "../SpaceportPanel.h"
#include "../StartConditions.h"
#include "../StartConditionsPanel.h"
#include "../TextArea.h"
#include "../TradingPanel.h"

#include <list>
#include <memory>



// Read-only (and, for the review cursor, carefully limited) access to the
// private state of game classes. Every class that needs to be inspected
// declares "friend class AccessInspector", so the accessibility layer never
// needs to change how those classes work.
class AccessInspector {
public:
	// Panel internals.
	static bool DoKey(Panel &panel, SDL_Keycode key, Uint16 mod = 0) { return panel.DoKey(key, mod); }
	static UI &GetUI(Panel &panel) { return panel.GetUI(); }
	static const std::vector<std::shared_ptr<Panel>> &Children(Panel &panel) { return panel.GetChildren(); }

	// Engine (only while the calculation thread is idle).
	static const std::list<std::shared_ptr<Ship>> &Ships(const Engine &engine) { return engine.ships; }
	static const AsteroidField &Asteroids(const Engine &engine) { return engine.asteroids; }
	static const std::list<std::shared_ptr<Flotsam>> &FlotsamList(const Engine &engine) { return engine.flotsam; }
	static const std::vector<Projectile> &Projectiles(const Engine &engine) { return engine.projectiles; }
	static const Information &HudInfo(const Engine &engine) { return engine.info; }
	static const Command &AutoPilot(const Engine &engine) { return engine.ai.autoPilot; }
	static Command &ActiveCommands(Engine &engine) { return engine.activeCommands; }
	static AI &GetAI(Engine &engine) { return engine.ai; }

	// Main panel.
	static Engine &GetEngine(MainPanel &panel) { return panel.engine; }

	// Item info displays (shop and info panels).
	static const std::vector<std::string> &AttributeLabels(const ItemInfoDisplay &d) { return d.attributeLabels; }
	static const std::vector<std::string> &AttributeValues(const ItemInfoDisplay &d) { return d.attributeValues; }
	static const std::string &DescriptionText(const ItemInfoDisplay &d) { return d.description.OriginalText(); }
	static const ShipInfoDisplay &ShipInfo(const ShopPanel &p) { return p.shipInfo; }
	static const OutfitInfoDisplay &OutfitInfo(const ShopPanel &p) { return p.outfitInfo; }
	static std::vector<std::string> EnergyHeatLines(const ShipInfoDisplay &d)
	{
		std::vector<std::string> lines;
		for(size_t i = 0; i < d.attributeHeaderLabels.size() && i < d.attributeHeaderValues.size(); ++i)
			lines.push_back(d.attributeHeaderLabels[i] + " " + d.attributeHeaderValues[i]);
		for(size_t i = 0; i < d.tableLabels.size() && i < d.energyTable.size() && i < d.heatTable.size(); ++i)
			lines.push_back(d.tableLabels[i] + ": energy " + d.energyTable[i] + ", heat " + d.heatTable[i]);
		return lines;
	}
	static const std::vector<std::string> &SaleLabels(const ShipInfoDisplay &d) { return d.saleLabels; }
	static const std::vector<std::string> &SaleValues(const ShipInfoDisplay &d) { return d.saleValues; }
	static const std::vector<std::string> &OutfitLabels(const ShipInfoDisplay &d) { return d.outfitLabels; }
	static const std::vector<std::string> &OutfitValues(const ShipInfoDisplay &d) { return d.outfitValues; }
	static const std::vector<std::string> &RequirementLabels(const OutfitInfoDisplay &d) { return d.requirementLabels; }
	static const std::vector<std::string> &RequirementValues(const OutfitInfoDisplay &d) { return d.requirementValues; }

	// Map panels.
	static const System *SelectedSystem(const MapPanel &p) { return p.selectedSystem; }
	static const Planet *SelectedPlanet(const MapPanel &p) { return p.selectedPlanet; }
	static const DistanceMap &Distances(const MapPanel &p) { return p.distance; }
	static int Commodity(const MapPanel &p) { return p.commodity; }
	static bool IsSimplified(const MapPanel &p) { return p.isSimplified; }
	static void Select(MapPanel &p, const System *system) { p.Select(system); }
	static void CenterOn(MapPanel &p, const System *system) { p.CenterOnSystem(system); }
	static void SetSelectedSystem(MapPanel &p, const System *system)
	{
		p.selectedSystem = system;
		p.UpdateCache();
		p.CenterOnSystem(system);
	}
	static const std::string &ButtonCondition(const MapPanel &p) { return p.buttonCondition; }
	static bool PlanetViewSelected(const MapDetailPanel &p) { return p.isPlanetViewSelected; }
	static const Planet *SelectedCard(const MapDetailPanel &p)
	{
		for(const MapPlanetCard &card : p.planetCards)
			if(card.IsSelected())
				return card.GetPlanet();
		return nullptr;
	}

	// Missions.
	static const std::list<Mission> &Available(const MissionPanel &p) { return p.available; }
	static const std::list<Mission> &Accepted(const MissionPanel &p) { return p.accepted; }
	static std::list<Mission>::const_iterator AvailableIt(const MissionPanel &p) { return p.availableIt; }
	static std::list<Mission>::const_iterator AcceptedIt(const MissionPanel &p) { return p.acceptedIt; }
	static bool CanAccept(const MissionPanel &p) { return p.CanAccept(); }

	// Conversation.
	static int Node(const ConversationPanel &p) { return p.node; }
	static int Choice(const ConversationPanel &p) { return p.choice; }
	static size_t TextCount(const ConversationPanel &p) { return p.text.size(); }
	static std::vector<std::string> Texts(const ConversationPanel &p)
	{
		std::vector<std::string> result;
		for(const auto &paragraph : p.text)
			result.push_back(paragraph.Text());
		return result;
	}
	static std::vector<std::string> Choices(const ConversationPanel &p)
	{
		std::vector<std::string> result;
		for(const auto &it : p.choices)
			result.push_back(it.first.Text());
		return result;
	}
	static bool ChoiceIsActive(const ConversationPanel &p, int index)
	{
		if(index < 0 || index >= static_cast<int>(p.choices.size()))
			return false;
		return p.conversation.ChoiceIsActive(p.node, p.MapChoice(index));
	}
	static const std::string &FirstName(const ConversationPanel &p) { return p.firstName; }
	static const std::string &LastName(const ConversationPanel &p) { return p.lastName; }
	static int NameChoice(const ConversationPanel &p) { return p.choice; }

	// Dialog.
	static std::string Message(const DialogPanel &p) { return p.text ? p.text->GetText() : std::string(); }
	static const std::string &Input(const DialogPanel &p) { return p.input; }
	static bool AcceptsInput(const DialogPanel &p) { return p.AcceptsInput(); }
	static int ActiveButton(const DialogPanel &p) { return p.activeButton; }
	static int NumButtons(const DialogPanel &p) { return p.numButtons; }
	static const std::string &OkText(const DialogPanel &p) { return p.okText; }
	static const std::string &CancelText(const DialogPanel &p) { return p.cancelText; }
	static const std::string &ThirdText(const DialogPanel &p) { return p.buttonThree.buttonLabel; }
	static char ThirdKey(const DialogPanel &p) { return static_cast<char>(p.buttonThree.buttonKey); }
	static bool IsOkDisabled(const DialogPanel &p) { return p.isOkDisabled; }
	static bool IsMission(const DialogPanel &p) { return p.isMission; }
	static bool CanCancel(const DialogPanel &p) { return p.canCancel; }
	static bool HasSystem(const DialogPanel &p) { return p.system && p.player; }
	static bool IsNumeric(const DialogPanel &p) { return p.intFun || p.doubleFun; }

	// Hail.
	static std::string HailMessage(const HailPanel &p) { return p.message ? p.message->GetText() : std::string(); }
	static const std::string &HailHeader(const HailPanel &p) { return p.header; }

	// Load panel.
	static const std::shared_ptr<PilotProfile> &SelectedPilot(const LoadPanel &p) { return p.selectedPilot; }
	static const std::string &SelectedFile(const LoadPanel &p) { return p.selectedFile; }
	static const std::vector<std::shared_ptr<PilotProfile>> &Pilots(const LoadPanel &p) { return p.pilots; }
	static bool SideHasFocus(const LoadPanel &p) { return p.sideHasFocus; }
	static const SavedGame &LoadedInfo(const LoadPanel &p) { return p.loadedInfo; }

	// Start conditions.
	static bool HasScenario(const StartConditionsPanel &p) { return p.startIt != p.scenarios.end(); }
	static const StartConditions &Scenario(const StartConditionsPanel &p) { return *p.startIt; }
	static size_t ScenarioIndex(const StartConditionsPanel &p)
	{
		std::vector<StartConditions>::const_iterator it = p.startIt;
		return static_cast<size_t>(it - p.scenarios.cbegin());
	}
	static size_t ScenarioCount(const StartConditionsPanel &p) { return p.scenarios.size(); }
	static const Information &ScenarioInfo(const StartConditionsPanel &p) { return p.info; }
	static std::string ScenarioDescription(const StartConditionsPanel &p)
	{
		return p.description ? p.description->GetText() : std::string();
	}

	// Planet.
	static const Planet &GetPlanet(const PlanetPanel &p) { return p.planet; }
	static Panel *SelectedPanel(const PlanetPanel &p) { return p.selectedPanel.get(); }
	static bool HasShipyard(const PlanetPanel &p) { return p.hasShipyard; }
	static bool HasOutfitter(const PlanetPanel &p) { return p.hasOutfitter; }

	// Trading.
	static int TonsSold(const TradingPanel &p) { return p.tonsSold; }
	static int64_t Profit(const TradingPanel &p) { return p.profit; }
	static bool CanSellOutfits(const TradingPanel &p) { return p.canSellOutfits; }
	static bool CanStoreOutfits(const TradingPanel &p) { return p.canStoreOutfits; }

	// Bank.
	static int SelectedRow(const BankPanel &p) { return p.selectedRow; }
	static int MortgageRows(const BankPanel &p) { return p.mortgageRows; }
	static int64_t Qualify(const BankPanel &p) { return p.qualify; }

	// Hiring.
	static int MaxHire(const HiringPanel &p) { return p.maxHire; }
	static int MaxFire(const HiringPanel &p) { return p.maxFire; }

	// Spaceport.
	static std::string SpaceportDescription(const SpaceportPanel &p)
	{
		return p.description ? p.description->GetText() : std::string();
	}
	static bool HasNews(const SpaceportPanel &p) { return p.hasNews; }
	static std::string NewsText(const SpaceportPanel &p)
	{
		return p.newsMessage ? p.newsMessage->GetText() : std::string();
	}
	static const Information &NewsInfo(const SpaceportPanel &p) { return p.newsInfo; }

	// Shops.
	static const Ship *SelectedShip(const ShopPanel &p) { return p.selectedShip; }
	static const Outfit *SelectedOutfit(const ShopPanel &p) { return p.selectedOutfit; }
	static Ship *PlayerShip(const ShopPanel &p) { return p.playerShip; }
	static const std::set<Ship *> &PlayerShips(const ShopPanel &p) { return p.playerShips; }
	static bool SidebarActive(const ShopPanel &p) { return p.activePane == ShopPanel::ShopPane::Sidebar; }
	static bool IsOutfitter(const ShopPanel &p) { return p.isOutfitter; }
	static const std::map<std::string, std::vector<std::string>> &Catalog(const ShopPanel &p) { return p.catalog; }
	static const CategoryList &Categories(const ShopPanel &p) { return p.categories; }
	static const std::set<std::string> &Collapsed(const ShopPanel &p) { return p.collapsed; }
	static bool HasItem(const ShopPanel &p, const std::string &name) { return p.HasItem(name); }
	static int64_t LicenseCost(const ShopPanel &p, const Outfit *outfit) { return p.LicenseCost(outfit); }
	static const Planet *ShopPlanet(const ShopPanel &p) { return p.planet; }
	static void SelectShipModel(ShopPanel &p, const Ship *ship) { p.selectedShip = ship; p.selectedOutfit = nullptr; }
	static void SelectOutfitItem(ShopPanel &p, const Outfit *outfit) { p.selectedOutfit = outfit; p.selectedShip = nullptr; }
	static void SelectPlayerShip(ShopPanel &p, Ship *ship) { p.SideSelect(ship); }
	static void ShopCheckSelection(ShopPanel &p) { p.CheckSelection(); }
	static void ShopAutoScroll(ShopPanel &p) { p.delayedAutoScroll = true; }

	// Boarding.
	static int BoardingSelected(const BoardingPanel &p) { return p.selected; }
	static const std::string &BoardingMessages(const BoardingPanel &p) { return p.messages; }
	static bool IsCapturing(const BoardingPanel &p) { return p.isCapturing; }
	static bool CanTake(const BoardingPanel &p) { return p.CanTake() == BoardingPanel::CanTakeResult::CAN_TAKE; }
	static std::string CannotTakeReason(const BoardingPanel &p)
	{
		switch(p.CanTake())
		{
			case BoardingPanel::CanTakeResult::TARGET_YOURS: return "This ship is yours.";
			case BoardingPanel::CanTakeResult::NO_SELECTION: return "Nothing selected.";
			case BoardingPanel::CanTakeResult::NO_CARGO_SPACE: return "Not enough cargo space.";
			case BoardingPanel::CanTakeResult::CAN_TAKE: return "";
			default: return "Cannot take this.";
		}
	}
	static bool CanCapture(const BoardingPanel &p) { return p.CanCapture(); }
	static bool CanAttack(const BoardingPanel &p) { return p.CanAttack(); }
	static bool CanExit(const BoardingPanel &p) { return p.CanExit(); }
	static const std::shared_ptr<Ship> &You(const BoardingPanel &p) { return p.you; }
	static const CaptureOdds &AttackOdds(const BoardingPanel &p) { return p.attackOdds; }
	static const CaptureOdds &DefenseOdds(const BoardingPanel &p) { return p.defenseOdds; }
	static const std::shared_ptr<Ship> &Victim(const BoardingPanel &p) { return p.victim; }
	static std::string PlunderLine(const BoardingPanel &p, int index)
	{
		if(index < 0 || index >= static_cast<int>(p.plunder.size()))
			return std::string();
		const auto &item = p.plunder[static_cast<size_t>(index)];
		std::string line = item.Name() + ", " + std::to_string(item.Count()) + (item.Count() == 1 ? " unit" : " units");
		if(!item.Size().empty())
			line += ", size " + item.Size();
		line += ", value " + item.Value();
		if(item.InCargo())
			line += ", in cargo";
		return line;
	}
	static int PlunderCount(const BoardingPanel &p) { return static_cast<int>(p.plunder.size()); }

	// Preferences.
	static char PreferencesPage(const PreferencesPanel &p) { return p.page; }
	static int PreferencesSelected(const PreferencesPanel &p) { return p.selected; }
	static int PreferencesEditing(const PreferencesPanel &p) { return p.editing; }
	static const std::string &PreferencesSelectedItem(const PreferencesPanel &p) { return p.selectedItem; }
	static const std::string &PreferencesSelectedPlugin(const PreferencesPanel &p) { return p.selectedPlugin; }
	static std::vector<Command> ControlZones(const PreferencesPanel &p)
	{
		std::vector<Command> result;
		for(const auto &zone : p.zones)
			result.push_back(zone.Value());
		return result;
	}
	static std::vector<std::string> SettingZones(const PreferencesPanel &p)
	{
		std::vector<std::string> result;
		for(const auto &zone : p.prefZones)
			result.push_back(zone.Value());
		return result;
	}
	static std::vector<std::string> PluginZones(const PreferencesPanel &p)
	{
		std::vector<std::string> result;
		for(const auto &zone : p.pluginZones)
			result.push_back(zone.Value());
		return result;
	}
	static int ControlsPage(const PreferencesPanel &p) { return p.currentControlsPage; }
	// Screen area of the selected entry on the current preferences page.
	static Rectangle PreferencesRow(const PreferencesPanel &p, int index)
	{
		size_t i = static_cast<size_t>(index);
		if(p.page == 'c' && i < p.zones.size())
			return p.zones[i];
		if(p.page == 's' && i < p.prefZones.size())
			return p.prefZones[i];
		if(p.page == 'p' && i < p.pluginZones.size())
			return p.pluginZones[i];
		return Rectangle();
	}
	static int SettingsPage(const PreferencesPanel &p) { return p.currentSettingsPage; }
	static void HandleSetting(PreferencesPanel &p, const std::string &name)
	{
		for(const auto &zone : p.prefZones)
			if(zone.Value() == name)
			{
				p.HandleSettingsString(name, zone.Center());
				return;
			}
	}

	// Player info panel.
	static const InfoPanelState &InfoState(const PlayerInfoPanel &p) { return p.panelState; }

	// Ship info panel.
	static const InfoPanelState &InfoState(const ShipInfoPanel &p) { return p.panelState; }
	static const ShipInfoDisplay &Info(const ShipInfoPanel &p) { return p.info; }

	// Logbook and message log.
	static std::string LogbookSelection(const LogbookPanel &p) { return p.selection.first + " " + p.selection.second; }

	// Gamerules.
	static char GamerulesPage(const GamerulesPanel &p) { return p.page; }
	static int GamerulesSelected(const GamerulesPanel &p) { return p.selectedIndex; }
	static const std::string &GamerulesSelectedItem(const GamerulesPanel &p) { return p.selectedItem; }
	static std::vector<std::string> GameruleZones(const GamerulesPanel &p)
	{
		std::vector<std::string> result;
		for(const auto &zone : p.gameruleZones)
			result.push_back(zone.Value());
		return result;
	}
	static const std::string &SelectedPreset(const GamerulesPanel &p) { return p.selectedPreset; }
	static int GamerulesPageNumber(const GamerulesPanel &p) { return p.currentGamerulesPage; }
	static Rectangle GamerulesRow(const GamerulesPanel &p, int index)
	{
		size_t i = static_cast<size_t>(index);
		if(p.page == 'g' && i < p.gameruleZones.size())
			return p.gameruleZones[i];
		if(p.page == 'p' && i < p.presetZones.size())
			return p.presetZones[i];
		return Rectangle();
	}
	static size_t GamerulesRowCount(const GamerulesPanel &p)
	{
		return p.page == 'g' ? p.gameruleZones.size() : p.presetZones.size();
	}

	static const std::string &Empty() { static const std::string empty; return empty; }
};
