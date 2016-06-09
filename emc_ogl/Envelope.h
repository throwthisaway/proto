#pragma once
#include "Timer.h"
class Envelope {
public:
	virtual void Update(const Time&) = 0;
	virtual bool Finished() const = 0;
	~Envelope() {}
};

class Blink : public Envelope
{
	float& val;
	const double duration, start, rate;
	const float ratio;
	bool finished;
	double orig;
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