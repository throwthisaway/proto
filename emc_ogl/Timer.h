#pragma once
#include <chrono>

class Timer
{
	std::chrono::duration<double> _elapsed;
	std::chrono::time_point<std::chrono::high_resolution_clock> _current, _prev, _start;
public:
	Timer();
	inline void Tick(void) {
		_prev = _current;
		_current = std::chrono::high_resolution_clock::now();
	}
	inline int64_t Elapsed(void) { return std::chrono::duration_cast< std::chrono::milliseconds>(_current - _prev).count(); }
	inline int64_t Total(void) { return std::chrono::duration_cast< std::chrono::milliseconds>(_current - _start).count(); }
};

struct Time {
	double total, frame;
};