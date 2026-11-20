/* AccessCapture.cpp
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

#include "AccessCapture.h"

#include "../Color.h"

#include <algorithm>
#include <cmath>
#include <utility>

using namespace std;

namespace {
	struct PanelRecord {
		const Panel *panel = nullptr;
		vector<AccessCapture::Item> texts;
		vector<AccessCapture::Item> buttons;
	};

	vector<PanelRecord> building;
	vector<PanelRecord> completed;
	const Panel *current = nullptr;
	int suppressed = 0;
	bool inFrame = false;

	PanelRecord *Record()
	{
		if(!inFrame || !current)
			return nullptr;
		if(building.empty() || building.back().panel != current)
		{
			// A panel may be drawn in several pieces; find an existing record.
			for(PanelRecord &record : building)
				if(record.panel == current)
					return &record;
			building.push_back(PanelRecord{current, {}, {}});
		}
		return &building.back();
	}

	const PanelRecord *Find(const Panel *panel)
	{
		for(const PanelRecord &record : completed)
			if(record.panel == panel)
				return &record;
		return nullptr;
	}

	bool IsBlank(const string &text)
	{
		return all_of(text.begin(), text.end(), [](char c) { return c == ' ' || c == '\t' || c == '\n'; });
	}
}



void AccessCapture::BeginFrame()
{
	building.clear();
	current = nullptr;
	inFrame = true;
}



void AccessCapture::EndFrame()
{
	completed.swap(building);
	building.clear();
	current = nullptr;
	inFrame = false;
}



void AccessCapture::SetPanel(const Panel *panel)
{
	current = panel;
}



void AccessCapture::Text(const string &text, double x, double y, const Color &color)
{
	if(suppressed || IsBlank(text))
		return;
	PanelRecord *record = Record();
	if(!record)
		return;
	const float *c = color.Get();
	Item item;
	item.text = text;
	item.x = x;
	item.y = y;
	item.brightness = (c[0] + c[1] + c[2]) / 3. * (c[3] > 0. ? 1. : 0.);
	// Do not record the same text at the same place twice in a frame.
	for(const Item &it : record->texts)
		if(it.text == text && fabs(it.x - x) < .5 && fabs(it.y - y) < .5)
			return;
	record->texts.push_back(std::move(item));
}



void AccessCapture::Button(const string &label, char key, bool active, double x, double y,
	double width, double height)
{
	PanelRecord *record = Record();
	if(!record || IsBlank(label))
		return;
	Item item;
	item.text = label;
	item.x = x;
	item.y = y;
	item.width = width;
	item.height = height;
	item.isButton = true;
	item.isActive = active;
	item.key = key;
	record->buttons.push_back(std::move(item));
}



void AccessCapture::Suppress(bool suppress)
{
	suppressed += suppress ? 1 : -1;
	if(suppressed < 0)
		suppressed = 0;
}



vector<AccessCapture::Item> AccessCapture::Raw(const Panel *panel)
{
	const PanelRecord *record = Find(panel);
	return record ? record->texts : vector<Item>();
}



vector<AccessCapture::Item> AccessCapture::Buttons(const Panel *panel)
{
	const PanelRecord *record = Find(panel);
	return record ? record->buttons : vector<Item>();
}



vector<const Panel *> AccessCapture::Panels()
{
	vector<const Panel *> result;
	for(const PanelRecord &record : completed)
		result.push_back(record.panel);
	return result;
}



vector<AccessCapture::Item> AccessCapture::Lines(const Panel *panel)
{
	const PanelRecord *record = Find(panel);
	if(!record)
		return {};

	vector<Item> items = record->texts;
	// Mark text that sits on a button as that button.
	for(Item &item : items)
		for(const Item &button : record->buttons)
			if(item.x >= button.x - 2. && item.x <= button.x + button.width + 2.
					&& item.y >= button.y - 4. && item.y <= button.y + button.height + 2.)
			{
				item.isButton = true;
				item.isActive = button.isActive;
				item.key = button.key;
				break;
			}

	// Sort into reading order: rows (within a tolerance), then left to right.
	stable_sort(items.begin(), items.end(), [](const Item &a, const Item &b)
	{
		if(fabs(a.y - b.y) > 6.)
			return a.y < b.y;
		return a.x < b.x;
	});

	// Merge items on the same row into one line. Buttons stay separate so
	// that they can be activated individually.
	vector<Item> lines;
	for(const Item &item : items)
	{
		if(!lines.empty() && !item.isButton && !lines.back().isButton
				&& fabs(lines.back().y - item.y) <= 6.)
		{
			lines.back().text += ", " + item.text;
			lines.back().brightness = max(lines.back().brightness, item.brightness);
		}
		else
			lines.push_back(item);
	}
	return lines;
}
