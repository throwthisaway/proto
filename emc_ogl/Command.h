#pragma once
#include <functional>
#include <queue>
struct Command {
	double t;
	std::function<void(double frame)> fct;
};

void Execute(double total, double frame, std::queue<Command> commands);
