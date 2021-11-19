#include <iostream>
#include <WS2tcpip.h>
#include <vector>
#include <memory>
#include <thread>
#include <string>

#pragma comment (lib, "ws2_32.lib")

class ClientHandler {
	std::string _hostName;
public:
	void operator()(SOCKET s, std::string hostName) {
		_hostName = hostName;
		int bytesReceived = 0;
		do {
			char buf[4096];
			ZeroMemory(buf, 4096);

			// wait for client to send data
			std::cout << "waiting for bytes" << std::endl;
			bytesReceived = recv(s, buf, 4096, 0);
			std::cout << "gottem" << std::endl;
			if (bytesReceived == SOCKET_ERROR) {
				std::cerr << "Error in recv(), quitting" << std::endl;
				break;
			}

			if (bytesReceived == 0) {
				std::cout << "Client disconnected " << std::endl;
				break;
			}

			// Echo message back to client
			// The +1 is for the terminating character, since we don't get that on receive
			std::cout << "buf: " << buf << std::endl;
			std::string userString = "User " + _hostName + " sent: " + std::string(buf) + "\n";
			std::cout << "userstring: " << userString << std::endl;
			int userStringBytes = sizeof(userString);
			send(s, userString.c_str(), userStringBytes, 0);
		} while (bytesReceived > 0);
		closesocket(s);
	}
};

void main() {
	// initialize winsock
	WSADATA wsData;
	WORD ver = MAKEWORD(2, 2);

	int wsOk = WSAStartup(ver, &wsData);
	if (wsOk != 0) {
		std::cerr << "can't initialize winsock! quitting!" << std::endl;
		return;
	}

	// create a socket
	// SOCK_STREAM is TCP socket
	SOCKET listening = socket(AF_INET, SOCK_STREAM, 0);
	if (listening == INVALID_SOCKET) {
		std::cerr << "Can't create a socket, quitting" << std::endl;
		WSACleanup();
		return;
	}

	// bind the ip address and port to the socket
	std::string ipAddress = "127.0.0.1";
	sockaddr_in hint;
	hint.sin_family = AF_INET;
	// host to network short to go between big and little endian
	hint.sin_port = htons(54000);
	inet_pton(AF_INET, ipAddress.c_str(), &hint.sin_addr);

	bind(listening, (sockaddr*)&hint, sizeof(hint));

	// tell winsock the socket is for listening
	listen(listening, SOMAXCONN);
	std::cout << "listening" << std::endl;
	std::vector<std::unique_ptr<std::thread>> threads;
	while (true) {
		// wait for connection
		sockaddr_in client;
		int clientSize = sizeof(client);

		SOCKET clientSocket = accept(listening, (sockaddr*)&client, &clientSize);
		if (clientSocket == INVALID_SOCKET) {
			std::cerr << "can't create client socket, quitting" << std::endl;
			WSACleanup();
			return;
		}

		char host[NI_MAXHOST];    // Client's remote name
		char service[NI_MAXHOST]; // Service (i.e. port) the client is connected on

		ZeroMemory(host, NI_MAXHOST);
		ZeroMemory(service, NI_MAXHOST);

		std::string hostName;
		if (getnameinfo((sockaddr*)&client, sizeof(client), host, NI_MAXHOST, service, NI_MAXSERV, 0) == 0) {
			std::cout << host << " connected on port " << service << std::endl;
			hostName = std::string(host) + ":" + std::string(service);
		}
		else
		{
			inet_ntop(AF_INET, &client.sin_addr, host, NI_MAXHOST);
			std::cout << host << " connected on port " << ntohs(client.sin_port) << std::endl;
			hostName = std::string(host) + ":" + std::to_string(client.sin_port);
		}


		threads.emplace_back(new std::thread((ClientHandler()), clientSocket, hostName));
	}

	// close listening socket
	closesocket(listening);
	// Cleanup winsock
	WSACleanup();
}