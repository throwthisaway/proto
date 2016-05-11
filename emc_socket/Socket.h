
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <vector>
#ifdef __EMSCRIPTEN__
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <emscripten.h>
#else
#include <Winsock2.h>
#include <ws2tcpip.h>
#include <tuple>
#pragma comment(lib, "Ws2_32.lib")
#endif
#undef min
#undef max
#ifdef __EMSCRIPTEN__
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif
struct Session {
	std::function<void()> onOpen, onClose;
	std::function<void(int, const char*)> onError;
	std::function<void(const char*, int)> onMessage;
};
class Socket {
protected:
	const Session session;
	SOCKET fd;

	void cleanup()
	{
#ifdef __EMSCRIPTEN__
		if (fd)
			close(fd);
#else
		if (fd)
			closesocket(fd);
		::WSACleanup();
#endif
		fd = 0;
	}

	void finish(int result) {
		cleanup();
//#ifdef __EMSCRIPTEN__
//		//REPORT_RESULT();
//		emscripten_force_exit(result);
//#else
//		exit(result);
//#endif
	}

	static void error_callback(int fd, int err, const char* msg, void* _this)
	{
		char error;
		socklen_t len = sizeof(error);

		int ret = getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len);

		printf("error message: %s\n", msg);
		auto socket = (Socket*)_this;
		if (err == error)
		{
			socket->finish(EXIT_SUCCESS);
		}
		else
		{
			socket->finish(EXIT_FAILURE);
		}
		
		if (socket->session.onError) {
			socket->session.onError(err, msg);
		}
	}

	static void message_callback(int fd, void* _this)
	{
		char msg[1024*10];
		memset(msg, 0, sizeof(msg));
		int res = recv(fd, msg, sizeof(msg), 0);
		if (res == -1) {
			assert(errno == EAGAIN);
		}
		//printf("message callback %d %s\n", res, msg);
		auto socket = (Socket*)_this;
		if (socket->session.onMessage) {
			socket->session.onMessage(msg, res);
		}
	}
	static void open_callback(int fd, void* _this)
	{
		printf("open_callback\n");
		auto socket = (Socket*)_this;
		if (socket->session.onOpen) {
			socket->session.onOpen();
		}
	}
	static void close_callback(int fd, void* _this)
	{
		printf("close_callback\n");
		auto socket = (Socket*)_this;
		if (socket->session.onClose) {
			socket->session.onClose();
		}
	}
public:
	Socket(const Session& session) : session(session), fd(0) {
#ifdef __EMSCRIPTEN__
		emscripten_set_socket_error_callback(this, error_callback);
		emscripten_set_socket_open_callback(this, open_callback);
		emscripten_set_socket_close_callback(this, close_callback);
		emscripten_set_socket_message_callback(this, message_callback);
#endif
	}
	~Socket() {
		cleanup();
	}
};

