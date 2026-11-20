/* AccessFormat.cpp
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

#include "AccessFormat.h"

#include "../Angle.h"
#include "../Point.h"

#include <cmath>
#include <numbers>

using namespace std;



double AccessFormat::RelativeDegrees(const Angle &facing, const Point &delta)
{
	if(!delta.X() && !delta.Y())
		return 0.;
	// The game's angles: 0 points up the screen, positive is clockwise.
	double target = atan2(delta.X(), -delta.Y()) * 180. / numbers::pi;
	double relative = target - facing.Degrees();
	while(relative > 180.)
		relative -= 360.;
	while(relative <= -180.)
		relative += 360.;
	return relative;
}



int AccessFormat::ClockPosition(double relativeDegrees)
{
	double normalized = relativeDegrees;
	while(normalized < 0.)
		normalized += 360.;
	int hour = static_cast<int>(lround(normalized / 30.)) % 12;
	return hour ? hour : 12;
}



string AccessFormat::Bearing(double relativeDegrees)
{
	double magnitude = fabs(relativeDegrees);
	if(magnitude < 4.)
		return "dead ahead";
	if(magnitude > 176.)
		return "directly behind";
	if(magnitude < 30.)
		return to_string(static_cast<int>(lround(magnitude))) + " degrees "
			+ (relativeDegrees > 0. ? "right" : "left");
	return "at " + to_string(ClockPosition(relativeDegrees)) + " o'clock";
}



string AccessFormat::CompassDirection(const Point &delta)
{
	if(!delta.X() && !delta.Y())
		return "here";
	static const char *NAMES[8] = {"north", "north-east", "east", "south-east",
		"south", "south-west", "west", "north-west"};
	double angle = atan2(delta.X(), -delta.Y()) * 180. / numbers::pi;
	if(angle < 0.)
		angle += 360.;
	int index = static_cast<int>(lround(angle / 45.)) % 8;
	return NAMES[index];
}



string AccessFormat::Integer(double value)
{
	long long rounded = llround(value);
	bool negative = rounded < 0;
	string digits = to_string(negative ? -rounded : rounded);
	string result;
	int count = 0;
	for(auto it = digits.rbegin(); it != digits.rend(); ++it)
	{
		if(count && count % 3 == 0)
			result.insert(result.begin(), ',');
		result.insert(result.begin(), *it);
		++count;
	}
	return negative ? "minus " + result : result;
}



string AccessFormat::Distance(double distance)
{
	return Integer(distance) + (llround(distance) == 1 ? " unit" : " units");
}



string AccessFormat::Credits(int64_t credits)
{
	return Integer(static_cast<double>(credits)) + (credits == 1 ? " credit" : " credits");
}



string AccessFormat::Percent(double fraction)
{
	if(fraction < 0.)
		fraction = 0.;
	return to_string(static_cast<int>(lround(fraction * 100.))) + " percent";
}



string AccessFormat::List(const vector<string> &items)
{
	string result;
	for(size_t i = 0; i < items.size(); ++i)
	{
		if(i)
			result += (i + 1 == items.size()) ? (items.size() > 2 ? ", and " : " and ") : ", ";
		result += items[i];
	}
	return result;
}



string AccessFormat::Plural(int64_t count, const string &singular, const string &plural)
{
	return Integer(static_cast<double>(count)) + " "
		+ (count == 1 ? singular : (plural.empty() ? singular + "s" : plural));
}



string AccessFormat::Motion(const Angle &facing, const Point &velocity, double speedPerSecond)
{
	if(speedPerSecond < 1.)
		return "stationary";
	double relative = RelativeDegrees(facing, velocity);
	double magnitude = fabs(relative);
	string direction;
	if(magnitude < 20.)
		direction = "moving forward";
	else if(magnitude > 160.)
		direction = "moving backward";
	else if(magnitude < 70.)
		direction = string("moving forward and drifting ") + (relative > 0. ? "right" : "left");
	else if(magnitude > 110.)
		direction = string("moving backward and drifting ") + (relative > 0. ? "right" : "left");
	else
		direction = string("drifting ") + (relative > 0. ? "right" : "left");
	return direction + " at " + Integer(speedPerSecond) + " units per second";
}



string AccessFormat::TrimPeriod(const string &text)
{
	if(!text.empty() && text.back() == '.')
		return text.substr(0, text.size() - 1);
	return text;
}



string AccessFormat::FriendlyKey(const string &keyName)
{
	if(keyName == "Return")
		return "Enter";
	if(keyName.empty())
		return "unassigned";
	return keyName;
}



string AccessFormat::Sentence(const string &text)
{
	string result = text;
	if(!result.empty() && result[0] >= 'a' && result[0] <= 'z')
		result[0] = static_cast<char>(result[0] - 'a' + 'A');
	return result;
}
