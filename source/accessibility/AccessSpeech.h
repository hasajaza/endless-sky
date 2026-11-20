/* AccessSpeech.h
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
#include <string>
#include <vector>



// Priority categories for spoken output. Lower values are more urgent.
enum class SpeechPriority : int {
	// Danger alerts: interrupt everything (hull critical, incoming missile...).
	CRITICAL = 0,
	// Important gameplay events: queued ahead of background chatter.
	IMPORTANT = 1,
	// Focus and navigation changes: interrupt any earlier focus speech.
	FOCUS = 2,
	// Information the player explicitly asked for: interrupt and speak now.
	REQUESTED = 3,
	// Informational background messages: queued, may be dropped when busy.
	INFO = 4
};



// Centralised speech output. All accessibility components send text here; this
// class decides what is interrupted, queued, merged or dropped, keeps a history,
// and hands the text to Universal Speech (screen readers or SAPI) on a worker
// thread so the game loop is never blocked by a speech engine.
class AccessSpeech {
public:
	// Start the speech worker. The library is looked for next to the executable.
	static void Init(const std::filesystem::path &executableDirectory, const std::filesystem::path &logFile);
	static void Quit();

	// Speak the given text with the given priority.
	static void Say(const std::string &text, SpeechPriority priority = SpeechPriority::INFO);
	// Speak several pieces as one utterance (joined with sentence breaks).
	static void SayParts(const std::vector<std::string> &parts, SpeechPriority priority);
	// Stop speaking and drop everything queued.
	static void Silence();

	// Repeat the last thing said (that was not itself a history review).
	static void RepeatLast();
	// Move through the speech history. Negative values go back in time.
	static void ReviewHistory(int direction);
	static std::vector<std::string> History();

	// Speech engine control.
	static std::string EngineName();
	static void CycleEngine();
	// The engine chosen with CycleEngine (-1 = automatic), for saving.
	static int ForcedEngine();
	static void SetForcedEngine(int index);
	static bool UseSapiFallback();
	static void SetSapiFallback(bool use);

	// Verbosity: 0 = minimal, 1 = normal, 2 = verbose.
	static int Verbosity();
	static void SetVerbosity(int level);

	// The last text spoken (for tests and diagnostics).
	static std::string LastSpoken();
	// Count of utterances accepted (for tests).
	static size_t SpokenCount();

	// Clean text for speech: drop control characters, collapse whitespace,
	// expand a few symbols that screen readers often skip.
	static std::string Clean(const std::string &text);
};
