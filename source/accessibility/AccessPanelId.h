/* AccessPanelId.h
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

// Place at the very end of a Panel subclass body. It lets the accessibility
// layer recognise the panel type (the game is compiled without RTTI) and read
// its state through AccessInspector. It adds no data members.
#define ES_ACCESSIBLE_PANEL(Name) \
public: \
	const char *AccessibilityId() const noexcept override { return #Name; } \
private: \
	friend class AccessInspector;
