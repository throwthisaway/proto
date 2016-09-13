#include "Command.h"

void Execute(double total, double frame, std::queue<Command> commands) {
	while (!commands.empty()) {
		const auto& cmd = commands.front();
		if (cmd.t <= total) {
			cmd.fct(frame);
			commands.pop();
		}
		else
			break;
	}
}