#include "audio.h"
#include "Logging.h"
#include <assert.h>
#include <vector>
#include <stdio.h>
namespace {
	// http://bleepsandpops.com/post/37792760450/adding-cue-points-to-wav-files-in-c
	struct Chunks {
		char id[4];  //  "data"
		unsigned int size;
		union {
			struct { // "RIFF"
				char type[4]; // "WAVE"
			}header;
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
}

void Play(ALuint id, float pan, float gain, bool loop) {
	const float max_gain = .5f;
	ALint state;
	::alGetSourcei(id, AL_SOURCE_STATE, &state);
	if (state != AL_PLAYING) {
		::alSourcei(id, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
		::alSourcePlay(id);
	}
	::alSource3f(id, AL_POSITION, pan, 0.f, 0.f);
	::alSourcef(id, AL_GAIN, gain * max_gain);
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
#define PATH_PREFIX "..//..//emc_openal//"
#endif
	const char* fname = PATH_PREFIX"audio.wav";
	const auto res = LoadWAV(fname);
	if (!res.data.empty()) {
		alBufferData(pew = die = jet = buffers[0], res.format, &res.data.front(), res.data.size(), res.freq);
	}
}
Audio::~Audio() {
	for (const auto source : sources)
		::alSourcei(source, AL_BUFFER, 0);
	::alDeleteSources(sources.size(), &sources.front());
	::alDeleteBuffers(buffers.size(), &buffers.front());
	::alcDestroyContext(context);
	::alcCloseDevice(device);
}
ALuint Audio::GenSource(ALuint buffer) {
	ALuint id = sources[index++];
	if (index >= sources.size()) index = 0;
	ALint state;
	::alGetSourcei(id, AL_SOURCE_STATE, &state);
	if (state == AL_PLAYING) ::alSourceStop(id);
	::alSourcei(id, AL_BUFFER, buffer);
	return id;
}
