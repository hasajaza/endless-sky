/* AccessCapture.h
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

#include <string>
#include <vector>

class Color;
class Panel;



// Records, for each panel drawn in the most recent frame, the text the game
// rendered and the interface buttons it placed. This is not OCR: the strings
// come straight from the game's own text renderer and interface definitions.
// It is used as a generic "screen review" fallback and to label buttons; the
// dedicated readers use the underlying game data instead wherever possible.
class AccessCapture {
public:
	struct Item {
		std::string text;
		double x = 0.;
		double y = 0.;
		// Perceived brightness of the text colour (0 to 1); highlighted or
		// selected entries are usually drawn brighter.
		double brightness = 0.;
		// Interface button information.
		bool isButton = false;
		bool isActive = true;
		char key = '\0';
		double width = 0.;
		double height = 0.;
	};


public:
	static void BeginFrame();
	static void EndFrame();
	// Set which panel is currently being drawn (nullptr outside panels).
	static void SetPanel(const Panel *panel);

	// Hooks called by the renderer.
	static void Text(const std::string &text, double x, double y, const Color &color);
	static void Button(const std::string &label, char key, bool active, double x, double y,
		double width, double height);
	// Suppress text capture (used while a truncated string is drawn).
	static void Suppress(bool suppress);

	// Items captured for the given panel in the last completed frame, in
	// reading order (top to bottom, left to right), merged into lines.
	static std::vector<Item> Lines(const Panel *panel);
	// Raw items for the given panel, in drawing order.
	static std::vector<Item> Raw(const Panel *panel);
	// Buttons for the given panel.
	static std::vector<Item> Buttons(const Panel *panel);
	// All panels that drew something last frame, bottom to top.
	static std::vector<const Panel *> Panels();
};
