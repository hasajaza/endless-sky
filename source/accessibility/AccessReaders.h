/* AccessReaders.h
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

#include "AccessReader.h"

#include <memory>

class Panel;
class Rectangle;



// Factories for the readers of each group of screens. Each returns nullptr if
// the panel is not one of its types.
std::unique_ptr<AccessReader> CreateMenuReader(Panel &panel, const std::string &id);
std::unique_ptr<AccessReader> CreateDialogReader(Panel &panel, const std::string &id);
std::unique_ptr<AccessReader> CreatePlanetReader(Panel &panel, const std::string &id);
std::unique_ptr<AccessReader> CreateShopReader(Panel &panel, const std::string &id);
std::unique_ptr<AccessReader> CreateMapReader(Panel &panel, const std::string &id);
std::unique_ptr<AccessReader> CreateInfoReader(Panel &panel, const std::string &id);

// Shared helpers for readers.
namespace AccessText {
	// The captured line of screen text closest to the given y coordinate.
	std::string LineAt(const Panel &panel, double y, double tolerance = 8.);
	// The captured text inside the given screen area, left to right.
	std::string TextIn(const Panel &panel, const Rectangle &area);
	// Lines of a long text, split at paragraph breaks.
	std::vector<std::string> Paragraphs(const std::string &text);
	// True if the key is one that types text.
	bool IsTypingKey(int32_t key);
	// Describe a key press inside a text field (character typed or deleted).
	std::string EchoEdit(const std::string &before, const std::string &after);
}
