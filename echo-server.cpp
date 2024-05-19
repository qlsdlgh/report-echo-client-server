#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef __linux__
#include <arpa/inet.h>
#include <sys/socket.h>
#endif // __linux
#ifdef WIN32
#include <winsock2.h>
#include "../mingw_net.h"
#include <ws2tcppip.h>
#pragma comment(lib, "ws2_32.lib")
#endif // WIN32
#include <thread>
#include <iostream>
#include <vector>
#include <mutex>
#include <cstring>
#include <cstdlib>
#include <netinet/in.h>

std::vector<int> clients;
std::mutex mtx;

#ifdef WIN32
void myerror(const char* msg) { fprintf(stderr, "%s %ld\n", msg, GetLastError()); }
#else
void myerror(const char* msg) { fprintf(stderr, "%s %s %d\n", msg, strerror(errno), errno); }
#endif

void usage() {
	std::cout << "syntax: echo-server <port> [-e] [-b]\n";
    std::cout << "sample: echo-server 1234 -e -b\n";
}

struct Param {
	bool echo{false};
    bool broadcast{false};
	uint16_t port{0};
	uint32_t srcIp{0};

	bool parse(int argc, char* argv[]) {
		for (int i = 1; i < argc;i++) {
			if (strcmp(argv[i], "-e") == 0) {
				echo = true;
				continue;
			}

			if (strcmp(argv[i], "-b") == 0) {
				broadcast = true;
                continue;
			}
			port = atoi(argv[i]);
        }
		return port != 0;
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
		if (param.echo) {
			res = ::send(sd, buf, res, 0);
			if (res == 0 || res == -1) {
				fprintf(stderr, "send return %ld", res);
				myerror(" ");
				break;
			}
		}
	}
	printf("disconnected\n");
	fflush(stdout);
	::close(sd);
}

void clientHandlers(int clientSocket){
    char buf[1024];
    while (true) {
        memset(buf, 0, sizeof(buf));
        ssize_t received = recv(clientSocket, buf, sizeof(buf) - 1, 0);
        if (received == 0 || received == -1) {
#ifdef __linux__
            close(clientSocket);
#elif defined(Win32)
			closesocket(clientSocket)
#endif
            mtx.lock();
            clients.erase(std::remove(clients.begin(), clients.end(), clientSocket), clients.end());
            mtx.unlock();
            break;
        }
        std::cout << "Received: " << buf << std::endl;

        if (param.echo) {
            send(clientSocket, buf, strlen(buf), 0);
        }

        if (param.broadcast) {
            mtx.lock();
            for (int sock : clients) {
                if (sock != clientSocket) { // Do not echo to self
                    send(sock, buf, strlen(buf), 0);
                }
            }
            mtx.unlock();
        }
    }
}
int main(int argc, char* argv[]) {
	if (!param.parse(argc, argv)) {
		usage();
		return -1;
	}

#ifdef WIN32
	WSAData wsaData;
	if(WSAStartup(MAKEWORD(2,2), &wsaData) != 0){
		std::cerr << "WSAStartup failed.\n";
		return -1;
	}
#endif // WIN32

#ifdef __linux__
	int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSocket == -1){
		std::cerr << "socket failed\n";
		return -1;
	}

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(param.port);

    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
        std::cerr << "bind failed\n";
        return -1;
    }

    if (listen(serverSocket, 5) == -1) {
        std::cerr << "listen failed\n";
        return -1;
    }
#elif defined(WIN32)
	SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSocket == INVALID_SOCKET){
		std::cerr << "Socket creation failed with error: " << WSAGetLastError() << std::endl;
		WSACleanup();
		return -1;
	}

	sockaddr_in serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddr.sin_port = htons(param.port);

	if(bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		std::cerr << "Bind failed with error: " << WSAGetLastError() << std::endl;
		closesocket(serverSocket);
		WSACleanup();
		return -1;
	}

	if (listen(serverSocket, 5) == SOCKET_ERROR){
		std::cerr << "Listen failed with error: " << WSAGetLastError() << std::endl;
		closesocket(serverSocket);
		WSACleanup();
		return -1;
	}
#endif // __linux
	while(true){
		sockaddr_in clientAddr;
		socklen_t clientAddrSize = sizeof(clientAddr);
#ifdef __linux__
		int clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &clientAddrSize);
#elif defined(WIN32)
		SOCKET clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &clientAddrSize);
#endif
		if(clientSocket == -1){
			std::cerr << "accept failed\n"
			continue;
		}

		mtx.lock();
		client.push_back(clientSocket);
		mtx.unlock();

		std::thread t(clientHandler, clientSocket);
		t.detach();		
	}
#ifdef __linux__
	close(serverSocket);
#elif defined(WIN32)
	closesocket(serverSocket);
	WSACleanup();
#endif
	return 0;
}