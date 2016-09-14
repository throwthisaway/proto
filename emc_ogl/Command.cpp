#include "Command.h"

void Execute(double total, double frame, std::queue<Command>& commands) {
	while (!commands.empty()) {
		const auto& cmd = commands.front();
		if (cmd.end <= total) {
			commands.pop();
			continue;
		}
		if (cmd.start <= total && cmd.end >= total)
			cmd.fct(frame);
		break;
	}
}