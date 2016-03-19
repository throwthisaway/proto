// Server1.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "..\..\Socket.h"
int main()
{
	WORD wVersionRequested = MAKEWORD(2, 2);
	WSADATA wsaData;
	int err;
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		/* Tell the user that we could not find a usable */
		/* Winsock DLL.                                  */
		printf("WSAStartup failed with error: %d\n", err);
		return 1;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
		/* Tell the user that we could not find a usable */
		/* WinSock DLL.                                  */
		printf("Could not find a usable version of Winsock.dll\n");
		WSACleanup();
		return 1;
	}
	//Server server(8080);
	//server.Listen();
	Client client("localhost", 8080, false);
	client.Send("GET / HTTP/1.1\r\n\r\n"\
		"Host: localhost:8080\r\n\r\n"\
		"Upgrade : websocket\r\n\r\n"\
		"Connection : Upgrade\r\n\r\n"\
		"Sec - WebSocket - Key : dGhlIHNhbXBsZSBub25jZQ ==\r\n\r\n"\
		"Origin : http://localhost:8080\r\n\r\n"\
		"Sec - WebSocket - Protocol : chat, superchat\r\n\r\n"\
		"Sec - WebSocket - Version : 13\r\n\r\n");
	client.Recv();
	//Client client("127.0.0.1", 8080, false);
	::WSACleanup();
	return 0;
}

