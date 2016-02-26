#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <vector>
#include <netdb.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

class Socket {
	struct server_t {
		int fd;
	};

	struct client_t
	{
		int fd;
		struct sockaddr_in addr;
	};

	server_t server;
	client_t client;
	void cleanup()
	{
		if (client.fd)
		{
			close(client.fd);
			client.fd = 0;
		}
		if (server.fd)
		{
			close(server.fd);
			server.fd = 0;
		}
	}

	void finish(int result) {
		cleanup();
#ifdef __EMSCRIPTEN__
		//REPORT_RESULT();
		emscripten_force_exit(result);
#else
		exit(result);
#endif
	}

	static void error_callback(int fd, int err, const char* msg, void* _this)
	{
		emscripten_log(EM_LOG_CONSOLE, "error");
		int error;
		socklen_t len = sizeof(error);

		int ret = getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len);
		printf("error_callback\n");
		printf("error message: %s\n", msg);

		if (err == error)
		{
			((Socket*)_this)->finish(EXIT_SUCCESS);
		}
		else
		{
			((Socket*)_this)->finish(EXIT_FAILURE);
		}
	}

	static void message_callback(int fd, void* _this)
	{
		char msg[1024];
		memset(msg, 0, sizeof(msg));
		int res = recvfrom(fd, msg, 1024, 0, NULL, NULL);
		if (res == -1) {
			assert(errno == EAGAIN);
		}
		printf("message callback %d %s\n", res, msg);
	}
	static void open_callback(int fd, void* _this)
	{
		printf("open_callback\n");

	}
public:
	Socket(const char * inet_addr, unsigned short port) {
		int res = 0;
		memset(&client, 0, sizeof(client_t));
		client.fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (client.fd == -1)
		{
			perror("cannot create socket");
			exit(EXIT_FAILURE);
		}
		fcntl(client.fd, F_SETFL, O_NONBLOCK);
		struct hostent *host;
		host = gethostbyname(inet_addr);
		if (host == NULL)
		{
			perror("no such host");
			exit(EXIT_FAILURE);
		}
		memset(&client.addr, 0, sizeof(client.addr));
		client.addr.sin_family = AF_INET;
		client.addr.sin_port = htons(port);
		bcopy((char *)host->h_addr,
			(char *)&client.addr.sin_addr.s_addr,
			host->h_length);
		//if (inet_pton(AF_INET, inet_addr, &client.addr.sin_addr) != 1)
		//{
		//	perror("inet_pton failed");
		//	exit(EXIT_FAILURE);
		//}
		res = connect(client.fd, (struct sockaddr *)&client.addr, sizeof(client.addr));
		emscripten_log(EM_LOG_CONSOLE, "connect: %d", res);
		if (res == -1 && errno != EINPROGRESS)
		{
			perror("connect failed");
			finish(EXIT_FAILURE);
		}

		emscripten_set_socket_error_callback(this, error_callback);
		emscripten_set_socket_open_callback(this, open_callback);
		emscripten_set_socket_message_callback(this, message_callback);
	}
	int Send(const std::string& msg) {
		int res = 0;
		fd_set fdr;
		fd_set fdw;
		FD_ZERO(&fdr);
		FD_ZERO(&fdw);
		FD_SET(client.fd, &fdr);
		FD_SET(client.fd, &fdw);
		res = select(64, &fdr, &fdw, NULL, NULL);
		if (res == -1)
		{
			perror("select failed");
			finish(EXIT_FAILURE);
			return -1;
		}
		if (!FD_ISSET(client.fd, &fdw))
		{
			perror("isset failed for write");
			return -1;
		}
		res = ::send(client.fd, msg.c_str(), msg.size(), 0);
		if (res == -1)
		{
			assert(errno == EAGAIN);
			return res;
		}
		return res;
	}
};

class Client : public Socket {

};
