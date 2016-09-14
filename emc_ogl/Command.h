#pragma once
#include <functional>
#include <queue>
struct Command {
	double start, end;
	std::function<void(double frame)> fct;
};

void Execute(double total, double frame, std::queue<Command>& commands);
