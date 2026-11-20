/* AccessFormat.h
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

#include <cstdint>
#include <string>
#include <vector>

class Angle;
class Point;



// Text formatting helpers for spoken output. Everything here is pure (no game
// state), so it is covered by unit tests.
namespace AccessFormat {
	// Relative bearing in degrees in the range (-180, 180]; positive is to the
	// right (clockwise) of the given facing. "delta" is target minus observer.
	double RelativeDegrees(const Angle &facing, const Point &delta);
	// Spoken bearing: "dead ahead", "12 degrees right", "at 4 o'clock", ...
	std::string Bearing(double relativeDegrees);
	// Clock position (1 to 12) for a relative bearing.
	int ClockPosition(double relativeDegrees);
	// Compass-style direction on the map (screen up is north).
	std::string CompassDirection(const Point &delta);
	// "1,250 units".
	std::string Distance(double distance);
	// "1,234 credits".
	std::string Credits(int64_t credits);
	// "75 percent".
	std::string Percent(double fraction);
	// Rounded integer with thousands separators.
	std::string Integer(double value);
	// Join with commas and "and".
	std::string List(const std::vector<std::string> &items);
	// "1 ton" / "3 tons".
	std::string Plural(int64_t count, const std::string &singular, const std::string &plural = "");
	// Describe a velocity relative to the ship's facing: "moving forward",
	// "drifting left", "stationary".
	std::string Motion(const Angle &facing, const Point &velocity, double speedPerSecond);
	// Remove a trailing period.
	std::string TrimPeriod(const std::string &text);
	// Capitalise the first letter only.
	std::string Sentence(const std::string &text);
	// Make a key name friendlier for speech ("Return" -> "Enter").
	std::string FriendlyKey(const std::string &keyName);
}
