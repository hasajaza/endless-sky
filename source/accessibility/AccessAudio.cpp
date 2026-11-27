/* AccessAudio.cpp
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

#include "AccessAudio.h"

#include "../audio/Audio.h"
#include "../audio/SoundCategory.h"

#include <AL/al.h>
#include <AL/alc.h>

#include <cmath>
#include <cstdint>
#include <map>
#include <numbers>
#include <vector>

using namespace std;

namespace {
	const int RATE = 44100;
	const int SOURCE_COUNT = 6;

	bool initialized = false;
	double volume = .6;
	map<AccessAudio::Cue, ALuint> buffers;
	vector<ALuint> sources;
	size_t nextSource = 0;

	// Generate a tone with a short attack and release so it does not click.
	vector<int16_t> Tone(const vector<pair<double, double>> &segments, double amplitude, bool square = false)
	{
		vector<int16_t> samples;
		for(const auto &[frequency, seconds] : segments)
		{
			int count = static_cast<int>(seconds * RATE);
			int fade = min(count / 4, RATE / 200);
			for(int i = 0; i < count; ++i)
			{
				double t = static_cast<double>(i) / RATE;
				double wave = sin(2. * numbers::pi * frequency * t);
				if(square)
					wave = wave >= 0. ? .6 : -.6;
				double envelope = 1.;
				if(i < fade)
					envelope = static_cast<double>(i) / fade;
				else if(i > count - fade)
					envelope = static_cast<double>(count - i) / fade;
				samples.push_back(static_cast<int16_t>(wave * envelope * amplitude * 32767.));
			}
		}
		return samples;
	}

	ALuint MakeBuffer(const vector<int16_t> &samples)
	{
		ALuint buffer = 0;
		alGenBuffers(1, &buffer);
		alBufferData(buffer, AL_FORMAT_MONO16, samples.data(),
			static_cast<ALsizei>(samples.size() * sizeof(int16_t)), RATE);
		return buffer;
	}
}



void AccessAudio::Init()
{
	if(initialized)
		return;
	// The game's audio must already have created a context.
	if(!alcGetCurrentContext())
		return;
	alGetError();

	buffers[Cue::BEACON] = MakeBuffer(Tone({{880., .07}}, .5));
	buffers[Cue::SHIELD_HIT] = MakeBuffer(Tone({{330., .05}}, .35, true));
	buffers[Cue::HULL_HIT] = MakeBuffer(Tone({{150., .09}}, .45, true));
	buffers[Cue::IN_RANGE] = MakeBuffer(Tone({{660., .05}, {990., .07}}, .4));
	buffers[Cue::BOUNDARY] = MakeBuffer(Tone({{220., .06}}, .35));
	buffers[Cue::AIM] = MakeBuffer(Tone({{1200., .025}}, .3));
	buffers[Cue::ON_TARGET] = MakeBuffer(Tone({{1600., .03}, {2100., .04}}, .45));

	sources.resize(SOURCE_COUNT);
	alGenSources(SOURCE_COUNT, sources.data());
	for(ALuint source : sources)
	{
		alSourcei(source, AL_SOURCE_RELATIVE, AL_TRUE);
		alSourcef(source, AL_REFERENCE_DISTANCE, 1.f);
		alSourcef(source, AL_ROLLOFF_FACTOR, 0.f);
	}
	initialized = (alGetError() == AL_NO_ERROR);
}



void AccessAudio::Quit()
{
	if(!initialized)
		return;
	for(ALuint source : sources)
		alSourceStop(source);
	alDeleteSources(static_cast<ALsizei>(sources.size()), sources.data());
	for(auto &it : buffers)
		alDeleteBuffers(1, &it.second);
	sources.clear();
	buffers.clear();
	initialized = false;
}



bool AccessAudio::IsAvailable()
{
	return initialized;
}



void AccessAudio::Play(Cue cue, double relativeDegrees, double distanceFactor)
{
	if(!initialized)
		return;
	auto it = buffers.find(cue);
	if(it == buffers.end())
		return;

	ALuint source = sources[nextSource];
	nextSource = (nextSource + 1) % sources.size();
	alSourceStop(source);
	alSourcei(source, AL_BUFFER, static_cast<ALint>(it->second));

	double radians = relativeDegrees * numbers::pi / 180.;
	// The listener looks along -z with +x to the right.
	alSource3f(source, AL_POSITION, static_cast<ALfloat>(sin(radians)), 0.f, static_cast<ALfloat>(-cos(radians)));
	double pitch = 1.;
	if(cue == Cue::BEACON)
	{
		// Front/back cannot be heard from stereo panning alone, so the pitch
		// falls smoothly from ahead (high) to behind (low).
		pitch = 1.25 - .5 * fabs(relativeDegrees) / 180.;
	}
	alSourcef(source, AL_PITCH, static_cast<ALfloat>(pitch));
	double gain = volume * Audio::Volume(SoundCategory::UI) * (.35 + .65 * max(0., min(1., distanceFactor)));
	alSourcef(source, AL_GAIN, static_cast<ALfloat>(gain));
	alSourcePlay(source);
}



void AccessAudio::SetVolume(double newVolume)
{
	volume = max(0., min(1., newVolume));
}



double AccessAudio::Volume()
{
	return volume;
}
