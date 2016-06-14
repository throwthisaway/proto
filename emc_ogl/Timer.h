#pragma once
#include <chrono>

class Timer
{
	//std::chrono::duration<double> _elapsed;
	std::chrono::time_point<std::chrono::high_resolution_clock> _current, _prev, _start;
public:
	Timer();
	inline void Tick(void) {
		_prev = _current;
		_current = std::chrono::high_resolution_clock::now();
	}
	inline double ElapsedMs(void) { return std::chrono::duration_cast<std::chrono::nanoseconds>(_current - _prev).count() / 1000000.; }
	inline double TotalMs(void) { return std::chrono::duration_cast<std::chrono::nanoseconds>(_current - _start).count() / 1000000.; }
};

struct Time {
	double total, frame;
};