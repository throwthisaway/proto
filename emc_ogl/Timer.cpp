#include "Timer.h"

Timer::Timer() : _current(std::chrono::high_resolution_clock::now()),
	_prev(_current),
	_start(_current) {};
