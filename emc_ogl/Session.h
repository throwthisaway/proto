#pragma once
#include <functional>
struct Session {
	std::function<void()> onOpen, onClose;
	std::function<void(bool)> onConnect;
	std::function<void(int, const char*)> onError;
	std::function<void(const char*, int)> onMessage, wsOnMessage;
};
