/* ReadersMap.cpp
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

// Readers for the galaxy map (all its modes) and the job board / missions map.

#include "AccessReaders.h"

#include "AccessContext.h"
#include "AccessFlight.h"
#include "AccessFormat.h"
#include "AccessInspector.h"
#include "AccessSpeech.h"

#include "../Command.h"
#include "../DistanceMap.h"
#include "../GameData.h"
#include "../Government.h"
#include "../Mission.h"
#include "../Planet.h"
#include "../PlayerInfo.h"
#include "../SDL.h"
#include "../Ship.h"
#include "../ShipJumpNavigation.h"
#include "../StellarObject.h"
#include "../System.h"
#include "../Trade.h"

#include <algorithm>
#include <set>

using namespace std;

namespace {
	string SystemName(const System *system)
	{
		if(!system)
			return "no system";
		PlayerInfo &player = AccessContext::Player();
		if(player.KnowsName(*system))
			return system->DisplayName();
		// The game names mission destinations; stopovers and waypoints are
		// named in the mission text, so name them too.
		for(const Mission &mission : player.Missions())
		{
			if(!mission.IsVisible())
				continue;
			for(const Planet *planet : mission.Stopovers())
				if(planet->IsInSystem(system))
					return system->DisplayName();
			if(mission.Waypoints().contains(system))
				return system->DisplayName();
		}
		return "unexplored system";
	}

	// Everything a sighted player can read about a system on the map.
	string SystemInfo(MapPanel &map, const System *system, bool withPlanets)
	{
		PlayerInfo &player = AccessContext::Player();
		if(!system)
			return "No system selected.";
		vector<string> parts;
		parts.push_back(SystemName(system));
		bool canView = player.CanView(*system);
		if(system == player.GetSystem())
			parts.push_back("your current system");
		if(canView && system->GetGovernment())
			parts.push_back(system->GetGovernment()->DisplayName());
		if(!player.HasVisited(*system))
			parts.push_back("not visited");
		if(player.Flagship() && player.Flagship()->IsRestrictedFrom(*system))
			parts.push_back("your flagship is restricted from entering");
		const DistanceMap &distance = AccessInspector::Distances(map);
		if(system != player.GetSystem())
		{
			if(distance.HasRoute(*system))
			{
				int days = distance.Days(*system);
				parts.push_back(AccessFormat::Plural(days, "jump") + " away");
			}
			else
				parts.push_back("no known route");
		}
		Point delta = system->Position() - (player.GetSystem() ? player.GetSystem()->Position() : Point());
		if(system != player.GetSystem())
			parts.push_back(AccessFormat::CompassDirection(delta) + " of here");
		// Missions involving this system.
		int missions = 0;
		for(const Mission &mission : player.Missions())
			if(mission.IsVisible() && ((mission.Destination() && mission.Destination()->IsInSystem(system))
					|| mission.Waypoints().contains(system)))
				++missions;
		if(missions)
			parts.push_back(AccessFormat::Plural(missions, "mission") + " here");
		// The price of the trade good chosen with Up and Down, if known.
		int commodity = AccessInspector::Commodity(map);
		const auto &commodities = GameData::Commodities();
		if(canView && commodity >= 0 && commodity < static_cast<int>(commodities.size()))
		{
			const string &name = commodities[static_cast<size_t>(commodity)].name;
			if(system->Trade(name))
				parts.push_back(name + " " + AccessFormat::Credits(system->Trade(name)));
		}
		else if(!canView && commodity >= 0 && commodity < static_cast<int>(commodities.size()))
			parts.push_back(commodities[static_cast<size_t>(commodity)].name + " price unknown");
		string text = AccessFormat::List(parts) + ".";
		if(withPlanets && canView)
		{
			vector<string> planets;
			set<const Planet *> shown;
			for(const StellarObject &object : system->Objects())
				if(object.HasSprite() && object.HasValidPlanet())
				{
					const Planet *planet = object.GetPlanet();
					if(planet->IsWormhole() || !planet->IsAccessible(player.Flagship()) || shown.contains(planet))
						continue;
					shown.insert(planet);
					string line = planet->DisplayName();
					if(player.HasVisited(*planet))
					{
						vector<string> services;
						if(planet->HasNamedPort())
							services.push_back("spaceport");
						if(planet->HasShipyard())
							services.push_back("shipyard");
						if(planet->HasOutfitter())
							services.push_back("outfitter");
						if(!services.empty())
							line += " with " + AccessFormat::List(services);
					}
					planets.push_back(line);
				}
			if(!planets.empty())
				text += " Planets: " + AccessFormat::List(planets) + ".";
		}
		return text;
	}

	string RouteInfo()
	{
		PlayerInfo &player = AccessContext::Player();
		const vector<const System *> &plan = player.TravelPlan();
		if(plan.empty())
			return "No route planned.";
		string text = "Route: " + AccessFormat::Plural(static_cast<int64_t>(plan.size()), "jump") + " to "
			+ SystemName(plan.front());
		const Ship *flagship = player.Flagship();
		if(flagship)
		{
			double fuel = flagship->FuelFraction() * flagship->Attributes().Get("fuel capacity");
			double needed = 0.;
			const System *from = player.GetSystem();
			for(auto it = plan.rbegin(); it != plan.rend(); ++it)
			{
				needed += flagship->JumpNavigation().GetCheapestJumpType(from, *it).second;
				from = *it;
			}
			text += ", needs " + AccessFormat::Integer(needed) + " fuel, you have " + AccessFormat::Integer(fuel);
			if(needed > fuel)
				text += ", not enough fuel for the whole route: land on an inhabited planet on the way to refuel";
		}
		return text + ".";
	}



	// Systems the player's visible missions still need to reach, with the
	// mission name: destinations, unvisited waypoints and stopovers.
	vector<pair<const System *, string>> MissionSystems()
	{
		vector<pair<const System *, string>> result;
		set<const System *> added;
		PlayerInfo &player = AccessContext::Player();
		for(const Mission &mission : player.Missions())
		{
			if(!mission.IsVisible())
				continue;
			vector<const System *> systems;
			if(mission.Destination() && mission.Destination()->GetSystem())
				systems.push_back(mission.Destination()->GetSystem());
			for(const System *system : mission.Waypoints())
				if(!mission.VisitedWaypoints().contains(system))
					systems.push_back(system);
			for(const Planet *planet : mission.Stopovers())
				if(!mission.VisitedStopovers().contains(planet) && planet->GetSystem())
					systems.push_back(planet->GetSystem());
			for(const System *system : systems)
				if(system != player.GetSystem() && added.insert(system).second)
					result.emplace_back(system, mission.DisplayName());
		}
		return result;
	}

	class MapReader : public AccessReader {
	public:
		MapReader(Panel &panel) : AccessReader(panel), map(static_cast<MapPanel &>(panel)) {}

		string Title() override
		{
			string id = panel.AccessibilityId();
			if(id == "MapShipyardPanel")
				return "Map of shipyards";
			if(id == "MapOutfitterPanel")
				return "Map of outfitters";
			return "Galaxy map";
		}

		void OnOpen() override
		{
			PlayerInfo &player = Player();
			string text = Title() + ". You are in " + SystemName(player.GetSystem()) + ". ";
			const System *selected = AccessInspector::SelectedSystem(map);
			if(selected && selected != player.GetSystem())
				text += "Selected: " + SystemInfo(map, selected, false) + " ";
			text += RouteInfo() + " ";
			vector<pair<const System *, string>> missions = MissionSystems();
			if(!missions.empty())
			{
				vector<string> names;
				for(const auto &it : missions)
					names.push_back(SystemName(it.first) + " for " + it.second);
				text += "Mission destinations: " + AccessFormat::List(names) + ". ";
			}
			text += "Left and Right arrows choose a destination, Enter plans the route.";
			Say(text, SpeechPriority::FOCUS);
		}

		void OnReturn() override
		{
			Say(Title() + ". Selected: " + SystemInfo(map, AccessInspector::SelectedSystem(map), false),
				SpeechPriority::FOCUS);
		}

		string Summary() override
		{
			return "Selected: " + SystemInfo(map, AccessInspector::SelectedSystem(map), true) + " " + RouteInfo();
		}

		vector<string> Keys() override
		{
			vector<string> keys = {
				"Left and Right arrows choose a destination: mission destinations first, then reachable "
					"systems, nearest first",
				"Enter plans a route to the chosen destination; then close the map and press the jump key",
				"Up and Down choose a trade good, such as Food or Medical; after that, Left and Right also say "
					"its price in each system, to find where to buy cheap and sell high",
				"Tab or the jump key add the next hyperspace link to your route; hold Shift to extend the route",
				"Backspace or Delete removes the last jump of the route",
				"F finds a system or planet by name",
				"Control plus arrows browse systems: route, nearby, missions, all known, and planets",
				"Control plus Enter plans a route to the system under the review cursor",
				"C sends your escorts to the selected system",
				"P ports, T stars, S shipyards, O outfitters and I missions map modes",
				"Plus and minus zoom, M, D or Escape close the map"};
			return keys;
		}

		bool PreKey(int32_t key, uint16_t mod) override
		{
			if(mod & (KMOD_CTRL | KMOD_ALT | KMOD_GUI | KMOD_SHIFT))
				return false;
			if(key == SDLK_LEFT || key == SDLK_RIGHT)
			{
				BuildDestinations();
				if(destinations.empty())
				{
					Say("No known systems to travel to.");
					return true;
				}
				// No wrapping: Left at the first destination and Right at the
				// last one say so and stay, so Left always goes back to the
				// destination heard before.
				int count = static_cast<int>(destinations.size());
				string edge;
				if(destinationIndex < 0)
					destinationIndex = 0;
				else if(key == SDLK_RIGHT && destinationIndex + 1 >= count)
					edge = "Last destination. ";
				else if(key == SDLK_LEFT && destinationIndex <= 0)
					edge = "First destination. ";
				else
					destinationIndex += (key == SDLK_RIGHT) ? 1 : -1;
				const auto &it = destinations[static_cast<size_t>(destinationIndex)];
				string text = edge + (it.second.empty() ? string() : "Mission " + it.second + ": ");
				Say(text + SystemInfo(map, it.first, false) + " " + to_string(destinationIndex + 1) + " of "
					+ to_string(count) + ".");
				return true;
			}
			if((key == SDLK_RETURN || key == SDLK_KP_ENTER) && destinationIndex >= 0
					&& destinationIndex < static_cast<int>(destinations.size()))
			{
				const System *system = destinations[static_cast<size_t>(destinationIndex)].first;
				AccessInspector::SetSelectedSystem(map, system);
				AccessInspector::Select(map, system);
				Later(1, [this]()
				{
					if(Player().TravelPlan().empty())
						Say(NoRouteHelp(AccessInspector::SelectedSystem(map)));
					else
						Say(RouteInfo() + (Player().GetPlanet()
							? " Now close the map with " + CloseKey() + ", press D to take off, then press "
								+ JumpKey() + " in space to jump. On a planet, " + JumpKey() + " opens the job board."
							: " Now close the map with " + CloseKey() + " and press " + JumpKey() + " to jump."));
				});
				return true;
			}
			return false;
		}

		void PostKey(int32_t key, uint16_t mod) override
		{
			if(key == SDLK_TAB || key == SDLK_BACKSPACE || key == SDLK_DELETE || Command(key).Has(Command::JUMP))
				Later(1, [this]()
				{
					Say(SystemName(AccessInspector::SelectedSystem(map)) + ". " + RouteInfo());
				});
			else if(key == SDLK_UP || key == SDLK_DOWN)
				Later(1, [this]()
				{
					string id = panel.AccessibilityId();
					if(id == "MapDetailPanel" && AccessInspector::PlanetViewSelected(static_cast<MapDetailPanel &>(panel)))
						Say(PlanetCard());
					else
						Say(Coloring());
				});
			else if(key == 'c')
				Say("Escorts ordered to " + SystemName(AccessInspector::SelectedSystem(map)) + ".");
		}

		void BuildReview(vector<ReviewGroup> &groups) override
		{
			PlayerInfo &player = Player();
			auto makeItem = [this](const System *system, const string &prefix)
			{
				ReviewItem item([this, system, prefix]()
				{
					return prefix + SystemInfo(map, system, false);
				}, [this, system]()
				{
					AccessInspector::SetSelectedSystem(map, system);
					AccessInspector::Select(map, system);
					Say("Selected " + SystemName(system) + ". " + RouteInfo(), SpeechPriority::REQUESTED);
					return true;
				});
				item.detail = [this, system]() { return SystemInfo(map, system, true); };
				return item;
			};

			// The current route.
			ReviewGroup route{"Route", {}};
			const vector<const System *> &plan = player.TravelPlan();
			int step = 1;
			for(auto it = plan.rbegin(); it != plan.rend(); ++it, ++step)
				route.items.push_back(makeItem(*it, "Jump " + to_string(step) + ": "));
			groups.push_back(std::move(route));

			// Known systems sorted by number of jumps.
			const DistanceMap &distance = AccessInspector::Distances(map);
			// Systems drawn on the map: seen ones (unexplored ones are not named).
			vector<const System *> known;
			for(const auto &it : GameData::Systems())
			{
				const System &system = it.second;
				if(system.IsValid() && !system.Inaccessible() && player.HasSeen(system))
					known.push_back(&system);
			}
			vector<const System *> nearby;
			for(const System *system : known)
				if(distance.HasRoute(*system) && system != player.GetSystem())
					nearby.push_back(system);
			sort(nearby.begin(), nearby.end(), [&distance](const System *a, const System *b)
			{
				int da = distance.Days(*a);
				int db = distance.Days(*b);
				return da != db ? da < db : a->DisplayName() < b->DisplayName();
			});
			ReviewGroup near{"Reachable systems, nearest first", {}};
			for(size_t i = 0; i < nearby.size() && i < 60; ++i)
				near.items.push_back(makeItem(nearby[i], ""));
			groups.push_back(std::move(near));

			// Mission destinations.
			ReviewGroup missions{"Mission destinations", {}};
			set<const System *> added;
			for(const Mission &mission : player.Missions())
			{
				if(!mission.IsVisible())
					continue;
				vector<const System *> systems;
				if(mission.Destination() && mission.Destination()->GetSystem())
					systems.push_back(mission.Destination()->GetSystem());
				for(const System *system : mission.Waypoints())
					if(!mission.VisitedWaypoints().contains(system))
						systems.push_back(system);
				for(const Planet *planet : mission.Stopovers())
					if(!mission.VisitedStopovers().contains(planet) && planet->GetSystem())
						systems.push_back(planet->GetSystem());
				for(const System *system : systems)
					missions.items.push_back(makeItem(system, mission.DisplayName() + ": "));
				(void)added;
			}
			groups.push_back(std::move(missions));

			// Alphabetical list.
			sort(known.begin(), known.end(), [&player](const System *a, const System *b)
			{
				return SystemName(a) < SystemName(b);
			});
			ReviewGroup all{"All known systems, alphabetical", {}};
			for(const System *system : known)
				if(player.KnowsName(*system))
					all.items.push_back(makeItem(system, ""));
			groups.push_back(std::move(all));

			// Screen details of the selected system (prices, planets).
			AddScreenText(groups, true);
		}

	private:
		// Mission destinations first, then reachable known systems by distance.
		void BuildDestinations()
		{
			if(!destinations.empty())
				return;
			PlayerInfo &player = Player();
			set<const System *> added;
			for(const auto &it : MissionSystems())
				if(added.insert(it.first).second)
					destinations.push_back(it);
			const DistanceMap &distance = AccessInspector::Distances(map);
			vector<const System *> nearby;
			for(const auto &it : GameData::Systems())
			{
				const System &system = it.second;
				if(system.IsValid() && !system.Inaccessible() && player.HasSeen(system)
						&& &system != player.GetSystem() && distance.HasRoute(system) && !added.contains(&system))
					nearby.push_back(&system);
			}
			sort(nearby.begin(), nearby.end(), [&distance](const System *a, const System *b)
			{
				int da = distance.Days(*a);
				int db = distance.Days(*b);
				return da != db ? da < db : SystemName(a) < SystemName(b);
			});
			for(const System *system : nearby)
				destinations.emplace_back(system, string());
		}

		// No known route: the systems in between are not on the star map yet.
		// Suggest the reachable system closest to the destination, and make it
		// the current choice so Enter plans a route there.
		string NoRouteHelp(const System *target)
		{
			string name = SystemName(target);
			string text = "No known route to " + name + ". The systems in between are not on your map yet. "
				"Buy a map at an outfitter, or travel toward it to discover the way.";
			if(!target)
				return text;
			BuildDestinations();
			const DistanceMap &distance = AccessInspector::Distances(map);
			int best = -1;
			double bestDistance = target->Position().Distance(Player().GetSystem()->Position());
			for(size_t i = 0; i < destinations.size(); ++i)
			{
				const System *system = destinations[i].first;
				if(system == target || !distance.HasRoute(*system))
					continue;
				double d = target->Position().Distance(system->Position());
				if(d < bestDistance)
				{
					bestDistance = d;
					best = static_cast<int>(i);
				}
			}
			if(best < 0)
				return text;
			destinationIndex = best;
			const System *closest = destinations[static_cast<size_t>(best)].first;
			return text + " The closest system you can reach in that direction is " + SystemName(closest) + ", "
				+ AccessFormat::Plural(distance.Days(*closest), "jump")
				+ " away. Press Enter again to plan a route there.";
		}

		static string CloseKey()
		{
			return Command::MAP.HasBinding() ? AccessFormat::FriendlyKey(Command::MAP.KeyName()) + " or Escape"
				: string("Escape");
		}

		static string JumpKey()
		{
			return Command::JUMP.HasBinding() ? AccessFormat::FriendlyKey(Command::JUMP.KeyName())
				: string("the jump key");
		}

		string PlanetCard()
		{
			const Planet *planet = AccessInspector::SelectedCard(static_cast<MapDetailPanel &>(panel));
			if(!planet)
				return "No planet selected.";
			string text = "Planet " + planet->DisplayName();
			if(Player().HasVisited(*planet))
			{
				vector<string> services;
				if(planet->HasNamedPort())
					services.push_back("spaceport");
				if(planet->HasShipyard())
					services.push_back("shipyard");
				if(planet->HasOutfitter())
					services.push_back("outfitter");
				if(!services.empty())
					text += ", " + AccessFormat::List(services);
			}
			else
				text += ", not visited";
			return text + ".";
		}

		string Coloring()
		{
			int commodity = AccessInspector::Commodity(map);
			const auto &commodities = GameData::Commodities();
			if(commodity >= 0 && commodity < static_cast<int>(commodities.size()))
			{
				const string &name = commodities[static_cast<size_t>(commodity)].name;
				string text = "Trade good " + name;
				const System *system = Player().GetSystem();
				if(system && Player().CanView(*system) && system->Trade(name))
					text += ": " + AccessFormat::Credits(system->Trade(name)) + " here in " + SystemName(system);
				// Also give the price at the destination chosen with Left and
				// Right, so goods can be compared without moving away from it.
				if(destinationIndex >= 0 && destinationIndex < static_cast<int>(destinations.size()))
				{
					const System *chosen = destinations[static_cast<size_t>(destinationIndex)].first;
					if(Player().CanView(*chosen) && chosen->Trade(name))
						text += ". " + AccessFormat::Credits(chosen->Trade(name)) + " in " + SystemName(chosen);
					else
						text += ". Price in " + SystemName(chosen) + " unknown: you have not been there or bought a map"
							" covering it";
					return text + ".";
				}
				return text + ". Left and Right now also say its price in each system.";
			}
			return "Map colouring changed";
		}

	private:
		MapPanel &map;
		vector<pair<const System *, string>> destinations;
		int destinationIndex = -1;
	};



	class MissionReader : public AccessReader {
	public:
		MissionReader(Panel &panel) : AccessReader(panel), missions(static_cast<MissionPanel &>(panel)) {}

		string Title() override { return "Missions"; }

		void OnOpen() override
		{
			const auto &available = AccessInspector::Available(missions);
			int accepted = 0;
			for(const Mission &mission : AccessInspector::Accepted(missions))
				accepted += mission.IsVisible();
			Say("Missions. " + AccessFormat::Plural(static_cast<int64_t>(available.size()), "job") + " available, "
				+ AccessFormat::Integer(accepted) + " accepted. " + Space() + " " + Current(),
				SpeechPriority::FOCUS);
		}

		void OnReturn() override
		{
			Say("Missions. " + Current(), SpeechPriority::FOCUS);
		}

		string Summary() override
		{
			return Space() + " " + Current();
		}

		vector<string> Keys() override
		{
			return {"Left arrow: available jobs. Right arrow: your accepted missions",
				"Up and Down arrows choose a mission", "A accepts the selected job",
				"Shift A aborts the selected accepted mission", "M or Escape close",
				"Control plus Down arrow lists all jobs and missions; Control plus Space reads one in full"};
		}

		void PostKey(int32_t key, uint16_t mod) override
		{
			if(key == SDLK_UP || key == SDLK_DOWN || key == SDLK_LEFT || key == SDLK_RIGHT)
				Say(Current());
			else if(key == 'a' && !(mod & KMOD_SHIFT))
				Later(1, [this]() { Say(Space() + " " + Current()); });
		}

		void BuildReview(vector<ReviewGroup> &groups) override
		{
			ReviewGroup available{"Available jobs", {}};
			for(const Mission &mission : AccessInspector::Available(missions))
			{
				const Mission *m = &mission;
				available.items.emplace_back([this, m]() { return Describe(*m, true); });
				available.items.back().detail = [m]() { return AccessFlight::MissionSummary(*m, false); };
			}
			groups.push_back(std::move(available));
			ReviewGroup accepted{"Accepted missions", {}};
			for(const Mission &mission : AccessInspector::Accepted(missions))
				if(mission.IsVisible())
				{
					const Mission *m = &mission;
					accepted.items.emplace_back([this, m]() { return Describe(*m, false); });
					accepted.items.back().detail = [m]() { return AccessFlight::MissionSummary(*m); };
				}
			groups.push_back(std::move(accepted));
		}

	private:
		string Space()
		{
			return AccessFormat::Integer(Player().Cargo().Free()) + " tons of cargo space and "
				+ AccessFormat::Integer(Player().Cargo().BunksFree()) + " bunks free.";
		}

		string Describe(const Mission &mission, bool isAvailable)
		{
			string text = mission.DisplayName();
			if(isAvailable && !mission.CanAccept(Player()))
				text += ", cannot accept now";
			if(mission.Deadline())
				text += ", deadline " + mission.Deadline().ToString();
			if(mission.DisplayedPayment())
				text += ", pays " + AccessFormat::Credits(mission.DisplayedPayment());
			return text;
		}

		string Current()
		{
			const auto &available = AccessInspector::Available(missions);
			const auto &accepted = AccessInspector::Accepted(missions);
			auto availableIt = AccessInspector::AvailableIt(missions);
			auto acceptedIt = AccessInspector::AcceptedIt(missions);
			if(availableIt != available.end())
			{
				int index = 1 + static_cast<int>(distance(available.begin(), availableIt));
				string text = "Available job " + to_string(index) + " of " + to_string(available.size()) + ": "
					+ availableIt->DisplayName() + ". ";
				if(AccessInspector::CanAccept(missions))
					text += "Press A to accept. ";
				else if(!availableIt->HasSpace(Player()))
					text += "Not enough cargo space or bunks to accept. ";
				else
					text += "Cannot be accepted now. ";
				return text + AccessFlight::MissionSummary(*availableIt, false);
			}
			if(acceptedIt != accepted.end())
			{
				int index = 1;
				int count = 0;
				for(auto it = accepted.begin(); it != accepted.end(); ++it)
					if(it->IsVisible())
					{
						++count;
						if(it == acceptedIt)
							index = count;
					}
				return "Accepted mission " + to_string(index) + " of " + to_string(count) + ": "
					+ acceptedIt->DisplayName() + ". " + AccessFlight::MissionSummary(*acceptedIt);
			}
			if(available.empty())
				return "No jobs are available here.";
			return "No mission selected. Use the arrow keys.";
		}

	private:
		MissionPanel &missions;
	};
}



unique_ptr<AccessReader> CreateMapReader(Panel &panel, const string &id)
{
	if(id == "MissionPanel")
		return make_unique<MissionReader>(panel);
	if(id == "MapPanel" || id == "MapDetailPanel" || id == "MapShipyardPanel" || id == "MapOutfitterPanel"
			|| id == "MapSalesPanel")
		return make_unique<MapReader>(panel);
	return nullptr;
}
