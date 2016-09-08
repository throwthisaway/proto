#pragma once
#include <array>
#include <queue>
#include <random>
#ifdef __EMSCRIPTEN__
#include <AL/al.h>
#include <AL/alc.h>
#else
#include <al.h>
#include <alc.h>
#endif
template<typename ArrayT> 
class Randomizer {
	const ArrayT& array;
	std::mt19937& mt;
	std::uniform_int_distribution<> dist;
public:
	Randomizer(const ArrayT& array, std::mt19937& mt) : array(array), mt(mt), dist(0, array.size() - 1) {}
	typename ArrayT::value_type Gen() { return array[dist(mt)]; }
};

class Audio {
	static constexpr size_t NUM = 6;
	static constexpr size_t MAX_SOURCES = 32;
	ALCdevice* device;
	ALCcontext* context;
	std::array<ALuint, NUM> buffers;
	ALuint LoadToBuffer(const char* fname);
public:
	std::array<ALuint, MAX_SOURCES> sources;
	std::array<size_t, MAX_SOURCES> counters;
	size_t index = 0;
	struct Source {
		size_t index, counter;
		ALuint buffer;
	};
	struct Command {
		enum class ID { Start, Stop, Ctrl };
		ID id;
		size_t index;
		float pan, gain;
		bool loop;
	};
	std::queue<Command> cmd_queue;
	ALuint die, engine;
	static constexpr size_t PEW_COUNT = 4;
	std::array<ALuint, PEW_COUNT> pew;
	Audio();
	~Audio();
	Source GenSource(ALuint buffer);
	void Enqueue(Command::ID id, Source& source, float pan = 0.f, float gain = 1.f, bool loop = false);
	void Play(size_t index, float pan, float gain, bool loop = false) const;
	void Ctrl(size_t index, float pan, float gain) const;
	void Execute();
};
