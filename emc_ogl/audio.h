#pragma once
#include <array>
#include <vector>
#ifdef __EMSCRIPTEN__
#include <AL/al.h>
#include <AL/alc.h>
#else
#include <al.h>
#include <alc.h>
#endif
class Audio {
	static constexpr int PEW = 0, CRASH = 1, PROPULSION = 2, NUM = 3;
	static constexpr int MAX_SOURCES = 32;
	ALCdevice* device;
	ALCcontext* context;
	std::array<ALuint, NUM> buffers;
	std::array<ALuint, MAX_SOURCES> sources;
	size_t index = 0;
public:
	ALuint pew, crash, propulsion;
	Audio();
	~Audio();
	ALuint GenSource(ALuint buffer);
};
