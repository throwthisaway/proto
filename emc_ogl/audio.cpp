#include "audio.h"
#include "Logging.h"
#include <assert.h>
#include <vector>
#include <stdio.h>
namespace {
	// http://bleepsandpops.com/post/37792760450/adding-cue-points-to-wav-files-in-c
	struct Format { // "fmt "
		unsigned short compression;
		unsigned short channels;
		unsigned int freq;
		unsigned int avgBps;
		unsigned short blockAlign;
		unsigned short bits;
	}format;
	struct Cue {//  "cue "
		char cuePointsCount[4];
		struct CuePoint {
			char id[4];
			char playOrderPosition[4];
			char dataChunkID[4];
			char chunkStart[4];
			char blockStart[4];
			char frameOffset[4];
		};
		CuePoint* cuePoints;
	}cue;
	struct Wave { //  "data"
		char* data;
	}wave;
	struct Chunks {
		char id[4];  //  "data"
		unsigned int size;
		union {
			struct { // "RIFF"
				char type[4]; // "WAVE"
			}header;
			Format format;
			Cue cue;
			Wave wave;
		};
	};

	struct ChunkLocation {
		size_t startOffset; 
		size_t size;
	};

	struct Buffer {
		std::vector<unsigned char> data;
		ALenum format;
		unsigned int freq;
		// TODO:: cue...
	};

	auto LoadWAV(const char * fname) {
		LOG_INFO("Loading %s...\n", fname);
		Buffer res;
#ifdef __EMSCRIPTEN__
		FILE* f = fopen(fname, "rb");
#else
		FILE* f;
		fopen_s(&f, fname, "rb");
#endif
		if (!f) {
			LOG_ERR(-1, "File not found");
			return res;
		}
		Chunks chunks;
		fread(&chunks, 8 + sizeof(chunks.header), 1, f);
		if (strncmp(chunks.id, "RIFF", 4)) {
			LOG_ERR(-1, "Not a RIFF file");
			fclose(f);
			return res;
		}
		// chunk.header.size?
		if (strncmp(chunks.header.type, "WAVE", 4)) {
			LOG_ERR(-1, "Not a WAVE type");
			fclose(f);
			return res;
		}
		
		while (!feof(f)) {
			auto read = fread(&chunks, 8, 1, f);
			if (!read) break; // TODO:: why
			size_t skip = chunks.size;
			if (!memcmp(chunks.id, "fmt ", 4)) {
				auto read = fread(&chunks.format, sizeof(chunks.format), 1, f);
				if (read < 1 ) {
					LOG_ERR(-1, "Format chunk read error");
					fclose(f);
					return res;
				}
				if (chunks.format.compression != 1) {
					LOG_ERR(-1, "Compressed wav not supported");
					fclose(f);
					return res;
				}
				if (chunks.format.bits == 8)
				{
					if (chunks.format.channels == 1)
						res.format = AL_FORMAT_MONO8;
					else if (chunks.format.channels == 2)
						res.format = AL_FORMAT_STEREO8;
					// TODO:: else...
				}
				else if (chunks.format.bits == 16)
				{
					if (chunks.format.channels == 1)
						res.format = AL_FORMAT_MONO16;
					else if (chunks.format.channels == 2)
						res.format = AL_FORMAT_STEREO16;
					// TODO:: else...
				}
				// TODO:: else...
				res.freq = chunks.format.freq;
				skip -= sizeof(chunks.format);
				// skip extra format bytes
			} else if (!memcmp(chunks.id, "data", 4)) {
				res.data.resize(chunks.size);
				auto read = fread(&res.data.front(), chunks.size, 1, f);
				if (read < 1) {
					LOG_ERR(-1, "Data read error");
					fclose(f);
					return res;
				}
				skip -= chunks.size;
			}//else if (!memcmp(chunks.id, "cue ", 4)) {
			//	// TODO::
			//	skip -= sizeof(chunks.cue_;
			//}
			auto rr = ftell(f);
			if (skip > 0)
				fseek(f, skip + skip % 2, SEEK_CUR);
		}
		fclose(f);
		return res;
	}

	inline void InternalControl(ALuint id, float pan, float gain) {
		const float max_gain = .3f;
		::alSource3f(id, AL_POSITION, pan, 0.f, 0.f);
		::alSourcef(id, AL_GAIN, gain * max_gain);
	}
}

