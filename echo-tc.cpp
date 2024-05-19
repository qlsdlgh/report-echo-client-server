#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef __linux__
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif // __linux
#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#include "../mingw_net.h"
#endif // WIN32
#include <iostream>
#include <thread>
#include <cstring>
#include <cstdlib>

#ifdef WIN32
void myerror(const char* msg) { fprintf(stderr, "%s %ld\n", msg, GetLastError()); }
#else
void myerror(const char* msg) { fprintf(stderr, "%s %s %d\n", msg, strerror(errno), errno); }
#endif

void usage() {
	std::cout << "syntax: echo-client <ip> <port>\n";
    std::cout << "sample: echo-client 127.0.0.1 1234\n";
}

struct Param {
	char* ip{nullptr};
	char* port{nullptr};
	uint32_t srcIp{0};
	uint16_t srcPort{0};

	bool parse(int argc, char* argv[]) {
		for (int i = 1; i < argc;) {
			if (strcmp(argv[i], "-si") == 0) {
				int res = inet_pton(AF_INET, argv[i + 1], &srcIp);
				switch (res) {
					case 1: break;
					case 0: fprintf(stderr, "not a valid network address\n"); return false;
					case -1: myerror("inet_pton"); return false;
				}
				i += 2;
				continue;
			}

			if (strcmp(argv[i], "-sp") == 0) {
				srcPort = atoi(argv[i + 1]);
				i += 2;
				continue;
			}

			ip = argv[i++];
			if (i < argc) port =argv[i++];
		}
		return (ip != nullptr) && (port != nullptr);
	}
} param;

void recvThread(int sd) {
	printf("connected\n");
	fflush(stdout);
	static const int BUFSIZE = 65536;
	char buf[BUFSIZE];
	while (true) {
		ssize_t res = ::recv(sd, buf, BUFSIZE - 1, 0);
		if (res == 0 || res == -1) {
			fprintf(stderr, "recv return %ld", res);
			myerror(" ");
			break;
		}
		buf[res] = '\0';
		printf("%s", buf);
		fflush(stdout);
	}
	printf("disconnected\n");
	fflush(stdout);
	::close(sd);
	exit(0);
}

int main(int argc, char* argv[]) {
	if (argc != 3) {
        usage();
        return -1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);
#ifdef WIN32
	WSAData wsaData;
	if(WSAStartup(MAKEWORD(2,2), &wsaData) != 0){
		std::cerr << "WSAStartup failed.\n";
		return -1;
	}
#endif // WIN32

#ifdef __linux__
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1){
		perror("socket");
		return -1;
	}

	sockaddr_in serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port);
	if(inet_pton(AF_INET, ip, &serverAddr.sin_addr) <= 0){
		perror("inet_pton");
		close(sock);
		return -1;
	}

	if (connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1){
		perror("connect");
		close(sock);
		return -1;
	}
#elif defined(WIN32)
	Socket sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock == INVALID_SOCKET) {
		std::cerr << "Socket creation failed with error: " << WSAGetLastError() << std:endl;
		WSACleanup();
		return -1;
	}

	sockaddr_in serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port);
	serverAddr.sin_addr.s_addr = inet_addr(ip);

	if (connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		std::cerr << "Connect failed with error: " << WSAGetLastError() << std::endl;
		closesocket(sock);
		WSACleanup();
		return -1;
	}
#endif

	std::string message;
	while (true) {
		std::getline(std::cin, message);
		if(message == "exit"){
			break;
		}
		send(sock, message.c_str(), message.length(), 0);
	}
#ifdef __linux__
	close(sock);
#elif defined(WIN32)
	closesocket(sock);
	WSACleanup();
#endif
	return 0;
}