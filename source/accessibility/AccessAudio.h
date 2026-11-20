/* AccessAudio.h
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



// Synthesised audio cues for orientation: a directional beacon and short
// earcons. They are generated in memory and played through the game's own
// OpenAL context, so no extra files are needed. All calls must be made on
// the main thread.
class AccessAudio {
public:
	enum class Cue {
		// Beacon ping toward the selected object (pitch drops when it is behind).
		BEACON,
		// The flagship's shields took damage.
		SHIELD_HIT,
		// The flagship's hull took damage.
		HULL_HIT,
		// The target came into weapon range.
		IN_RANGE,
		// Something could not be done (end of a list, no target...).
		BOUNDARY
	};

public:
	static void Init();
	static void Quit();
	static bool IsAvailable();

	// Play a cue. relativeDegrees is the bearing relative to the ship's
	// heading (0 = ahead, 90 = right); distanceFactor (0..1) lowers the volume
	// for far objects.
	static void Play(Cue cue, double relativeDegrees = 0., double distanceFactor = 1.);
	static void SetVolume(double volume);
	static double Volume();
};
