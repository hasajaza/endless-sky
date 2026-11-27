/* AccessFlight.cpp
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

#include "AccessFlight.h"

#include "AccessAudio.h"
#include "AccessContext.h"
#include "AccessFormat.h"
#include "AccessInspector.h"
#include "AccessSpeech.h"

#include "../DistanceMap.h"
#include "../FormationPattern.h"
#include "../Account.h"
#include "../audio/Audio.h"
#include "../AsteroidField.h"
#include "../CargoHold.h"
#include "../Command.h"
#include "../Date.h"
#include "../Engine.h"
#include "../Flotsam.h"
#include "../GameData.h"
#include "../Government.h"
#include "../Hardpoint.h"
#include "../Information.h"
#include "../Minable.h"
#include "../Mission.h"
#include "../Outfit.h"
#include "../Personality.h"
#include "../Planet.h"
#include "../PlayerInfo.h"
#include "../Point.h"
#include "../Port.h"
#include "../Preferences.h"
#include "../Projectile.h"
#include "../Ship.h"
#include "../ShipEvent.h"
#include "../ShipJumpNavigation.h"
#include "../ScanType.h"
#include "../StellarObject.h"
#include "../System.h"
#include "../UI.h"
#include "../Weapon.h"
#include "../ship/ShipAICache.h"
#include "../text/Format.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

#include <cstring>

using namespace std;

namespace {
	// Monitor state.
	const System *lastSystem = nullptr;
	// "What to do next" hint after arriving in a system or taking off:
	// frames left before it is spoken (0 = none pending).
	int hintFrames = 0;
	bool wasOnPlanet = false;
	// Pause the game on arrival in a system (setting), and whether the
	// current pause was made by that.
	bool arrivalPause = true;
	bool pausedOnArrival = false;
	bool motionAnnouncements = true;
	// Motion monitor state.
	string lastCompass;
	bool wasTurning = false;
	bool fullSpeedSaid = true;
	bool movingSaid = false;
	// Escorts chosen with Control+Shift+N; the game's Shift+N replaces the
	// selection with one ship, so the mod keeps the list it is building.
	vector<weak_ptr<Ship>> markedEscorts;

	const Ship *lastFlagship = nullptr;
	weak_ptr<Ship> lastTargetShip;
	bool hadTargetShip = false;
	const StellarObject *lastTargetStellar = nullptr;
	weak_ptr<Minable> lastTargetAsteroid;
	bool hadTargetAsteroid = false;
	const System *lastTargetSystem = nullptr;
	double lastShields = 1.;
	double lastHull = 1.;
	int shieldBand = 4;
	int hullBand = 4;
	bool wasDisabled = false;
	bool warnedFuel = false;
	bool wasHyperspacing = false;
	int inRangeState = -1;
	int damageCueTimer = 0;
	int attackTimer = 0;
	int missileTimer = 0;
	set<const Ship *> knownAttackers;
	int step = 0;

	// Secondary weapons and pause state.
	set<const Outfit *> lastSecondary;
	map<const Outfit *, int> lastAmmo;
	bool secondaryKnown = false;
	bool wasPaused = false;

	// Beacon.
	bool beaconOn = false;
	// Whether "beacon silent, no target" was already said for this gap.
	bool beaconSilentSaid = false;
	int beaconTimer = 0;

	PlayerInfo &Player()
	{
		return AccessContext::Player();
	}

	double Speed(const Ship &ship)
	{
		// Velocities are per frame; the game runs at 60 frames per second.
		return ship.Velocity().Length() * 60.;
	}

	// Distance phrase. Exact figures require a range finder for ships; the
	// rest of the time the distance is rounded the way a pilot would estimate
	// it from the screen.
	string RoughDistance(double distance, bool exact)
	{
		if(exact)
			return AccessFormat::Distance(distance);
		double rounded = distance < 1000. ? round(distance / 50.) * 50. : round(distance / 100.) * 100.;
		return "about " + AccessFormat::Distance(rounded);
	}

	string Where(const Ship &flagship, const Point &position, bool exact = true)
	{
		Point delta = position - flagship.Position();
		double degrees = AccessFormat::RelativeDegrees(flagship.Facing(), delta);
		return AccessFormat::Bearing(degrees) + ", " + RoughDistance(delta.Length(), exact);
	}

	string StellarName(const StellarObject &object)
	{
		if(!object.DisplayName().empty() && object.GetPlanet())
			return object.DisplayName();
		if(object.IsStar())
			return "star";
		if(object.IsStation())
			return "station";
		if(object.IsMoon())
			return "moon";
		return "planet";
	}

	string StellarKind(const StellarObject &object)
	{
		if(object.IsStar())
			return "star";
		if(object.GetPlanet())
			return object.GetPlanet()->Noun();
		if(object.IsStation())
			return "station";
		if(object.IsMoon())
			return "moon";
		return "planet";
	}

	// Landing status as the HUD would describe it.
	string LandingStatus(const StellarObject &object, const Ship &flagship)
	{
		const Planet *planet = object.GetPlanet();
		if(!planet || !object.HasValidPlanet())
		{
			const string &message = object.LandingMessage();
			return message.empty() ? "cannot land" : AccessFormat::TrimPeriod(message);
		}
		if(!planet->IsAccessible(&flagship))
			return "cannot land";
		if(planet->IsWormhole())
			return "wormhole";
		if(!planet->CanLand(flagship))
			return "landing refused";
		string status = "can land";
		if(planet->HasServices() && planet->GetPort().CanRecharge(Port::RechargeType::Fuel, true))
			status += ", fuel available";
		return status;
	}

	enum class Kind { YOURS, FRIENDLY, HOSTILE_ATTACKING, HOSTILE, DISABLED };

	Kind Classify(const Ship &ship)
	{
		if(ship.IsDisabled())
			return Kind::DISABLED;
		if(ship.IsYours() || (ship.GetPersonality().IsEscort() && !ship.GetGovernment()->IsEnemy()))
			return Kind::YOURS;
		if(!ship.GetGovernment()->IsEnemy())
			return Kind::FRIENDLY;
		const auto target = ship.GetTargetShip();
		if(target && target->IsYours())
			return Kind::HOSTILE_ATTACKING;
		return Kind::HOSTILE;
	}

	string KindName(Kind kind)
	{
		switch(kind)
		{
			case Kind::YOURS: return "your fleet";
			case Kind::FRIENDLY: return "friendly";
			case Kind::HOSTILE_ATTACKING: return "hostile, attacking you";
			case Kind::HOSTILE: return "hostile";
			default: return "disabled";
		}
	}

	// Ships the player can see: in this system, and not cloaked (unless yours).
	vector<shared_ptr<Ship>> VisibleShips(const Engine &engine)
	{
		vector<shared_ptr<Ship>> result;
		const Ship *flagship = Player().Flagship();
		const System *system = Player().GetSystem();
		for(const shared_ptr<Ship> &ship : AccessInspector::Ships(engine))
		{
			if(ship.get() == flagship || ship->GetSystem() != system || ship->IsDestroyed())
				continue;
			if(ship->IsCloaked() && !ship->IsYours())
				continue;
			if(!ship->IsTargetable() && !ship->IsYours() && !ship->IsDisabled())
				continue;
			result.push_back(ship);
		}
		return result;
	}

	template<class T>
	void SortByDistance(vector<T> &list, const Point &from)
	{
		sort(list.begin(), list.end(), [&from](const T &a, const T &b)
		{
			return a->Position().Distance(from) < b->Position().Distance(from);
		});
	}

	bool HasRangeFinder(const Ship &target)
	{
		return Player().CanScan(target.shared_from_this()) & ScanType::RANGE;
	}

	// Select the given object the way a left click would.
	bool SelectShip(const weak_ptr<Ship> &weak)
	{
		shared_ptr<Ship> ship = weak.lock();
		Ship *flagship = Player().Flagship();
		if(!ship || !flagship || !ship->IsTargetable() || ship->GetSystem() != Player().GetSystem())
		{
			AccessSpeech::Say("That ship can no longer be targeted.", SpeechPriority::REQUESTED);
			return false;
		}
		UI::PlaySound(UI::UISound::TARGET);
		flagship->SetTargetShip(ship);
		if(ship->IsYours())
			Player().SelectEscort(ship.get(), false);
		return true;
	}

	bool SelectStellar(const StellarObject *object)
	{
		Ship *flagship = Player().Flagship();
		if(!flagship || !object || !object->GetPlanet() || !object->HasValidPlanet()
				|| !object->GetPlanet()->IsAccessible(flagship))
		{
			AccessSpeech::Say("You cannot select that as a landing target.", SpeechPriority::REQUESTED);
			return false;
		}
		Engine *engine = AccessContext::GetEngine();
		UI::PlaySound(UI::UISound::TARGET);
		flagship->SetTargetStellar(object);
		if(engine && flagship->Commands().Has(Command::LAND))
			AccessInspector::GetAI(*engine).DisengageAutopilot();
		return true;
	}

	bool SelectAsteroid(const weak_ptr<Minable> &weak)
	{
		shared_ptr<Minable> minable = weak.lock();
		Ship *flagship = Player().Flagship();
		if(!minable || !flagship)
			return false;
		if(!Player().HasScanner(ScanType::ASTEROID) || !(Player().CanScan(minable) & ScanType::ASTEROID))
		{
			AccessSpeech::Say("You need an asteroid scanner in range to select asteroids.",
				SpeechPriority::REQUESTED);
			return false;
		}
		UI::PlaySound(UI::UISound::TARGET);
		flagship->SetTargetAsteroid(minable);
		return true;
	}

	string ShipLine(const Ship &ship, const Ship &flagship)
	{
		string line = AccessFlight::DescribeShip(ship, true);
		line += ", " + Where(flagship, ship.Position(), HasRangeFinder(ship));
		return line;
	}

	string AsteroidName(const shared_ptr<const Minable> &minable)
	{
		// The asteroid's type is shown once it can be scanned; the HUD shows it
		// as "<name> <noun>" when it is targeted.
		if(Player().CanScan(minable) & ScanType::ASTEROID)
			return minable->DisplayName() + " " + minable->Noun();
		return "minable asteroid";
	}

	// Announce a newly selected target.
	void AnnounceTargets(Ship &flagship)
	{
		shared_ptr<Ship> target = flagship.GetTargetShip();
		bool hasTarget = static_cast<bool>(target);
		if(target != lastTargetShip.lock() || hasTarget != hadTargetShip)
		{
			if(target)
				AccessSpeech::Say("Target: " + ShipLine(*target, flagship), SpeechPriority::FOCUS);
			else if(hadTargetShip)
				AccessSpeech::Say("No ship targeted.", SpeechPriority::INFO);
			lastTargetShip = target;
			hadTargetShip = hasTarget;
			inRangeState = -1;
		}

		const StellarObject *stellar = flagship.GetTargetStellar();
		if(stellar != lastTargetStellar)
		{
			if(stellar)
				AccessSpeech::Say("Navigation target: " + StellarName(*stellar) + ", " + StellarKind(*stellar)
					+ ", " + LandingStatus(*stellar, flagship) + ", " + Where(flagship, stellar->Position()),
					SpeechPriority::FOCUS);
			lastTargetStellar = stellar;
		}

		shared_ptr<Minable> asteroid = flagship.GetTargetAsteroid();
		bool hasAsteroid = static_cast<bool>(asteroid);
		if(asteroid != lastTargetAsteroid.lock() || hasAsteroid != hadTargetAsteroid)
		{
			if(asteroid)
				AccessSpeech::Say("Asteroid target: " + AsteroidName(asteroid) + ", "
					+ Where(flagship, asteroid->Position()), SpeechPriority::FOCUS);
			lastTargetAsteroid = asteroid;
			hadTargetAsteroid = hasAsteroid;
		}

		const System *system = flagship.GetTargetSystem();
		if(system != lastTargetSystem)
		{
			if(system && !flagship.IsEnteringHyperspace())
			{
				string name = Player().KnowsName(*system) ? system->DisplayName() + " system" : "unexplored system";
				// A jump without a route is warned about when the game says it
				// engages the autopilot (see AccessFlight::OnGameMessage).
				if(!Player().TravelPlan().empty())
					AccessSpeech::Say("Hyperspace target: " + name, SpeechPriority::INFO);
			}
			lastTargetSystem = system;
		}
	}

	int Band(double fraction)
	{
		if(fraction <= .1)
			return 0;
		if(fraction <= .25)
			return 1;
		if(fraction <= .5)
			return 2;
		if(fraction <= .75)
			return 3;
		return 4;
	}

	void MonitorCondition(Ship &flagship)
	{
		double shields = flagship.ShieldFraction();
		double hull = flagship.HullFraction();

		// Damage earcons, rate limited.
		if(damageCueTimer)
			--damageCueTimer;
		if(AccessContext::AudioCues() && !damageCueTimer)
		{
			if(hull < lastHull - .002)
			{
				AccessAudio::Play(AccessAudio::Cue::HULL_HIT);
				damageCueTimer = 15;
			}
			else if(shields < lastShields - .002)
			{
				AccessAudio::Play(AccessAudio::Cue::SHIELD_HIT);
				damageCueTimer = 15;
			}
		}

		int newShieldBand = Band(shields);
		if(newShieldBand < shieldBand && newShieldBand < 3 && flagship.Attributes().Get("shields"))
		{
			static const char *TEXT[3] = {"Shields critical", "Shields at 25 percent", "Shields at half"};
			AccessSpeech::Say(TEXT[newShieldBand], newShieldBand == 0 ? SpeechPriority::CRITICAL
				: SpeechPriority::IMPORTANT);
		}
		// Bands only reset once the value has recovered well above them.
		if(newShieldBand < shieldBand || Band(max(0., shields - .08)) > shieldBand)
			shieldBand = newShieldBand;

		int newHullBand = Band(hull);
		if(newHullBand < hullBand && newHullBand < 3)
		{
			static const char *TEXT[3] = {"Hull critical", "Hull at 25 percent", "Hull at half"};
			AccessSpeech::Say(TEXT[newHullBand], newHullBand <= 1 ? SpeechPriority::CRITICAL
				: SpeechPriority::IMPORTANT);
		}
		if(newHullBand < hullBand || Band(max(0., hull - .08)) > hullBand)
			hullBand = newHullBand;

		bool disabled = flagship.IsDisabled();
		if(disabled && !wasDisabled)
			AccessSpeech::Say("Your flagship is disabled.", SpeechPriority::CRITICAL);
		else if(!disabled && wasDisabled)
			AccessSpeech::Say("Your flagship is operational again.", SpeechPriority::IMPORTANT);
		wasDisabled = disabled;

		lastShields = shields;
		lastHull = hull;

		// Fuel for at least one jump.
		double jumpFuel = flagship.JumpNavigation().JumpFuel();
		bool lowFuel = jumpFuel > 0. && flagship.Attributes().Get("fuel capacity")
			&& flagship.FuelFraction() * flagship.Attributes().Get("fuel capacity") < jumpFuel;
		if(lowFuel && !warnedFuel)
			AccessSpeech::Say("Not enough fuel for a hyperspace jump.", SpeechPriority::IMPORTANT);
		warnedFuel = lowFuel;

		bool hyperspacing = flagship.IsHyperspacing();
		if(hyperspacing && !wasHyperspacing)
			AccessSpeech::Say("Entering hyperspace.", SpeechPriority::INFO);
		wasHyperspacing = hyperspacing;
	}

	void MonitorThreats(const Engine &engine, Ship &flagship)
	{
		if(attackTimer)
			--attackTimer;
		if(missileTimer)
			--missileTimer;

		// Hostile ships that have started attacking the player's fleet.
		vector<shared_ptr<Ship>> newAttackers;
		set<const Ship *> attackers;
		for(const shared_ptr<Ship> &ship : VisibleShips(engine))
			if(Classify(*ship) == Kind::HOSTILE_ATTACKING)
			{
				attackers.insert(ship.get());
				if(!knownAttackers.contains(ship.get()))
					newAttackers.push_back(ship);
			}
		if(!newAttackers.empty() && !attackTimer)
		{
			SortByDistance(newAttackers, flagship.Position());
			string text;
			string line = ShipLine(*newAttackers.front(), flagship);
			size_t pos = line.find(", hostile, attacking you");
			if(pos != string::npos)
				line.erase(pos, strlen(", hostile, attacking you"));
			if(newAttackers.size() == 1)
				text = "Under attack by " + line;
			else
				text = "Under attack by " + to_string(newAttackers.size()) + " hostile ships, nearest " + line;
			AccessSpeech::Say(text, SpeechPriority::IMPORTANT);
			attackTimer = 120;
			for(const auto &ship : newAttackers)
				knownAttackers.insert(ship.get());
		}
		// Forget ships that stopped attacking (so a renewed attack is announced).
		for(auto it = knownAttackers.begin(); it != knownAttackers.end(); )
		{
			if(!attackers.contains(*it))
				it = knownAttackers.erase(it);
			else
				++it;
		}

		// Incoming guided missiles aimed at the flagship.
		if(!missileTimer)
		{
			const Projectile *closest = nullptr;
			double closestDistance = 1e9;
			int count = 0;
			for(const Projectile &projectile : AccessInspector::Projectiles(engine))
			{
				if(projectile.IsDead() || projectile.Target() != &flagship || !projectile.MissileStrength())
					continue;
				++count;
				double distance = projectile.Position().Distance(flagship.Position());
				if(distance < closestDistance)
				{
					closestDistance = distance;
					closest = &projectile;
				}
			}
			if(closest)
			{
				string text = count == 1 ? "Incoming missile, " : to_string(count) + " incoming missiles, nearest ";
				text += Where(flagship, closest->Position(), false);
				AccessSpeech::Say(text, SpeechPriority::IMPORTANT);
				missileTimer = 240;
			}
		}
	}

	void MonitorRange(Ship &flagship)
	{
		shared_ptr<Ship> target = flagship.GetTargetShip();
		if(!target || target->GetSystem() != flagship.GetSystem() || !target->IsTargetable()
				|| !target->GetGovernment()->IsEnemy())
		{
			inRangeState = -1;
			return;
		}
		double range = max(flagship.GetAICache().GunRange(), flagship.GetAICache().TurretRange());
		if(range <= 0.)
			return;
		double distance = target->Position().Distance(flagship.Position());
		int state = inRangeState;
		if(distance < range * .97)
			state = 1;
		else if(distance > range * 1.05)
			state = 0;
		if(state != inRangeState && inRangeState != -1)
		{
			if(state == 1)
			{
				if(AccessContext::AudioCues())
					AccessAudio::Play(AccessAudio::Cue::IN_RANGE);
				if(AccessSpeech::Verbosity() >= 1)
					AccessSpeech::Say("Target in weapons range.", SpeechPriority::INFO);
			}
			else
				AccessSpeech::Say("Target out of range.", SpeechPriority::INFO);
		}
		else if(inRangeState == -1 && state != -1)
		{
			// First evaluation after selecting a target: silent.
		}
		inRangeState = state;
	}

	void HandleEvents(const list<ShipEvent> &events, Ship &flagship)
	{
		shared_ptr<Ship> target = flagship.GetTargetShip();
		for(const ShipEvent &event : events)
		{
			const shared_ptr<Ship> &victim = event.Target();
			if(!victim)
				continue;
			int type = event.Type();
			bool isTarget = (victim == target);
			bool isOurs = victim->IsYours();
			string name = victim->GivenName().empty() ? victim->DisplayModelName() : victim->GivenName();
			if(type & ShipEvent::DESTROY)
			{
				if(isOurs && victim.get() != &flagship)
					AccessSpeech::Say("Your ship " + name + " was destroyed.", SpeechPriority::IMPORTANT);
				else if(isTarget)
					AccessSpeech::Say("Target destroyed.", SpeechPriority::IMPORTANT);
			}
			else if(type & ShipEvent::DISABLE)
			{
				if(isOurs && victim.get() != &flagship)
					AccessSpeech::Say("Your ship " + name + " was disabled.", SpeechPriority::IMPORTANT);
				else if(isTarget)
					AccessSpeech::Say("Target disabled. It can now be boarded.", SpeechPriority::IMPORTANT);
			}
		}
	}

	string SecondaryText(const Ship &flagship, const set<const Outfit *> &selected)
	{
		if(selected.empty())
			return "No secondary weapon selected";
		vector<string> names;
		for(const Outfit *outfit : selected)
		{
			string name = outfit->DisplayName();
			const Weapon *weapon = outfit->GetWeapon().get();
			if(weapon && weapon->Ammo())
				name += ", " + to_string(flagship.OutfitCount(weapon->Ammo())) + " " + weapon->Ammo()->DisplayName();
			names.push_back(name);
		}
		return "Secondary weapon: " + AccessFormat::List(names);
	}

	void MonitorWeapons(const Engine &engine, Ship &flagship)
	{
		const set<const Outfit *> &selected = Player().SelectedSecondaryWeapons();
		if(secondaryKnown && selected != lastSecondary)
			AccessSpeech::Say(SecondaryText(flagship, selected), SpeechPriority::FOCUS);
		lastSecondary = selected;

		// Running out of ammunition for a selected weapon.
		for(const Outfit *outfit : selected)
		{
			const Weapon *weapon = outfit->GetWeapon().get();
			if(!weapon || !weapon->Ammo())
				continue;
			int count = flagship.OutfitCount(weapon->Ammo());
			auto it = lastAmmo.find(weapon->Ammo());
			if(secondaryKnown && it != lastAmmo.end() && it->second > 0 && count == 0)
				AccessSpeech::Say("Out of " + weapon->Ammo()->DisplayName() + ".", SpeechPriority::IMPORTANT);
			lastAmmo[weapon->Ammo()] = count;
		}
		secondaryKnown = true;

		bool paused = engine.IsPaused();
		if(paused != wasPaused)
			AccessSpeech::Say(paused ? "Paused." : "Resumed.", SpeechPriority::FOCUS);
		wasPaused = paused;
	}

	void UpdateBeacon(Ship &flagship)
	{
		if(!beaconOn || !AccessContext::AudioCues())
			return;
		if(beaconTimer > 0)
		{
			--beaconTimer;
			return;
		}
		Point position;
		bool found = false;
		shared_ptr<Ship> target = flagship.GetTargetShip();
		if(target && target->GetSystem() == flagship.GetSystem() && target->IsTargetable())
		{
			position = target->Position();
			found = true;
		}
		else if(flagship.GetTargetStellar())
		{
			position = flagship.GetTargetStellar()->Position();
			found = true;
		}
		else if(flagship.GetTargetAsteroid())
		{
			position = flagship.GetTargetAsteroid()->Position();
			found = true;
		}
		else if(flagship.GetTargetSystem() && flagship.GetSystem())
		{
			// Direction of the hyperspace link, far away.
			position = flagship.Position()
				+ (flagship.GetTargetSystem()->Position() - flagship.GetSystem()->Position()).Unit() * 100000.;
			found = true;
		}
		if(!found)
		{
			beaconTimer = 60;
			if(!beaconSilentSaid)
			{
				AccessSpeech::Say("Beacon silent: no target.", SpeechPriority::INFO);
				beaconSilentSaid = true;
			}
			return;
		}
		beaconSilentSaid = false;
		Point delta = position - flagship.Position();
		double distance = delta.Length();
		double degrees = AccessFormat::RelativeDegrees(flagship.Facing(), delta);
		double closeness = max(0., min(1., 1. - distance / 6000.));
		AccessAudio::Play(AccessAudio::Cue::BEACON, degrees, closeness);
		// Faster pings when close or when nearly lined up.
		int interval = 18 + static_cast<int>(42. * (1. - closeness));
		if(fabs(degrees) < 6.)
			interval = min(interval, 20);
		beaconTimer = interval;
	}

	vector<shared_ptr<Ship>> Selected()
	{
		vector<shared_ptr<Ship>> result;
		for(const weak_ptr<Ship> &weak : Player().SelectedEscorts())
			if(shared_ptr<Ship> ship = weak.lock())
				if(!ship->IsDestroyed())
					result.push_back(ship);
		return result;
	}

	string ShipNames(const vector<shared_ptr<Ship>> &ships)
	{
		vector<string> names;
		for(const shared_ptr<Ship> &ship : ships)
			names.push_back(ship->GivenName());
		return AccessFormat::List(names);
	}

	string GroupNames(const set<Ship *> &group)
	{
		vector<shared_ptr<Ship>> ships;
		for(const shared_ptr<Ship> &ship : Player().Ships())
			if(group.contains(ship.get()) && !ship->IsDestroyed())
				ships.push_back(ship);
		return ShipNames(ships);
	}

	// Speak the compass direction while the player turns, the exact heading
	// when they let go, and when the ship reaches full speed or stops.
	void MonitorMotion(Ship &flagship)
	{
		string compass = AccessFormat::CompassDirection(flagship.Facing().Unit());
		Command keys;
		keys.ReadKeyboard();
		// Shift+Down is the autopilot stop, which turns the ship by itself.
		// Shift may be let go a moment before Down, so remember the whole press.
		static bool stopPress = false;
		if(!keys.Has(Command::BACK))
			stopPress = false;
		else if(keys.Has(Command::SHIFT))
			stopPress = true;
		bool turning = keys.Has(Command::LEFT) || keys.Has(Command::RIGHT)
			|| (keys.Has(Command::BACK) && !stopPress);
		double speed = Speed(flagship);
		double top = flagship.MaxVelocity() * 60.;
		bool jumping = flagship.IsEnteringHyperspace() || flagship.IsHyperspacing();
		if(motionAnnouncements && !jumping)
		{
			if(turning && compass != lastCompass && !lastCompass.empty())
				AccessSpeech::Say(compass, SpeechPriority::FOCUS);
			else if(!turning && wasTurning && !stopPress)
				AccessSpeech::Say("Facing " + compass + ", "
					+ to_string(static_cast<int>(lround(flagship.Facing().AbsDegrees()))) + " degrees.",
					SpeechPriority::FOCUS);
			if(top > 0. && speed >= top * .98 && !fullSpeedSaid)
			{
				AccessSpeech::Say("Full speed, " + AccessFormat::Integer(speed) + ".", SpeechPriority::INFO);
				fullSpeedSaid = true;
			}
			if(speed >= 30.)
				movingSaid = true;
			else if(speed < 2. && movingSaid && flagship.Zoom() >= 1. && !flagship.Commands().Has(Command::LAND))
			{
				AccessSpeech::Say("Stopped.", SpeechPriority::INFO);
				movingSaid = false;
			}
		}
		if(top > 0. && speed < top * .9)
			fullSpeedSaid = false;
		if(jumping)
		{
			fullSpeedSaid = true;
			movingSaid = false;
		}
		lastCompass = compass;
		wasTurning = turning;
	}

	void Reset()
	{
		knownAttackers.clear();
		lastTargetShip.reset();
		hadTargetShip = false;
		lastTargetStellar = nullptr;
		lastTargetAsteroid.reset();
		hadTargetAsteroid = false;
		lastTargetSystem = nullptr;
		inRangeState = -1;
		secondaryKnown = false;
		lastAmmo.clear();
		lastCompass.clear();
		wasTurning = false;
		fullSpeedSaid = true;
		movingSaid = false;
	}

	string Fraction(double value)
	{
		return AccessFormat::Percent(value);
	}
}



void AccessFlight::Step(Engine &engine, const list<ShipEvent> &events, bool isActive)
{
	++step;
	PlayerInfo &player = Player();
	Ship *flagship = player.Flagship();
	if(!player.IsLoaded() || !flagship || player.IsDead())
	{
		lastFlagship = nullptr;
		return;
	}
	bool arrived = lastSystem && lastFlagship == flagship && player.GetSystem() != lastSystem;
	bool tookOff = wasOnPlanet && !player.GetPlanet() && lastFlagship == flagship;
	wasOnPlanet = player.GetPlanet();
	// Wait a moment so the hint comes after the game's own arrival messages.
	if(arrived)
	{
		hintFrames = 150;
		// Give the player time to listen and decide what to do here.
		bool &paused = AccessInspector::TimePaused(engine);
		if(arrivalPause && !paused)
		{
			paused = true;
			Audio::Pause();
			pausedOnArrival = true;
		}
	}
	else if(tookOff)
		hintFrames = 90;
	if(flagship != lastFlagship || player.GetSystem() != lastSystem)
	{
		bool newShip = flagship != lastFlagship;
		Reset();
		// Do not announce whatever happened to be targeted on arrival.
		lastTargetShip = flagship->GetTargetShip();
		hadTargetShip = static_cast<bool>(flagship->GetTargetShip());
		lastTargetStellar = flagship->GetTargetStellar();
		lastTargetAsteroid = flagship->GetTargetAsteroid();
		hadTargetAsteroid = static_cast<bool>(flagship->GetTargetAsteroid());
		lastTargetSystem = flagship->GetTargetSystem();
		lastSystem = player.GetSystem();
		lastFlagship = flagship;
		if(newShip)
		{
			lastShields = flagship->ShieldFraction();
			lastHull = flagship->HullFraction();
			shieldBand = Band(lastShields);
			hullBand = Band(lastHull);
			wasDisabled = flagship->IsDisabled();
		}
	}
	// Only monitor while the player is actually flying.
	if(player.GetPlanet())
		hintFrames = 0;
	if(player.GetPlanet() || !isActive)
		return;
	if(hintFrames > 0 && --hintFrames == 0 && !flagship->IsEnteringHyperspace())
	{
		string hint = NextStepHint();
		if(pausedOnArrival && AccessInspector::TimePaused(engine))
			hint += (hint.empty() ? "" : " ") + string("Game paused. Press ")
				+ (Command::PAUSE.HasBinding() ? AccessFormat::FriendlyKey(Command::PAUSE.KeyName()) : string("the pause key"))
				+ " to continue.";
		pausedOnArrival = false;
		if(!hint.empty())
			AccessSpeech::Say(hint, SpeechPriority::INFO);
	}

	AnnounceTargets(*flagship);
	MonitorWeapons(engine, *flagship);
	MonitorCondition(*flagship);
	MonitorThreats(engine, *flagship);
	MonitorRange(*flagship);
	HandleEvents(events, *flagship);
	UpdateBeacon(*flagship);
	MonitorMotion(*flagship);
}



string AccessFlight::DescribeShip(const Ship &ship, bool brief)
{
	string name = ship.GivenName();
	string model = ship.DisplayModelName();
	string text = name.empty() ? model : name + ", " + model;
	const Government *gov = ship.GetGovernment();
	if(!ship.IsYours() && gov)
		text += ", " + gov->DisplayName();
	text += ", " + KindName(Classify(ship));
	if(ship.GetPersonality().IsTarget())
		text += ", mission target";
	if(!brief && ship.IsTargetable())
	{
		text += ", shields " + Fraction(ship.ShieldFraction()) + ", hull " + Fraction(ship.HullFraction());
	}
	return text;
}



string AccessFlight::Location()
{
	PlayerInfo &player = Player();
	if(!player.IsLoaded() || !player.GetSystem())
		return "No pilot is loaded.";
	const System &system = *player.GetSystem();
	string text = "In the " + system.DisplayName() + " system";
	if(system.GetGovernment())
		text += ", " + system.GetGovernment()->DisplayName() + " territory";
	text += ". Date " + player.GetDate().ToString() + ".";
	if(player.GetPlanet())
	{
		text += " Landed on " + player.GetPlanet()->DisplayName() + ".";
		return text;
	}
	const Ship *flagship = player.Flagship();
	if(!flagship)
		return text;
	// Nearest stellar object as a reference point.
	const StellarObject *nearest = nullptr;
	double best = 1e18;
	for(const StellarObject &object : system.Objects())
		if(object.HasSprite())
		{
			double distance = object.Position().Distance(flagship->Position()) - object.Radius();
			if(distance < best)
			{
				best = distance;
				nearest = &object;
			}
		}
	if(nearest)
	{
		double distance = nearest->Position().Distance(flagship->Position());
		if(distance < nearest->Radius())
			text += " Over " + StellarName(*nearest) + ".";
		else
			text += " Nearest object: " + StellarName(*nearest) + ", " + Where(*flagship, nearest->Position()) + ".";
	}
	// Distance from the system centre helps when leaving to jump.
	text += " " + AccessFormat::Distance(flagship->Position().Length()) + " from the system centre, "
		+ AccessFormat::CompassDirection(flagship->Position()) + " of it.";
	return text;
}



string AccessFlight::Status()
{
	PlayerInfo &player = Player();
	const Ship *flagship = player.Flagship();
	if(!player.IsLoaded() || !flagship)
		return "No flagship.";
	// Most needed first; the ship's name is in Control+F, motion in Control+O.
	vector<string> parts;
	if(flagship->IsDisabled())
		parts.push_back("disabled");
	if(flagship->Attributes().Get("shields"))
		parts.push_back("shields " + Fraction(flagship->ShieldFraction()));
	parts.push_back("hull " + Fraction(flagship->HullFraction()));
	double fuel = flagship->FuelFraction() * flagship->Attributes().Get("fuel capacity");
	double jumpFuel = flagship->JumpNavigation().JumpFuel();
	if(jumpFuel > 0.)
		parts.push_back("fuel for " + AccessFormat::Plural(static_cast<int64_t>(fuel / jumpFuel), "jump"));
	else
		parts.push_back("fuel " + AccessFormat::Integer(fuel));
	parts.push_back("energy " + Fraction(flagship->EnergyFraction()));
	parts.push_back("heat " + Fraction(flagship->HeatFraction()) + (flagship->IsOverheated() ? ", overheated" : ""));
	if(!player.GetPlanet())
		parts.push_back(SpeedText(Speed(*flagship), flagship->MaxVelocity() * 60.));
	if(flagship->Crew() < flagship->RequiredCrew())
		parts.push_back("crew " + to_string(flagship->Crew()) + " of " + to_string(flagship->RequiredCrew())
			+ " required");
	string text = AccessFormat::List(parts) + ".";
	return AccessFormat::Sentence(text);
}



string AccessFlight::SpeedText(double speed, double topSpeed)
{
	if(speed < 2.)
		return "stopped";
	string text = "speed " + AccessFormat::Integer(speed);
	if(topSpeed > 0.)
	{
		if(speed >= topSpeed * .98)
			text += ", full speed";
		else
			text += " of " + AccessFormat::Integer(topSpeed);
	}
	return text;
}



string AccessFlight::Motion()
{
	const Ship *flagship = Player().Flagship();
	if(!flagship)
		return "No flagship.";
	if(Player().GetPlanet())
		return "You are landed.";
	// Speed first, then where the ship is going and where it points.
	double speed = Speed(*flagship);
	string text = AccessFormat::Sentence(SpeedText(speed, flagship->MaxVelocity() * 60.));
	if(speed >= 2.)
	{
		string motion = AccessFormat::Motion(flagship->Facing(), flagship->Velocity(), speed);
		text += ". " + AccessFormat::Sentence(motion.substr(0, motion.find(" at ")))
			+ ", travelling " + AccessFormat::CompassDirection(flagship->Velocity());
	}
	text += ". Facing " + AccessFormat::CompassDirection(flagship->Facing().Unit()) + ", "
		+ to_string(static_cast<int>(lround(flagship->Facing().AbsDegrees()))) + " degrees.";
	if(flagship->IsThrusting())
		text += " Thrusting.";
	if(flagship->IsReversing())
		text += " Reverse thrust.";
	if(speed >= 2.)
		text += " Shift+Down to stop.";
	return text;
}



string AccessFlight::Target()
{
	PlayerInfo &player = Player();
	const Ship *flagship = player.Flagship();
	Engine *engine = AccessContext::GetEngine();
	if(!flagship || !engine)
		return "No flagship.";
	const Information &info = AccessInspector::HudInfo(*engine);
	shared_ptr<Ship> target = flagship->GetTargetShip();
	shared_ptr<Minable> asteroid = flagship->GetTargetAsteroid();
	if(target)
	{
		vector<string> parts;
		parts.push_back(DescribeShip(*target, true));
		if(target->GetSystem() != player.GetSystem())
		{
			parts.push_back("not in this system");
			return AccessFormat::List(parts) + ".";
		}
		if(!target->IsTargetable())
		{
			parts.push_back("not currently visible");
			return AccessFormat::List(parts) + ".";
		}
		parts.push_back(Where(*flagship, target->Position(), HasRangeFinder(*target)));
		parts.push_back("shields " + Fraction(info.BarValue("target shields")));
		parts.push_back("hull " + Fraction(info.BarValue("target hull")));
		if(info.HasCondition("target crew display") || info.HasCondition("mobility crew display"))
			parts.push_back("crew " + info.GetString("target crew"));
		if(info.HasCondition("target energy display"))
			parts.push_back("energy " + info.GetString("target energy"));
		if(info.HasCondition("target fuel display"))
			parts.push_back("fuel " + info.GetString("target fuel"));
		if(info.HasCondition("target thermal display"))
			parts.push_back("heat " + info.GetString("target heat"));
		if(info.HasCondition("target weapon range display"))
			parts.push_back("gun range " + info.GetString("target gun") + ", turret range "
				+ info.GetString("target turret"));
		Point relative = target->Velocity() - flagship->Velocity();
		if(relative.Length() * 60. >= 5.)
		{
			Point delta = target->Position() - flagship->Position();
			double closing = -relative.Dot(delta.Unit()) * 60.;
			parts.push_back(closing > 0. ? "closing at " + AccessFormat::Integer(closing) + " units per second"
				: "moving away at " + AccessFormat::Integer(-closing) + " units per second");
		}
		double range = max(flagship->GetAICache().GunRange(), flagship->GetAICache().TurretRange());
		if(range > 0.)
			parts.push_back(target->Position().Distance(flagship->Position()) <= range
				? "in weapons range" : "out of weapons range");
		if(target->IsDisabled())
			parts.push_back("can be boarded");
		return AccessFormat::List(parts) + ".";
	}
	if(asteroid)
		return "Asteroid target: " + AsteroidName(asteroid) + ", " + Where(*flagship, asteroid->Position())
			+ ", condition " + Fraction(asteroid->HullFraction()) + ".";
	return "No target. Use the game's target keys to select one, or the review cursor to choose from nearby objects.";
}



void AccessFlight::SetArrivalPause(bool on)
{
	arrivalPause = on;
}



bool AccessFlight::ArrivalPause()
{
	return arrivalPause;
}



void AccessFlight::SetMotionAnnouncements(bool on)
{
	motionAnnouncements = on;
}



bool AccessFlight::MotionAnnouncements()
{
	return motionAnnouncements;
}



string AccessFlight::FormationDescription(const string &name)
{
	// From the comments in data/formations.txt. Formations only arrange where
	// escorts fly around the flagship; they change no ship values.
	static const map<string, string> DESCRIPTIONS = {
		{"Atomic", "groups in front and on both sides, keeping their directions on the map instead of turning with you"},
		{"Circle", "a circle around you"},
		{"Delta (tailing)", "a triangle behind you"},
		{"Diamond", "a diamond shape around you"},
		{"Echelon (left)", "a diagonal line behind you to the left"},
		{"Echelon (right)", "a diagonal line behind you to the right"},
		{"Hexagon", "a six-sided ring around you"},
		{"Line (sides)", "a line to your left and right"},
		{"Line (trailing)", "a line behind you"},
		{"Nabla (trailing)", "a triangle with you at the front and its point behind"},
		{"Square", "a square block around you"},
		{"Vic", "a V shape, two legs angled back behind you"},
		{"X", "an X shape that turns with you"},
		{"X stable", "an X or plus shape that does not turn with you"},
	};
	auto it = DESCRIPTIONS.find(name);
	return it == DESCRIPTIONS.end() ? string() : it->second;
}



bool AccessFlight::OnGameMessage(const string &text)
{
	// "2 ships will assume "Vic" formation." is a low priority message that
	// is otherwise only spoken at the verbose level, but it is the only answer
	// to Shift+G, so always say it, with what the formation looks like.
	static const string ASSUME = "formation.";
	size_t quote = text.find(" will assume \"");
	if(quote != string::npos && text.ends_with("\" " + ASSUME))
	{
		size_t start = quote + strlen(" will assume \"");
		string name = text.substr(start, text.size() - start - ASSUME.size() - 2);
		string description = FormationDescription(name);
		AccessSpeech::Say(text + (description.empty() ? string() : " Escorts fly in " + description + ".")
			+ " Shift+G again for the next formation.", SpeechPriority::REQUESTED);
		return true;
	}
	if(text.find(" will no longer fly in formation.") != string::npos)
	{
		AccessSpeech::Say(text + " Shift+G again to start with the first formation.", SpeechPriority::REQUESTED);
		return true;
	}
	// With no planned route, the jump key picks whichever linked system the
	// ship is facing. The game only says "Engaging autopilot to jump to the
	// X system", so make the consequence clear.
	static const string JUMP = "Engaging autopilot to jump to the ";
	PlayerInfo &player = Player();
	if(!text.starts_with(JUMP) || !player.TravelPlan().empty() || !player.Flagship())
		return false;
	if(player.Flagship()->JumpsRemaining() <= 0)
		return false;
	string name = text.substr(JUMP.size());
	if(name.ends_with("."))
		name.pop_back();
	if(name == "selected star system")
		name = "unexplored system";
	AccessSpeech::Say("Warning: no route planned. Your ship is about to jump to the " + name
		+ " ahead of you, which is not on a route. Press an arrow key now to cancel.", SpeechPriority::IMPORTANT);
	return true;
}



string AccessFlight::NextStepHint()
{
	PlayerInfo &player = Player();
	const System *system = player.GetSystem();
	if(!player.IsLoaded() || !system || player.GetPlanet())
		return string();
	auto keyName = [](const Command &command, const string &fallback)
	{
		return command.HasBinding() ? AccessFormat::FriendlyKey(command.KeyName()) : fallback;
	};
	// Mission planets in this system.
	vector<string> here;
	set<const Planet *> added;
	bool missionsElsewhere = false;
	for(const Mission &mission : player.Missions())
	{
		if(!mission.IsVisible())
			continue;
		vector<const Planet *> planets;
		if(mission.Destination())
			planets.push_back(mission.Destination());
		for(const Planet *planet : mission.Stopovers())
			if(!mission.VisitedStopovers().contains(planet))
				planets.push_back(planet);
		for(const Planet *planet : planets)
		{
			if(planet->IsInSystem(system))
			{
				if(added.insert(planet).second)
					here.push_back(planet->DisplayName());
			}
			else
				missionsElsewhere = true;
		}
	}
	const vector<const System *> &plan = player.TravelPlan();
	const Ship *flagship = player.Flagship();
	string fuel;
	if(flagship)
	{
		int jumps = flagship->JumpsRemaining();
		if(jumps <= 0)
			fuel = " No fuel left for jumping: land on an inhabited planet to refuel.";
		else if(jumps < 2)
			fuel = " Fuel for 1 jump only: land on an inhabited planet to refuel.";
	}
	if(!here.empty())
		return (plan.empty() ? "End of your route. " : string()) + "Your mission destination "
			+ AccessFormat::List(here) + " is in this system. Press "
			+ keyName(Command::LAND, "the land key") + " once: the autopilot flies there and lands."
			+ (plan.empty() ? " Do not press " + keyName(Command::JUMP, "the jump key") + " here." : string())
			+ fuel;
	if(!plan.empty())
	{
		const System *next = plan.back();
		string name = player.KnowsName(*next) ? next->DisplayName() : string("unexplored system");
		if(flagship && flagship->JumpsRemaining() <= 0)
			return "Next jump: " + name + ", but you have no fuel left. Land on an inhabited planet here to refuel"
				" first: press " + keyName(Command::LAND, "the land key") + ".";
		return "Next jump: " + name + ". Press " + keyName(Command::JUMP, "the jump key") + " to jump." + fuel;
	}
	if(!fuel.empty())
		return fuel.substr(1);
	if(missionsElsewhere)
		return "No route planned. Press " + keyName(Command::MAP, "the map key")
			+ " for the map, then Left and Right arrows to choose your mission destination and Enter to plan the route.";
	return string();
}



string AccessFlight::Destination()
{
	PlayerInfo &player = Player();
	const Ship *flagship = player.Flagship();
	if(!player.IsLoaded())
		return "No pilot is loaded.";
	vector<string> parts;
	if(flagship && flagship->GetTargetStellar() && !player.GetPlanet())
	{
		const StellarObject &object = *flagship->GetTargetStellar();
		string mode = flagship->Commands().Has(Command::LAND) ? "Landing on " : "Navigation target ";
		parts.push_back(mode + StellarName(object) + ", " + LandingStatus(object, *flagship) + ", "
			+ Where(*flagship, object.Position()));
	}
	const vector<const System *> &plan = player.TravelPlan();
	if(!plan.empty())
	{
		const System *final = plan.front();
		string finalName = player.KnowsName(*final) ? final->DisplayName() : "an unexplored system";
		string route = "Route to " + finalName + ", " + AccessFormat::Plural(static_cast<int64_t>(plan.size()), "jump");
		const System *next = plan.back();
		route += ". Next: " + (player.KnowsName(*next) ? next->DisplayName() : string("unexplored system"));
		if(flagship && player.GetSystem() && !player.GetPlanet())
		{
			Point direction = next->Position() - player.GetSystem()->Position();
			route += ", " + AccessFormat::Bearing(AccessFormat::RelativeDegrees(flagship->Facing(), direction));
		}
		if(flagship)
		{
			// Fuel needed along the route, using the game's jump costs.
			double fuel = flagship->FuelFraction() * flagship->Attributes().Get("fuel capacity");
			double needed = 0.;
			const System *from = player.GetSystem();
			for(auto it = plan.rbegin(); it != plan.rend(); ++it)
			{
				needed += flagship->JumpNavigation().GetCheapestJumpType(from, *it).second;
				from = *it;
			}
			route += ". Fuel needed " + AccessFormat::Integer(needed) + ", you have " + AccessFormat::Integer(fuel);
			if(needed > fuel)
				route += ". Not enough fuel for the whole route";
		}
		if(player.TravelDestination())
			route += ". Destination planet " + player.TravelDestination()->DisplayName();
		parts.push_back(route);
	}
	else if(flagship && flagship->GetTargetSystem())
	{
		const System *system = flagship->GetTargetSystem();
		parts.push_back("Hyperspace target " + (player.KnowsName(*system) ? system->DisplayName() + " system"
			: string("unexplored system")));
	}
	string hint = NextStepHint();
	if(parts.empty())
		return hint.empty() ? "No destination. Open the map to plan a route, or select a landing target." : hint;
	return AccessFormat::List(parts) + "." + (hint.empty() ? string() : " " + hint);
}



string AccessFlight::Nearby()
{
	PlayerInfo &player = Player();
	const Ship *flagship = player.Flagship();
	if(!flagship || !player.GetSystem())
		return "No flagship.";
	vector<const StellarObject *> objects;
	for(const StellarObject &object : player.GetSystem()->Objects())
		if(object.HasSprite() && (object.GetPlanet() || object.IsStar()))
			objects.push_back(&object);
	if(objects.empty())
		return "No planets or stations in this system.";
	sort(objects.begin(), objects.end(), [flagship](const StellarObject *a, const StellarObject *b)
	{
		return a->Position().Distance(flagship->Position()) < b->Position().Distance(flagship->Position());
	});
	vector<string> lines;
	bool landed = player.GetPlanet();
	for(const StellarObject *object : objects)
	{
		string line = StellarName(*object) + ", " + StellarKind(*object);
		if(object->GetPlanet())
			line += ", " + LandingStatus(*object, *flagship);
		if(!landed)
			line += ", " + Where(*flagship, object->Position());
		lines.push_back(line);
	}
	if(landed)
		return "Landed on " + player.GetPlanet()->DisplayName() + ". Objects in this system: "
			+ AccessFormat::List(lines) + ".";
	return to_string(lines.size()) + " objects. " + AccessFormat::List(lines) + ".";
}



string AccessFlight::Hostiles()
{
	Engine *engine = AccessContext::GetEngine();
	const Ship *flagship = Player().Flagship();
	if(!engine || !flagship)
		return "No flagship.";
	vector<shared_ptr<Ship>> hostiles;
	for(const shared_ptr<Ship> &ship : VisibleShips(*engine))
	{
		Kind kind = Classify(*ship);
		if(kind == Kind::HOSTILE || kind == Kind::HOSTILE_ATTACKING)
			hostiles.push_back(ship);
	}
	if(hostiles.empty())
		return "No hostile ships in sight.";
	SortByDistance(hostiles, flagship->Position());
	vector<string> lines;
	for(size_t i = 0; i < hostiles.size() && i < 6; ++i)
		lines.push_back(ShipLine(*hostiles[i], *flagship));
	string text = AccessFormat::Plural(static_cast<int64_t>(hostiles.size()), "hostile ship") + ". "
		+ AccessFormat::List(lines) + ".";
	if(hostiles.size() > 6)
		text += " Use the review cursor for the full list.";
	return text;
}



string AccessFlight::Fleet()
{
	PlayerInfo &player = Player();
	if(!player.IsLoaded())
		return "No pilot is loaded.";
	const Ship *flagship = player.Flagship();
	vector<string> lines;
	int parked = 0;
	for(const shared_ptr<Ship> &ship : player.Ships())
	{
		if(ship->IsDestroyed())
			continue;
		if(ship->IsParked())
		{
			++parked;
			continue;
		}
		string line = ship->GivenName() + ", " + ship->DisplayModelName();
		if(ship.get() == flagship)
			line += ", flagship";
		else if(ship->GetFormationPattern())
			line += ", " + ship->GetFormationPattern()->TrueName() + " formation";
		vector<string> inGroups;
		for(int group = 0; group < 10; ++group)
			if(player.GetEscortGroup(group).contains(ship.get()))
				inGroups.push_back(to_string(group));
		if(!inGroups.empty())
			line += string(inGroups.size() == 1 ? ", group " : ", groups ") + AccessFormat::List(inGroups);
		line += ", shields " + Fraction(ship->ShieldFraction()) + ", hull " + Fraction(ship->HullFraction());
		if(ship->IsDisabled())
			line += ", disabled";
		if(ship->GetSystem() != player.GetSystem())
			line += ", in " + (ship->GetSystem() ? ship->GetSystem()->DisplayName() : string("another system"));
		else if(flagship && ship.get() != flagship && !player.GetPlanet() && ship->GetSystem() == flagship->GetSystem())
			line += ", " + Where(*flagship, ship->Position());
		lines.push_back(line);
	}
	string text = lines.empty() ? string("No active ships.")
		: AccessFormat::Plural(static_cast<int64_t>(lines.size()), "active ship") + ". "
			+ AccessFormat::List(lines) + ".";
	Engine *engine = AccessContext::GetEngine();
	if(engine && !player.GetPlanet())
	{
		int friendly = 0;
		for(const shared_ptr<Ship> &ship : VisibleShips(*engine))
			if(Classify(*ship) == Kind::FRIENDLY)
				++friendly;
		if(friendly)
			text += " " + AccessFormat::Plural(friendly, "other friendly ship") + " in sight.";
	}
	if(parked)
		text += " " + AccessFormat::Plural(parked, "parked ship") + ".";
	vector<shared_ptr<Ship>> selected = Selected();
	if(!selected.empty())
		text += " Selected for orders: " + ShipNames(selected) + ".";
	// Escort orders currently configured.
	vector<string> settings;
	if(Preferences::Has("Escorts expend ammo"))
		settings.push_back(Preferences::Has("Escorts use ammo frugally") ? "escorts use ammo frugally"
			: "escorts use ammo freely");
	else
		settings.push_back("escorts do not use ammo");
	text += " " + AccessFormat::Sentence(AccessFormat::List(settings)) + ".";
	return text;
}



string AccessFlight::ToggleEscort()
{
	PlayerInfo &player = Player();
	Ship *flagship = player.Flagship();
	if(!player.IsLoaded() || !flagship || player.GetPlanet())
		return "Select escorts while flying.";
	shared_ptr<Ship> target = flagship->GetTargetShip();
	if(!target || !target->IsYours() || target.get() == flagship)
		return "First target one of your escorts with Shift+N.";
	// The list being built plus whatever the game has selected now.
	vector<shared_ptr<Ship>> list;
	auto add = [&list](const shared_ptr<Ship> &ship)
	{
		if(ship && !ship->IsDestroyed() && find(list.begin(), list.end(), ship) == list.end())
			list.push_back(ship);
	};
	bool wasMarked = false;
	for(const weak_ptr<Ship> &weak : markedEscorts)
		if(shared_ptr<Ship> ship = weak.lock())
		{
			wasMarked |= ship == target;
			add(ship);
		}
	for(const shared_ptr<Ship> &ship : Selected())
		add(ship);
	if(wasMarked)
		list.erase(remove(list.begin(), list.end(), target), list.end());
	else
		add(target);
	player.ClearSelectedEscorts();
	markedEscorts.clear();
	for(const shared_ptr<Ship> &ship : list)
	{
		player.SelectEscort(ship.get(), true);
		markedEscorts.push_back(ship);
	}
	string text = (wasMarked ? "Removed " : "Added ") + target->GivenName() + ". ";
	if(list.empty())
		return text + "No escorts selected; orders go to the whole fleet.";
	return text + "Selected " + AccessFormat::Plural(static_cast<int64_t>(list.size()), "escort") + ": "
		+ ShipNames(list) + ". Control plus a digit saves them as a group.";
}



string AccessFlight::ClearEscorts()
{
	PlayerInfo &player = Player();
	if(!player.IsLoaded() || !player.Flagship() || player.GetPlanet())
		return "Select escorts while flying.";
	player.ClearSelectedEscorts();
	markedEscorts.clear();
	return "No escorts selected. Orders go to the whole fleet. Control plus a digit now empties that group.";
}



void AccessFlight::AnnounceGroup(int group, bool shift, bool control)
{
	PlayerInfo &player = Player();
	if(!player.IsLoaded() || !player.Flagship())
		return;
	string number = to_string(group);
	vector<shared_ptr<Ship>> selected = Selected();
	// The selection now replaces the list built with Control+Shift+N.
	markedEscorts.assign(selected.begin(), selected.end());
	string text;
	if(control)
	{
		set<Ship *> members = player.GetEscortGroup(group);
		if(members.empty())
			text = "Group " + number + " cleared, because no escorts were selected. To fill it, target an "
				"escort with Shift+N, add more with Control+Shift+N, then press Control+" + number + ".";
		else
			text = "Group " + number + " saved: " + GroupNames(members) + ". Press " + number
				+ " to select this group.";
	}
	else if(selected.empty())
		text = shift ? "No escorts selected; orders go to the whole fleet."
			: "Group " + number + " is empty. To make it, target an escort with Shift+N, add more with "
				"Control+Shift+N, then press Control+" + number + ".";
	else
		text = (shift ? string("Selected: ") : "Group " + number + " selected: ") + ShipNames(selected)
			+ ". Fleet orders now go to these ships.";
	AccessSpeech::Say(text, SpeechPriority::REQUESTED);
}



string AccessFlight::Cargo()
{
	PlayerInfo &player = Player();
	if(!player.IsLoaded())
		return "No pilot is loaded.";
	const CargoHold &cargo = player.Cargo();
	vector<string> parts;
	parts.push_back(AccessFormat::Credits(player.Accounts().Credits()));
	parts.push_back("cargo " + AccessFormat::Integer(cargo.Used()) + " of " + AccessFormat::Integer(cargo.Size())
		+ " tons used, " + AccessFormat::Integer(cargo.Free()) + " free");
	vector<string> items;
	for(const auto &it : cargo.Commodities())
		if(it.second)
			items.push_back(AccessFormat::Plural(it.second, "ton") + " of " + it.first);
	for(const auto &it : cargo.Outfits())
		if(it.second)
			items.push_back(to_string(it.second) + " " + it.first->DisplayName());
	for(const auto &it : cargo.MissionCargo())
		if(it.second)
			items.push_back(AccessFormat::Plural(it.second, "ton") + " of mission cargo");
	if(!items.empty())
		parts.push_back("carrying " + AccessFormat::List(items));
	if(cargo.Passengers())
		parts.push_back(AccessFormat::Plural(cargo.Passengers(), "passenger") + ", "
			+ AccessFormat::Integer(cargo.BunksFree()) + " bunks free");
	else
		parts.push_back(AccessFormat::Integer(cargo.BunksFree()) + " bunks free");
	return AccessFormat::List(parts) + ".";
}



string AccessFlight::Weapons()
{
	PlayerInfo &player = Player();
	const Ship *flagship = player.Flagship();
	if(!flagship)
		return "No flagship.";
	map<string, int> primary;
	map<string, int> secondary;
	map<const Outfit *, int> ammoNeeded;
	for(const Hardpoint &hardpoint : flagship->Weapons())
	{
		const Outfit *outfit = hardpoint.GetOutfit();
		if(!outfit)
			continue;
		const Weapon *weapon = hardpoint.GetWeapon();
		bool isSecondary = weapon && weapon->Ammo();
		string name = outfit->DisplayName();
		if(hardpoint.IsTurret() && name.find("Turret") == string::npos && name.find("turret") == string::npos)
			name += " turret";
		(isSecondary ? secondary : primary)[name]++;
		if(weapon && weapon->Ammo())
			ammoNeeded[weapon->Ammo()]++;
	}
	vector<string> parts;
	if(primary.empty() && secondary.empty())
		parts.push_back("No weapons installed");
	vector<string> list;
	for(const auto &it : primary)
		list.push_back(it.second > 1 ? to_string(it.second) + " " + it.first : it.first);
	if(!list.empty())
		parts.push_back("Weapons: " + AccessFormat::List(list));
	list.clear();
	for(const auto &it : ammoNeeded)
	{
		bool selected = player.SelectedSecondaryWeapons().contains(it.first);
		list.push_back(it.first->DisplayName() + " " + to_string(flagship->OutfitCount(it.first))
			+ (selected ? ", selected" : ""));
	}
	if(!list.empty())
		parts.push_back("Ammunition: " + AccessFormat::List(list));
	double gun = flagship->GetAICache().GunRange();
	double turret = flagship->GetAICache().TurretRange();
	if(gun > 0.)
		parts.push_back("gun range " + AccessFormat::Integer(gun));
	if(turret > 0.)
		parts.push_back("turret range " + AccessFormat::Integer(turret));
	parts.push_back("energy " + Fraction(flagship->EnergyFraction()));
	parts.push_back("heat " + Fraction(flagship->HeatFraction()) + (flagship->IsOverheated() ? ", overheated" : ""));
	parts.push_back(Preferences::Has("Turrets focus fire") ? "turrets focus fire" : "turrets fire opportunistically");
	return AccessFormat::List(parts) + ".";
}



namespace {
	bool BeaconHasTarget(const Ship &flagship)
	{
		shared_ptr<Ship> target = flagship.GetTargetShip();
		return (target && target->GetSystem() == flagship.GetSystem() && target->IsTargetable())
			|| flagship.GetTargetStellar() || flagship.GetTargetAsteroid()
			|| (flagship.GetTargetSystem() && flagship.GetSystem());
	}
}



string AccessFlight::ToggleBeacon()
{
	beaconOn = !beaconOn;
	beaconTimer = 0;
	beaconSilentSaid = false;
	if(beaconOn && !AccessAudio::IsAvailable())
		return "Beacon on, but the sound system could not be started, so no beeps can be played.";
	if(beaconOn && !AccessContext::AudioCues())
		return "Beacon on, but audio cues are turned off. Press Control+F10 to turn them on.";
	if(!beaconOn)
		return "Beacon off.";
	string text = "Beacon on. It beeps toward your target ship, landing target, asteroid or hyperspace "
		"destination. Higher pitch means ahead, lower means behind.";
	const Ship *flagship = Player().Flagship();
	if(flagship && !BeaconHasTarget(*flagship))
	{
		text += " There is no target yet, so it is silent: choose one with N, R, V, L, or a route on the map.";
		beaconSilentSaid = true;
	}
	return text;
}



namespace {
	// "2 jumps north-west", "in this system", or "no known route, east".
	string TravelTo(const System *target, const DistanceMap &distance, const System *here)
	{
		if(!target || !here)
			return string();
		if(target == here)
			return "in this system";
		string direction = AccessFormat::CompassDirection(target->Position() - here->Position());
		if(!distance.HasRoute(*target))
			return "no known route, " + direction;
		return AccessFormat::Plural(distance.Days(*target), "jump") + " " + direction;
	}

	string SystemLabel(const System *system)
	{
		return system ? "the " + system->DisplayName() + " system" : string("an unknown system");
	}
}



string AccessFlight::MissionSummary(const Mission &mission, bool accepted)
{
	PlayerInfo &player = Player();
	const System *here = player.GetSystem();
	DistanceMap distance(player, here);
	vector<string> parts;
	const System *destination = mission.Destination() ? mission.Destination()->GetSystem() : nullptr;
	if(mission.Destination())
	{
		string where = "Destination " + mission.Destination()->DisplayName() + " in " + SystemLabel(destination);
		string travel = TravelTo(destination, distance, here);
		if(!travel.empty())
			where += ", " + travel;
		parts.push_back(where);
	}
	// Each stopover and waypoint still to visit, with where it is.
	for(const Planet *planet : mission.Stopovers())
		if(!mission.VisitedStopovers().contains(planet))
		{
			string where = "stopover " + planet->DisplayName() + " in " + SystemLabel(planet->GetSystem());
			string travel = TravelTo(planet->GetSystem(), distance, here);
			if(!travel.empty())
				where += ", " + travel;
			parts.push_back(where);
		}
	for(const System *system : mission.Waypoints())
		if(!mission.VisitedWaypoints().contains(system))
		{
			string where = "waypoint " + SystemLabel(system);
			string travel = TravelTo(system, distance, here);
			if(!travel.empty())
				where += ", " + travel;
			parts.push_back(where);
		}
	if(mission.Deadline())
	{
		// Days left, counted the way the game does (the deadline day itself
		// still counts).
		int days = mission.Deadline() - player.GetDate() + 1;
		string text = "deadline " + mission.Deadline().ToString() + ", " + AccessFormat::Plural(max(days, 0), "day")
			+ " left";
		// Rough travel time: stopovers first (each takes a day for landing),
		// nearest first, then the destination, as the game estimates it.
		if(destination && distance.HasRoute(*destination))
		{
			int travel = 0;
			bool known = true;
			const System *from = here;
			set<const System *> toVisit;
			for(const Planet *planet : mission.Stopovers())
				if(!mission.VisitedStopovers().contains(planet))
				{
					toVisit.insert(planet->GetSystem());
					++travel;
				}
			while(!toVisit.empty() && known)
			{
				DistanceMap step(player, from);
				const System *closest = nullptr;
				for(const System *system : toVisit)
					if(step.HasRoute(*system) && (!closest || step.Days(*system) < step.Days(*closest)))
						closest = system;
				if(!closest)
					known = false;
				else
				{
					travel += step.Days(*closest);
					from = closest;
					toVisit.erase(closest);
				}
			}
			if(known)
			{
				DistanceMap last(player, from);
				if(last.HasRoute(*destination))
				{
					travel += last.Days(*destination);
					text += ", the trip takes about " + AccessFormat::Plural(travel, "day");
					if(travel > days)
						text += ", too late to make it";
				}
			}
		}
		parts.push_back(text);
	}
	if(mission.CargoSize())
		parts.push_back(AccessFormat::Plural(mission.CargoSize(), "ton") + " of " + mission.Cargo());
	if(mission.Passengers())
		parts.push_back(AccessFormat::Plural(mission.Passengers(), "passenger"));
	if(mission.DisplayedPayment())
		parts.push_back("payment " + AccessFormat::Credits(mission.DisplayedPayment()));
	if(accepted && mission.IsFailed())
		parts.push_back("failed");
	else if(accepted && mission.IsSatisfied(player))
		parts.push_back("ready to complete");
	// For a job on offer: does it fit the trips already accepted?
	if(!accepted && destination && here)
	{
		string fit;
		int bestJumps = -1;
		for(const Mission &other : player.Missions())
		{
			if(!other.IsVisible() || !other.Destination() || !other.Destination()->GetSystem())
				continue;
			const System *target = other.Destination()->GetSystem();
			if(target == destination)
			{
				fit = "same system as your mission " + other.DisplayName();
				bestJumps = 0;
				break;
			}
			if(distance.HasRoute(*target))
			{
				vector<const System *> route = distance.Plan(*target);
				if(find(route.begin(), route.end(), destination) != route.end())
				{
					fit = "on the way to your mission " + other.DisplayName();
					bestJumps = 0;
					break;
				}
			}
			DistanceMap from(player, target);
			if(from.HasRoute(*destination) && (bestJumps < 0 || from.Days(*destination) < bestJumps))
			{
				bestJumps = from.Days(*destination);
				fit = AccessFormat::Plural(bestJumps, "jump") + " from the destination of your mission "
					+ other.DisplayName();
			}
		}
		if(!fit.empty())
			parts.push_back(fit);
	}
	// One sentence per fact: the parts contain commas themselves.
	string text;
	for(const string &part : parts)
		text += AccessFormat::Sentence(part) + ". ";
	text += mission.Description();
	return text;
}



void AccessFlight::BuildReview(vector<ReviewGroup> &groups)
{
	PlayerInfo &player = Player();
	Engine *engine = AccessContext::GetEngine();
	Ship *flagship = player.Flagship();
	if(!engine || !flagship || !player.GetSystem())
		return;
	const Point here = flagship->Position();

	// Planets and stations.
	ReviewGroup planets;
	planets.name = "Planets and stations";
	vector<const StellarObject *> objects;
	for(const StellarObject &object : player.GetSystem()->Objects())
		if(object.HasSprite() && (object.GetPlanet() || object.IsStar()))
			objects.push_back(&object);
	sort(objects.begin(), objects.end(), [&here](const StellarObject *a, const StellarObject *b)
	{
		return a->Position().Distance(here) < b->Position().Distance(here);
	});
	for(const StellarObject *object : objects)
	{
		ReviewItem item([object]()
		{
			const Ship *ship = Player().Flagship();
			if(!ship)
				return StellarName(*object);
			string text = StellarName(*object) + ", " + StellarKind(*object);
			if(object->GetPlanet())
				text += ", " + LandingStatus(*object, *ship);
			if(ship->GetTargetStellar() == object)
				text += ", selected";
			return text + ", " + Where(*ship, object->Position());
		}, [object]() { return SelectStellar(object); });
		item.detail = [object]() -> string
		{
			const Planet *planet = object->GetPlanet();
			if(!planet)
				return string();
			string text;
			if(planet->GetGovernment())
				text += "Government " + planet->GetGovernment()->DisplayName() + ". ";
			if(planet->HasNamedPort())
				text += "Has a spaceport. ";
			if(planet->HasShipyard())
				text += "Has a shipyard. ";
			if(planet->HasOutfitter())
				text += "Has an outfitter. ";
			return text + "Select it, then press the land key to land there.";
		};
		planets.items.push_back(std::move(item));
	}
	groups.push_back(std::move(planets));

	// Ships, by category.
	vector<shared_ptr<Ship>> ships = VisibleShips(*engine);
	SortByDistance(ships, here);
	ReviewGroup hostile{"Hostile ships", {}};
	ReviewGroup other{"Other ships", {}};
	ReviewGroup fleet{"Your fleet", {}};
	ReviewGroup disabled{"Disabled ships", {}};
	for(const shared_ptr<Ship> &ship : ships)
	{
		weak_ptr<Ship> weak = ship;
		ReviewItem item([weak]()
		{
			shared_ptr<Ship> s = weak.lock();
			const Ship *flag = Player().Flagship();
			if(!s || !flag)
				return string("Ship no longer present");
			string text = AccessFlight::DescribeShip(*s, false);
			if(flag->GetTargetShip() == s)
				text += ", targeted";
			if(s->GetSystem() != Player().GetSystem() || s->IsDestroyed())
				return text + ", gone";
			return text + ", " + Where(*flag, s->Position(), HasRangeFinder(*s));
		}, [weak]() { return SelectShip(weak); });
		Kind kind = Classify(*ship);
		if(kind == Kind::YOURS)
			fleet.items.push_back(std::move(item));
		else if(kind == Kind::DISABLED)
			disabled.items.push_back(std::move(item));
		else if(kind == Kind::HOSTILE || kind == Kind::HOSTILE_ATTACKING)
			hostile.items.push_back(std::move(item));
		else
			other.items.push_back(std::move(item));
	}
	groups.push_back(std::move(hostile));
	groups.push_back(std::move(other));
	groups.push_back(std::move(disabled));
	groups.push_back(std::move(fleet));

	// Asteroids that can be mined (closest twenty).
	ReviewGroup asteroids{"Minable asteroids", {}};
	vector<shared_ptr<Minable>> minables(AccessInspector::Asteroids(*engine).Minables().begin(),
		AccessInspector::Asteroids(*engine).Minables().end());
	SortByDistance(minables, here);
	for(size_t i = 0; i < minables.size() && i < 20; ++i)
	{
		weak_ptr<Minable> weak = minables[i];
		asteroids.items.emplace_back([weak]()
		{
			shared_ptr<Minable> m = weak.lock();
			const Ship *flag = Player().Flagship();
			if(!m || !flag)
				return string("Asteroid gone");
			string text = AsteroidName(m);
			if(flag->GetTargetAsteroid() == m)
				text += ", targeted";
			return text + ", " + Where(*flag, m->Position());
		}, [weak]() { return SelectAsteroid(weak); });
	}
	groups.push_back(std::move(asteroids));

	// Flotsam that can be collected.
	ReviewGroup flotsam{"Flotsam", {}};
	vector<shared_ptr<Flotsam>> pieces(AccessInspector::FlotsamList(*engine).begin(),
		AccessInspector::FlotsamList(*engine).end());
	SortByDistance(pieces, here);
	for(size_t i = 0; i < pieces.size() && i < 20; ++i)
	{
		weak_ptr<Flotsam> weak = pieces[i];
		flotsam.items.emplace_back([weak]()
		{
			shared_ptr<Flotsam> f = weak.lock();
			const Ship *flag = Player().Flagship();
			if(!f || !flag)
				return string("Flotsam gone");
			string what = f->OutfitType() ? f->OutfitType()->DisplayName() : f->CommodityType();
			return to_string(f->Count()) + " " + what + ", " + Where(*flag, f->Position());
		});
	}
	groups.push_back(std::move(flotsam));

	// Hyperspace links from this system.
	ReviewGroup links{"Hyperspace links", {}};
	const set<const System *> &neighbours = flagship->JumpNavigation().HasJumpDrive()
		? player.GetSystem()->JumpNeighbors(flagship->JumpNavigation().JumpRange()) : player.GetSystem()->Links();
	for(const System *system : neighbours)
	{
		if(!player.HasSeen(*system))
			continue;
		links.items.emplace_back([system]()
		{
			const Ship *flag = Player().Flagship();
			string name = Player().KnowsName(*system) ? system->DisplayName() : string("unexplored system");
			if(flag && flag->GetTargetSystem() == system)
				name += ", hyperspace target";
			if(!flag || !Player().GetSystem())
				return name;
			Point direction = system->Position() - Player().GetSystem()->Position();
			return name + ", " + AccessFormat::Bearing(AccessFormat::RelativeDegrees(flag->Facing(), direction))
				+ ", " + AccessFormat::CompassDirection(direction);
		});
	}
	groups.push_back(std::move(links));
}