Audio::Audio() : device(::alcOpenDevice(NULL)),
	context(::alcCreateContext(device, NULL)) {
	assert(device);
	assert(context);
	::alcMakeContextCurrent(context);
	int major, minor;
	alcGetIntegerv(device, ALC_MAJOR_VERSION, 1, &major);
	alcGetIntegerv(device, ALC_MAJOR_VERSION, 1, &minor);
	assert(major == 1);
	const ALfloat pos[] = { 0.f, 0.f, 0.f },
		vel[] = { 0.f, 0.f, 0.f },
		orientation[] = { 0.f, 0.f, -1.f, 0.f, 1.f, 0.f };

	::alListenerfv(AL_POSITION, pos);
	::alListenerfv(AL_VELOCITY, vel);
	::alListenerfv(AL_ORIENTATION, orientation);

	::alGenBuffers(buffers.size(), &buffers.front());
	::alGenSources(sources.size(), &sources.front());

#ifdef __EMSCRIPTEN__
#define PATH_PREFIX ""
#else
#define PATH_PREFIX "..//..//emc_ogl//"
#endif

	pew[0] = LoadToBuffer(PATH_PREFIX"asset//sound//pew1.wav");
	pew[1] = LoadToBuffer(PATH_PREFIX"asset//sound//pew2.wav");
	pew[2] = LoadToBuffer(PATH_PREFIX"asset//sound//pew3.wav");
	pew[3] = LoadToBuffer(PATH_PREFIX"asset//sound//pew4.wav");
	die = LoadToBuffer(PATH_PREFIX"asset//sound//die.wav");
	engine = LoadToBuffer(PATH_PREFIX"asset//sound//engine.wav");
}
Audio::~Audio() {
	for (const auto source : sources)
		::alSourcei(source, AL_BUFFER, 0);
	::alDeleteSources(sources.size(), &sources.front());
	::alDeleteBuffers(buffers.size(), &buffers.front());
	::alcDestroyContext(context);
	::alcCloseDevice(device);
}
ALuint Audio::LoadToBuffer(const char* fname) {
	static size_t index = 0;
	const auto res = LoadWAV(fname);
	if (!res.data.empty()) {
		alBufferData(buffers[index], res.format, &res.data.front(), res.data.size(), res.freq);
		return buffers[index++];
	}
	return -1;
}
Audio::Source Audio::GenSource(ALuint buffer) {
	Source res{ index, ++counters[index], buffer };
	ALuint id = sources[index++];
	if (index >= sources.size()) index = 0;
	ALint state;
	::alGetSourcei(id, AL_SOURCE_STATE, &state);
	if (state == AL_PLAYING) ::alSourceStop(id);
	::alSourcei(id, AL_BUFFER, buffer);
	return res;
}
void Audio::Enqueue(Command::ID id, Source& source, float pan, float gain, bool loop) {
	if (counters[source.index] > source.counter) {
		// audio source reused, 
		if (id != Command::ID::Start) return;	// already stopped, nothing to do
		// get a new one
		if (gain>0.f)
			source = GenSource(source.buffer);
	}
	if (gain>0.f)
		cmd_queue.push({ id, source.index, pan, gain, loop });
}
void Audio::Play(size_t index, float pan, float gain, bool loop) const {
	ALint state, id = sources[index];
	::alGetSourcei(id, AL_SOURCE_STATE, &state);
	if (state != AL_PLAYING) {
		::alSourcei(id, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
		::alSourcePlay(id);
	}
	InternalControl(id, pan, gain);
}

void Audio::Ctrl(size_t index, float pan, float gain) const {
	ALint state, id = sources[index];
	::alGetSourcei(id, AL_SOURCE_STATE, &state);
	if (state != AL_PLAYING) return;
	InternalControl(id, pan, gain);
}

void Audio::Execute() {
	while (!cmd_queue.empty()) {
		const auto& cmd = cmd_queue.front();
		switch (cmd.id) {
		case Command::ID::Start:
			Play(cmd.index, cmd.pan, cmd.gain, cmd.loop);
			break;
		case Command::ID::Stop:
			::alSourceStop(sources[cmd.index]);
			break;
		case Command::ID::Ctrl:
			Ctrl(cmd.index, cmd.pan, cmd.gain);
			break;
		}
		cmd_queue.pop();
	}
}