class Client : public Socket {
	struct sockaddr_in addr;
	bool async;
public:
	Client(const char * inet_addr, unsigned short port, const Session& session, bool async = true, bool host_by_name = true) : Socket(session),
		async(async) {
		int res = 0;
		fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (fd == -1)
		{
			perror("cannot create socket");
			exit(EXIT_FAILURE);
		}
		if (async) {
#ifdef __EMSCRIPTEN__
			fcntl(fd, F_SETFL, O_NONBLOCK);
			// ?? fcntl(s, F_SETFF, FNDELAY);
#else
			unsigned long arg = -1;
			ioctlsocket(fd, FIONBIO, &arg);
#endif
		}
		if (host_by_name) {
			// TODO:: getnameinfo https://msdn.microsoft.com/en-us/library/windows/desktop/ms738532(v=vs.85).aspx
			struct hostent *host;
			host = gethostbyname(inet_addr);
			if (host == NULL)
			{
				perror("no such host");
				exit(EXIT_FAILURE);
			}
			memset(&addr, 0, sizeof(addr));
			addr.sin_family = AF_INET;
			addr.sin_port = htons(port);
            memcpy((char *)&addr.sin_addr.s_addr,
				(char *)host->h_addr,
                host->h_length);
			res = connect(fd, (struct sockaddr *)&addr, sizeof(addr));

			if (res == -1 && errno != EINPROGRESS) {
#ifndef __EMSCRIPTEN__
				if (WSAGetLastError() != WSAEWOULDBLOCK)
#endif
				{
#ifdef __EMSCRIPTEN__
					emscripten_log(EM_LOG_ERROR, "connect failed: %d", errno);
#endif
					perror("connect failed");
					finish(EXIT_FAILURE);
				}
			}
		} else {
// TODO:: getaddrinfo https://msdn.microsoft.com/en-us/library/windows/desktop/ms738520(v=vs.85).aspx

//			struct addrinfo *host = NULL, hints;
//			memset(&hints, 0, sizeof(hints));
//			hints.ai_family = AF_UNSPEC;
//			hints.ai_socktype = SOCK_STREAM;
//			hints.ai_protocol = IPPROTO_TCP;
//
//			// Resolve the server address and port
//			res = getaddrinfo(inet_addr, NULL, &hints, &host);
//			if (res != 0) {
//				printf("getaddrinfo failed with error: %d\n", res);
//				exit(EXIT_FAILURE);
//			}
//
//			for (struct addrinfo *ptr = host; ptr != NULL; ptr = ptr->ai_next) {
//
//				// Create a SOCKET for connecting to server
//				fd = socket(ptr->ai_family, ptr->ai_socktype,
//					ptr->ai_protocol);
//				if (fd == INVALID_SOCKET) {
//#ifdef __EMSCRIPTEN__
//					perror("socket failed");
//#else
//					printf("socket failed with error: %ld\n", WSAGetLastError());
//#endif
//					exit(EXIT_FAILURE);
//				}
//
//				// Connect to server.
//				res = connect(fd, ptr->ai_addr, (int)ptr->ai_addrlen);
//				if (res == SOCKET_ERROR) {
//#ifdef __EMSCRIPTEN__
//					close(fd);
//#else
//					closesocket(fd);
//#endif
//					continue;
//				}
//				break;
//			}
//
//			freeaddrinfo(host);
//
//			if (fd == INVALID_SOCKET) {
//				printf("Unable to connect to server!\n");
//				exit(EXIT_FAILURE);
//			}

			memset(&addr, 0, sizeof(addr));
			if (inet_pton(AF_INET, inet_addr, &addr.sin_addr) != 1)
			{
#ifdef __EMSCRIPTEN__
				emscripten_log(EM_LOG_ERROR, "inet_pton failed: %d", errno);
#endif
				perror("inet_pton failed");
				exit(EXIT_FAILURE);
			}
			addr.sin_family = AF_INET;
			addr.sin_port = htons(port);
			res = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
#ifdef __EMSCRIPTEN__
			emscripten_log(EM_LOG_CONSOLE, "connect: %d", res);
#endif

			if (res == -1 && errno != EINPROGRESS)
			{
#ifndef __EMSCRIPTEN__
				if (WSAGetLastError() != WSAEWOULDBLOCK)
#endif
				{
					//auto r = WSAGetLastError();


					perror("connect failed");
					finish(EXIT_FAILURE);
				}
			}
		}
	}
	int Recv() {
		char msg[1024];
		memset(msg, 0, sizeof(msg));
		int res = recv(fd, msg, 1024, 0);
		if (res == -1) {
			assert(errno == EAGAIN);
		}
		printf("message callback %d %s\n", res, msg);
		return res;
	}
	int Send(const char* msg, size_t len) {
		if (!fd) return -1;
		int res = 0;
		fd_set fdr;
		fd_set fdw;
		FD_ZERO(&fdr);
		FD_ZERO(&fdw);
		FD_SET(fd, &fdr);
		FD_SET(fd, &fdw);
		res = select(64, nullptr/*&fdr*/, &fdw, NULL, NULL);
		if (res == -1)
		{
			perror("select failed");
			finish(EXIT_FAILURE);
			return -1;
		}
		//if (FD_ISSET(fd, &fdr))
		//{
		//	message_callback(fd, this);
		//}
		if (!FD_ISSET(fd, &fdw))
		{
			perror("isset failed for write");
			return -1;
		}
//#ifdef __EMSCRIPTEN__
//		auto fd_flags = fcntl(fd, F_GETFL);
//		if (fd_flags & O_NONBLOCK)
//			fcntl(fd, F_SETFL, fd_flags & ~O_NONBLOCK);
//#else
//		if (async) {
//			unsigned long arg = 0;
//			ioctlsocket(fd, FIONBIO, &arg);
//		}
//#endif
		// TODO:: while (sum += res) < len
		res = ::send(fd, msg, len, 0);
//#ifdef __EMSCRIPTEN__
//		fcntl(fd, F_SETFL, fd_flags);
//#else
//		if (async) {
//			unsigned long arg = -1;
//			ioctlsocket(fd, FIONBIO, &arg);
//		}
//#endif
		if (res == -1)
		{
			if (errno != EAGAIN && errno != EINPROGRESS)
				perror("send error");
			assert(errno == EAGAIN || errno == EINPROGRESS);
			return res;
		}
		return res;
	}
	//int Send(const std::string& msg) {
	//	int res = 0;
	//	fd_set fdr;
	//	fd_set fdw;
	//	FD_ZERO(&fdr);
	//	FD_ZERO(&fdw);
	//	FD_SET(fd, &fdr);
	//	FD_SET(fd, &fdw);
	//	res = select(64, &fdr, &fdw, NULL, NULL);
	//	if (res == -1)
	//	{
	//		perror("select failed");
	//		finish(EXIT_FAILURE);
	//		return -1;
	//	}
	//	if (!FD_ISSET(fd, &fdw))
	//	{
	//		perror("isset failed for write");
	//		return -1;
	//	}
	//	res = ::send(fd, msg.c_str(), msg.size(), 0);
	//	if (res == -1)
	//	{
	//		assert(errno == EAGAIN);
	//		return res;
	//	}
	//	return res;
	//}
};

