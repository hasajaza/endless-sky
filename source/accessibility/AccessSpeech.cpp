/* AccessSpeech.cpp
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

#include "AccessSpeech.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#endif

using namespace std;

namespace {
	using Clock = chrono::steady_clock;

	struct Utterance {
		string text;
		SpeechPriority priority;
		bool interrupt;
		Clock::time_point time;
	};

	// Commands executed on the worker thread (all speech library calls must
	// happen on one thread because Universal Speech is not thread safe).
	enum class Control { NONE, STOP, CYCLE_ENGINE, APPLY_FALLBACK };

	mutex queueMutex;
	condition_variable queueCondition;
	deque<Utterance> pending;
	deque<Control> controls;
	bool quitting = false;
	thread worker;
	bool started = false;

	// State only touched under queueMutex.
	deque<string> history;
	const size_t MAX_HISTORY = 300;
	int historyIndex = -1;
	string lastSpoken;
	string lastRepeatable;
	size_t spokenCount = 0;
	Clock::time_point lastCriticalTime;
	// Recently spoken texts, for duplicate suppression.
	deque<pair<string, Clock::time_point>> recent;

	atomic<int> verbosity = 1;
	// Engine requested in the settings (-1 = automatic); applied by the worker.
	atomic<int> requestedEngine = -1;
	atomic<bool> sapiFallback = true;
	string engineName = "not started";

	ofstream logStream;
	Clock::time_point startTime;

#ifdef _WIN32
	using SayFn = int (__cdecl *)(const wchar_t *, int);
	using StopFn = int (__cdecl *)();
	using GetValueFn = int (__cdecl *)(int);
	using SetValueFn = int (__cdecl *)(int, int);
	using GetStringFn = const wchar_t *(__cdecl *)(int);
	using BrailleFn = int (__cdecl *)(const wchar_t *);

	HMODULE library = nullptr;
	SayFn speechSay = nullptr;
	StopFn speechStop = nullptr;
	GetValueFn speechGetValue = nullptr;
	SetValueFn speechSetValue = nullptr;
	GetStringFn speechGetString = nullptr;
	BrailleFn brailleDisplay = nullptr;
	// Values from UniversalSpeech.h.
	const int SP_ENABLE_NATIVE_SPEECH = 0xFFFF;
	const int SP_ENGINE = 0x40000;
	const int SP_ENGINE_AVAILABLE = 0x50000;
	// -1 means automatic selection.
	int forcedEngine = -1;

	wstring Widen(const string &text)
	{
		if(text.empty())
			return wstring();
		int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
		wstring result(static_cast<size_t>(max(size, 0)), L'\0');
		if(size > 0)
			MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
		return result;
	}

	string Narrow(const wchar_t *text)
	{
		if(!text)
			return string();
		int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
		if(size <= 1)
			return string();
		string result(static_cast<size_t>(size - 1), '\0');
		WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), size, nullptr, nullptr);
		return result;
	}

	wstring libraryDirectory;

	void LoadLibraryOnWorker()
	{
		wstring path = libraryDirectory + L"UniversalSpeech.dll";
		library = LoadLibraryW(path.c_str());
		if(!library)
			library = LoadLibraryW(L"UniversalSpeech.dll");
		if(!library)
			return;
		auto get = [](const char *name) { return reinterpret_cast<void *>(GetProcAddress(library, name)); };
		speechSay = reinterpret_cast<SayFn>(get("speechSay"));
		speechStop = reinterpret_cast<StopFn>(get("speechStop"));
		speechGetValue = reinterpret_cast<GetValueFn>(get("speechGetValue"));
		speechSetValue = reinterpret_cast<SetValueFn>(get("speechSetValue"));
		speechGetString = reinterpret_cast<GetStringFn>(get("speechGetString"));
		brailleDisplay = reinterpret_cast<BrailleFn>(get("brailleDisplay"));
		if(!speechSay)
		{
			FreeLibrary(library);
			library = nullptr;
		}
	}

	string CurrentEngineName()
	{
		if(!library || !speechGetValue || !speechGetString)
			return "no speech library";
		int current = speechGetValue(SP_ENGINE);
		string name = current >= 0 ? Narrow(speechGetString(SP_ENGINE + current)) : "none";
		if(forcedEngine < 0)
			name = "automatic, using " + name;
		return name;
	}

	void ApplyFallback()
	{
		if(library && speechSetValue)
			speechSetValue(SP_ENABLE_NATIVE_SPEECH, sapiFallback ? 1 : 0);
	}

	void CycleEngineOnWorker()
	{
		if(!library || !speechGetValue || !speechSetValue || !speechGetString)
			return;
		// Collect the engines that report being available.
		vector<int> available;
		for(int i = 0; i < 32; ++i)
		{
			const wchar_t *name = speechGetString(SP_ENGINE + i);
			if(!name)
				break;
			if(speechGetValue(SP_ENGINE_AVAILABLE + i))
				available.push_back(i);
		}
		// Order: automatic, then each available engine.
		auto it = find(available.begin(), available.end(), forcedEngine);
		if(forcedEngine < 0)
			forcedEngine = available.empty() ? -1 : available.front();
		else if(it == available.end() || next(it) == available.end())
			forcedEngine = -1;
		else
			forcedEngine = *next(it);
		speechSetValue(SP_ENGINE, forcedEngine);
	}
#endif

	void WriteLog(const string &tag, const string &text)
	{
		if(!logStream.is_open())
			return;
		double seconds = chrono::duration<double>(Clock::now() - startTime).count();
		char stamp[32];
		snprintf(stamp, sizeof(stamp), "%9.2f", seconds);
		logStream << stamp << ' ' << tag << ' ' << text << '\n';
		logStream.flush();
	}

	const char *PriorityTag(SpeechPriority priority)
	{
		switch(priority)
		{
			case SpeechPriority::CRITICAL: return "[critical]";
			case SpeechPriority::IMPORTANT: return "[important]";
			case SpeechPriority::FOCUS: return "[focus]";
			case SpeechPriority::REQUESTED: return "[requested]";
			default: return "[info]";
		}
	}

	// Output one utterance through the active backend. Runs on the worker.
	void Output(const Utterance &u)
	{
#ifdef _WIN32
		if(library && speechSay)
		{
			wstring wide = Widen(u.text);
			speechSay(wide.c_str(), u.interrupt ? 1 : 0);
			if(brailleDisplay && (u.priority == SpeechPriority::FOCUS || u.priority == SpeechPriority::REQUESTED))
				brailleDisplay(wide.c_str());
		}
#endif
		static const bool toStdout = getenv("ES_ACCESS_STDOUT") != nullptr;
		if(toStdout)
			cout << "SPEECH " << PriorityTag(u.priority) << (u.interrupt ? " (interrupt) " : " ") << u.text << endl;
	}

	void WorkerLoop()
	{
#ifdef _WIN32
		HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
		LoadLibraryOnWorker();
		ApplyFallback();
		if(requestedEngine >= 0 && library && speechGetValue && speechSetValue
				&& speechGetValue(SP_ENGINE_AVAILABLE + requestedEngine))
		{
			forcedEngine = requestedEngine;
			speechSetValue(SP_ENGINE, forcedEngine);
		}
		{
			lock_guard<mutex> lock(queueMutex);
			engineName = CurrentEngineName();
		}
#else
		{
			lock_guard<mutex> lock(queueMutex);
			engineName = getenv("ES_ACCESS_STDOUT") ? "standard output" : "log file only";
		}
#endif
		while(true)
		{
			Utterance next;
			Control control = Control::NONE;
			{
				unique_lock<mutex> lock(queueMutex);
				queueCondition.wait(lock, [] { return quitting || !pending.empty() || !controls.empty(); });
				if(quitting)
					break;
				if(!controls.empty())
				{
					control = controls.front();
					controls.pop_front();
				}
				else
				{
					// Most urgent first; FIFO within the same priority.
					auto best = pending.begin();
					for(auto it = pending.begin(); it != pending.end(); ++it)
						if(static_cast<int>(it->priority) < static_cast<int>(best->priority))
							best = it;
					next = *best;
					pending.erase(best);
				}
			}

			if(control != Control::NONE)
			{
#ifdef _WIN32
				if(control == Control::STOP && library && speechStop)
					speechStop();
				else if(control == Control::CYCLE_ENGINE)
				{
					CycleEngineOnWorker();
					requestedEngine = forcedEngine;
					string name = CurrentEngineName();
					{
						lock_guard<mutex> lock(queueMutex);
						engineName = name;
					}
					Output({"Speech: " + name, SpeechPriority::REQUESTED, true, Clock::now()});
				}
				else if(control == Control::APPLY_FALLBACK)
					ApplyFallback();
#else
				if(control == Control::CYCLE_ENGINE)
					Output({"Speech: " + engineName, SpeechPriority::REQUESTED, true, Clock::now()});
#endif
				continue;
			}

			Output(next);
		}
#ifdef _WIN32
		if(library)
		{
			if(speechStop)
				speechStop();
			FreeLibrary(library);
			library = nullptr;
		}
		if(SUCCEEDED(comResult))
			CoUninitialize();
#endif
	}

	void Push(Utterance u)
	{
		{
			lock_guard<mutex> lock(queueMutex);
			if(u.interrupt)
			{
				// Drop everything that is less urgent than a critical alert.
				pending.erase(remove_if(pending.begin(), pending.end(),
					[](const Utterance &p) { return p.priority != SpeechPriority::CRITICAL; }), pending.end());
				controls.push_back(Control::STOP);
			}
			else if(u.priority == SpeechPriority::INFO)
			{
				// Background chatter must never pile up: keep at most two.
				size_t infoCount = count_if(pending.begin(), pending.end(),
					[](const Utterance &p) { return p.priority == SpeechPriority::INFO; });
				if(infoCount >= 2)
				{
					auto oldest = find_if(pending.begin(), pending.end(),
						[](const Utterance &p) { return p.priority == SpeechPriority::INFO; });
					if(oldest != pending.end())
						pending.erase(oldest);
				}
			}
			pending.push_back(std::move(u));
		}
		queueCondition.notify_one();
	}

	string Join(const vector<string> &parts)
	{
		string result;
		for(const string &part : parts)
		{
			if(part.empty())
				continue;
			if(!result.empty())
			{
				char last = result.back();
				if(last != '.' && last != '!' && last != '?' && last != ':' && last != ',')
					result += '.';
				result += ' ';
			}
			result += part;
		}
		return result;
	}
}



void AccessSpeech::Init(const filesystem::path &executableDirectory, const filesystem::path &logFile)
{
	if(started)
		return;
	startTime = Clock::now();
	if(!logFile.empty())
	{
		logStream.open(logFile, ios::out | ios::trunc);
		WriteLog("[start]", "Endless Sky accessibility speech log");
	}
#ifdef _WIN32
	libraryDirectory = executableDirectory.wstring();
	if(!libraryDirectory.empty() && libraryDirectory.back() != L'\\' && libraryDirectory.back() != L'/')
		libraryDirectory += L'\\';
#else
	(void)executableDirectory;
#endif
	quitting = false;
	worker = thread(WorkerLoop);
	started = true;
}



void AccessSpeech::Quit()
{
	if(!started)
		return;
	{
		lock_guard<mutex> lock(queueMutex);
		quitting = true;
	}
	queueCondition.notify_one();
	if(worker.joinable())
		worker.join();
	started = false;
	if(logStream.is_open())
		logStream.close();
}



void AccessSpeech::Say(const string &rawText, SpeechPriority priority)
{
	string text = Clean(rawText);
	if(text.empty())
		return;

	Clock::time_point now = Clock::now();
	bool interrupt = false;
	{
		lock_guard<mutex> lock(queueMutex);
		// Forget old entries in the duplicate filter.
		while(!recent.empty() && now - recent.front().second > chrono::milliseconds(2500))
			recent.pop_front();
		// Requested and focus speech is always spoken; automatic speech is
		// suppressed if the very same text was spoken moments ago.
		if(priority != SpeechPriority::REQUESTED && priority != SpeechPriority::FOCUS)
			for(const auto &it : recent)
				if(it.first == text)
					return;
		recent.emplace_back(text, now);

		if(priority == SpeechPriority::CRITICAL)
		{
			interrupt = true;
			lastCriticalTime = now;
		}
		else if(priority == SpeechPriority::FOCUS || priority == SpeechPriority::REQUESTED)
			// Do not cut off a danger alert that has only just started.
			interrupt = now - lastCriticalTime > chrono::milliseconds(1200);

		history.push_back(text);
		if(history.size() > MAX_HISTORY)
			history.pop_front();
		historyIndex = -1;
		lastSpoken = text;
		lastRepeatable = text;
		++spokenCount;
	}
	WriteLog(PriorityTag(priority), text);
	Push({text, priority, interrupt, now});
}



void AccessSpeech::SayParts(const vector<string> &parts, SpeechPriority priority)
{
	Say(Join(parts), priority);
}



void AccessSpeech::Silence()
{
	{
		lock_guard<mutex> lock(queueMutex);
		pending.clear();
		controls.push_back(Control::STOP);
	}
	queueCondition.notify_one();
}



void AccessSpeech::RepeatLast()
{
	string text;
	{
		lock_guard<mutex> lock(queueMutex);
		text = lastRepeatable;
	}
	if(text.empty())
		text = "Nothing has been spoken yet.";
	WriteLog("[repeat]", text);
	Push({text, SpeechPriority::REQUESTED, true, Clock::now()});
}



void AccessSpeech::ReviewHistory(int direction)
{
	string text;
	{
		lock_guard<mutex> lock(queueMutex);
		if(history.empty())
			text = "Speech history is empty.";
		else
		{
			int size = static_cast<int>(history.size());
			// -1 means "not reviewing"; start from the newest entry.
			if(historyIndex < 0)
				historyIndex = size;
			historyIndex = max(0, min(size - 1, historyIndex + direction));
			text = history[static_cast<size_t>(historyIndex)];
			if(historyIndex == 0 && direction < 0)
				text = "Oldest. " + text;
			else if(historyIndex == size - 1 && direction > 0)
				text = "Newest. " + text;
		}
		lastSpoken = text;
	}
	WriteLog("[history]", text);
	Push({text, SpeechPriority::REQUESTED, true, Clock::now()});
}



vector<string> AccessSpeech::History()
{
	lock_guard<mutex> lock(queueMutex);
	return vector<string>(history.begin(), history.end());
}



string AccessSpeech::EngineName()
{
	lock_guard<mutex> lock(queueMutex);
	return engineName;
}



void AccessSpeech::CycleEngine()
{
	{
		lock_guard<mutex> lock(queueMutex);
		pending.clear();
		controls.push_back(Control::STOP);
		controls.push_back(Control::CYCLE_ENGINE);
	}
	queueCondition.notify_one();
}



int AccessSpeech::ForcedEngine()
{
	return requestedEngine;
}



void AccessSpeech::SetForcedEngine(int index)
{
	requestedEngine = index;
}



bool AccessSpeech::UseSapiFallback()
{
	return sapiFallback;
}



void AccessSpeech::SetSapiFallback(bool use)
{
	sapiFallback = use;
	{
		lock_guard<mutex> lock(queueMutex);
		controls.push_back(Control::APPLY_FALLBACK);
	}
	queueCondition.notify_one();
}



int AccessSpeech::Verbosity()
{
	return verbosity;
}



void AccessSpeech::SetVerbosity(int level)
{
	verbosity = max(0, min(2, level));
}



string AccessSpeech::LastSpoken()
{
	lock_guard<mutex> lock(queueMutex);
	return lastSpoken;
}



size_t AccessSpeech::SpokenCount()
{
	lock_guard<mutex> lock(queueMutex);
	return spokenCount;
}



string AccessSpeech::Clean(const string &text)
{
	string result;
	result.reserve(text.size());
	bool space = false;
	for(size_t i = 0; i < text.size(); ++i)
	{
		unsigned char c = static_cast<unsigned char>(text[i]);
		// Treat tabs, newlines and other control characters as spaces.
		if(c < 32 || c == 127)
		{
			// A paragraph break becomes a sentence break so that screen
			// readers pause between paragraphs.
			if(c == '\n' && !result.empty() && result.back() != '.' && result.back() != ':'
					&& result.back() != '?' && result.back() != '!' && result.back() != ',')
			{
				if(i + 1 < text.size() && text[i + 1] != '\n')
				{
					// Single line breaks inside a sentence are just spaces.
				}
				else
					result += '.';
			}
			space = true;
			continue;
		}
		if(c == ' ')
		{
			space = true;
			continue;
		}
		if(space && !result.empty())
			result += ' ';
		space = false;
		// Underscores and backticks are used as formatting markers.
		if(c == '_' || c == '`')
			continue;
		result += static_cast<char>(c);
	}
	return result;
}
