#pragma once
#include <array>
#include <queue>
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
public:
	std::array<ALuint, MAX_SOURCES> sources;
	std::array<size_t, MAX_SOURCES> counters;
	size_t index = 0;
	struct Source {
		size_t index, counter;
		ALuint buffer;
	};
	struct Command {
		enum class ID { Start, Stop };
		ID id;
		Source source;
		float pan, gain;
		bool loop;
	};
	std::queue<Command> cmd_queue;
	ALuint pew, die, jet;
	Audio();
	~Audio();
	Source GenSource(ALuint buffer);
	void Enqueue(Command::ID id, Source& source, float pan = 0.f, float gain = 1.f, bool loop = false);
	void Play(const Source& source, float pan, float gain, bool loop = false) const;
	void Execute();
};
