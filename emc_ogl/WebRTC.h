#pragma once
#include "Session.h"
#include <string>
class WebRTC {
	const Session session;
public:
	size_t sent = 0, received = 0;
	WebRTC(const char * hostname, unsigned short port, const char *id, const Session&);
	void Send(const char* msg, size_t len);
	void OnMessage(const std::string& str);
	void OnClose();
	void OnOpen();
	void OnError();
	void OnConnect(bool initial);
	void RTCConnect();
	void WSSend(const char* msg, size_t len);
	void WSOnMessage(const std::string& str);
	void WSOnClose();
};
