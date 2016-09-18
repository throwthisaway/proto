#include "Command.h"

void Execute(double total, double frame, std::queue<Command>& commands) {
	while (!commands.empty()) {
		const auto& cmd = commands.front();
		if (cmd.start <= total)
			cmd.fct(frame);
		if (cmd.end <= total)
			commands.pop();
		else
			break;
	}
}