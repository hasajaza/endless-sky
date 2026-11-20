/* AccessCrash.h
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

#include <filesystem>



// On Windows, writes "crash report.txt" in the configuration folder when the
// game crashes: the exception, the stack (module and offset of each frame) and
// what was on screen. It changes nothing else. Elsewhere it does nothing.
class AccessCrash {
public:
	static void Install(const std::filesystem::path &reportFile);
	// Record what the player is doing, for the report (main thread only).
	static void SetContext(const char *screen);
};
