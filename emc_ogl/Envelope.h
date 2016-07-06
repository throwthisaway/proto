#pragma once
#include "Timer.h"
class Envelope {
public:
	virtual void Update(const Time&) = 0;
	virtual bool Finished() const = 0;
	~Envelope() {}
};

class Blink : public Envelope {
	float& val;
	const double duration, start, rate, orig;
	const float ratio;
	bool finished;
public:
	Blink(float& val, double duration, double current, double rate, float ratio/*ratio of the original value*/) : 
		val(val), duration(duration), start(current), rate(rate), ratio(ratio), finished(false), orig(val) {}
	void Update(const Time& t) override {
		if (finished) return;
		double d = t.total - start;
		if (duration > 0. && d > duration) {
			finished = true;
			val = orig;
			return;
		}
		auto mod = std::fmod(d, rate);
		val = mod < rate * .5f ? ratio * orig : orig;
	}
	bool Finished() const override { return finished; }
};
class SequenceAsc : public Envelope {
	size_t& val;
	const size_t begin, end, step, orig;
	const double duration, start, rate;
	const bool loop;
	bool finished = false;

public:
	SequenceAsc(size_t& val, double duration, double current, double rate, size_t begin, size_t end, bool loop, size_t step = 1) :
		val(val), duration(duration), start(current), rate(rate), orig(val),
		begin(begin), end(end), loop(loop), step(step) {
		this->val = begin;
	}
	void Update(const Time& t) override {
		if (finished) return;
		double d = t.total - start;
		if (duration > 0. && d > duration) {
			finished = true;
			val = orig;
			return;
		}
		auto temp = (size_t)(d / rate) * step;
		if (!loop && temp > end - begin) {
			finished = true;
			return;
		}
		val = begin + temp % (end - begin);
	}
	bool Finished() const override { return finished; }
};
