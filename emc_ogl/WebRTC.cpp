#include "WebRTC.h"
#include "Logging.h"
#include <vector>
#ifdef __EMSCRIPTEN__
#include <emscripten/bind.h>

using namespace emscripten;
#endif
//extern "C" {
//	void OnMessage(const char* msg, size_t len) {
//		WebRTC::OnMessage(msg, len);
//	}
//	void OnClose() {
//		WebRTC::OnClose();
//	}
//}
//
//class Client {
//	const std::string id;
//	void* conn;
//public:
//	Client(const std::string& id, void* conn) :id(id), conn(conn) {}
//	void OnMessage(const std::vector<char>& data) {}
//	void OnClose() {}
//	void OnError() {}
//	void Send(const std::vector<char>& data) {
//#ifdef __EMSCRIPTEN__
//		EM_ASM_({
//			// TODO::
//			conn.send(data);
//		}, conn, data);
//#endif
//	}
//};

WebRTC* passThrough(size_t ptr) {
	return reinterpret_cast<WebRTC*>(ptr);
}
WebRTC::WebRTC(const char * hostname, unsigned short port, const char *id, const Session& session) : session(session) {
#ifdef __EMSCRIPTEN__
	EM_ASM_({
		var hostname = Module.UTF8ToString($0);
		var id = Module.UTF8ToString($2);
		//console.log('>>>>>> ' + hostname + ' ' + $1 + ' ' + id + ' ' + $3);
		WebRTCPeer.init(hostname + ':' + $1, id, new Module.WebRTC($3));
	}, hostname, port, id, this);
#endif
}
void WebRTC::Send(const char* msg, size_t len) {
	sent += len;
#ifdef __EMSCRIPTEN__
	EM_ASM_({
		var buffer = new Uint8Array(Module.HEAPU8.buffer, $0, $1);
		WebRTCPeer.send(buffer);
	}, msg, len);
#endif
}
void WebRTC::WSSend(const char* msg, size_t len) {
#ifdef __EMSCRIPTEN__
	EM_ASM_({
		var buffer = new Uint8Array(Module.HEAPU8.buffer, $0, $1);
		WebRTCPeer.wsSend(buffer);
	}, msg, len);
#endif
}
void WebRTC::OnMessage(const std::string& str) {
	received += str.size();
	session.onMessage(str.c_str(), str.size());
}

void WebRTC::WSOnMessage(const std::string& str) {
	session.wsOnMessage(str.c_str(), str.size());
}

void WebRTC::OnClose() {
	session.onClose();
}
void WebRTC::OnOpen() {
	session.onOpen();
}
void WebRTC::OnError() {
	LOG_INFO(">>>>>>OnError");
}
void WebRTC::OnConnect(bool initial) {
	session.onConnect(initial);
}
void WebRTC::RTCConnect() {
#ifdef __EMSCRIPTEN__
	EM_ASM(WebRTCPeer.rtcConnect());
#endif
}
#ifdef __EMSCRIPTEN__
EMSCRIPTEN_BINDINGS(WebRTC_Binding) {
	class_<WebRTC>("WebRTC")
		.constructor(&passThrough, allow_raw_pointers())
		.function("OnMessage", &WebRTC::OnMessage, allow_raw_pointers())
		.function("WSOnMessage", &WebRTC::WSOnMessage, allow_raw_pointers())
		.function("OnClose", &WebRTC::OnClose)
		.function("OnError", &WebRTC::OnError)
		.function("OnConnect", &WebRTC::OnConnect)
		.function("OnOpen", &WebRTC::OnOpen);
}
#endif