class Server : public Socket {
	const int max_connections = 16;
	std::vector<std::tuple<SOCKET, sockaddr_in>> clients;
public:
	Server(unsigned short port, const Session& session) : Socket(session) {
		int res = 0;
		fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (fd == -1)
		{
			perror("cannot create socket");
			exit(EXIT_FAILURE);
		}
#ifdef __EMSCRIPTEN__
		//fcntl(fd, F_SETFL, O_NONBLOCK);
#else
		unsigned long arg = -1;
		//ioctlsocket(fd, FIONBIO, &arg);
#endif
		sockaddr_in addr;
		memset((char *) &addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		addr.sin_addr.s_addr = INADDR_ANY;
		if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
			perror("bind failed");
			exit(EXIT_FAILURE);
		}
	}
	~Server() {
		for (const auto& client: clients) {
#ifdef __EMSCRIPTEN__
			close(std::get<0>(client));
#else
			closesocket(std::get<0>(client));
#endif
		}
		cleanup();
	}
	void Listen() {
		listen(fd, max_connections);
		sockaddr_in client_addr;
		socklen_t client_addr_len = sizeof(client_addr);
		SOCKET client_fd;
		for (;;) {
			client_fd = accept(fd, (struct sockaddr *)&client_addr, &client_addr_len);
			if (client_fd == INVALID_SOCKET) {
				perror("accept failed");
				exit(EXIT_FAILURE);
			} 
			clients.emplace_back(client_fd, client_addr);
			/* the socket for this accepted connection is rqst */
			//...
		}
	}
	// TODO:: read/write/onmessage/shutdown/disconnect(remove client from list and close the socket)
};
