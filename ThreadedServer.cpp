#include <WS2tcpip.h>

#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>
#include <string>
#include <sstream>

#pragma comment (lib, "ws2_32.lib")

// mutex for increment/decrement concurrentThreads
std::mutex threadMu;
static int concurrentThreads = 0;

// Used to lock std::cout for pretty printing to console
std::mutex coutMu;

void handleConnection(SOCKET clientSocket, int i) {
	threadMu.lock();
	concurrentThreads++;
	coutMu.lock();
	std::cout << "Thread " << i << "connected, concurrentThreads: " << concurrentThreads << std::endl;
	coutMu.unlock();
	threadMu.unlock();

	const int BUFSIZE = 4096;
	char buf[BUFSIZE];
	ZeroMemory(buf, BUFSIZE);

	// Receive a message from the client
	int byteCount = recv(clientSocket, buf, BUFSIZE, 0);

	coutMu.lock();
	std::cout << "Thread " << i << " received message" << std::endl;
	coutMu.unlock();

	if (byteCount == SOCKET_ERROR) {
		coutMu.lock();
		std::cout << "Error in recv(), quitting" << std::endl;
		coutMu.unlock();
		return;
	}

	if (byteCount == 0) {
		coutMu.lock();
		std::cout << "Client disconnected " << std::endl;
		coutMu.unlock();
		return;
	}

	// Determine if the requested file exists
	std::ifstream f(buf);
	if (!f.good()) {
		std::string text = "File doesn't exist!";
		strcpy_s(buf, text.c_str());
		byteCount = strlen(text.c_str());
		// tell the client the file doesn't exist and close the socket
		send(clientSocket, buf, byteCount, 0);
		closesocket(clientSocket);
		return;
	}

	// write the contents of the file to stringstream, and then a string
	std::ostringstream ss_buf;
	ss_buf << f.rdbuf();
	std::string outText = ss_buf.str();

	// Send the contents of the file back to the client
	strcpy_s(buf, outText.c_str());
	byteCount = strlen(outText.c_str());

	coutMu.lock();
	std::cout << "Thread " << i << "sending message" << std::endl;
	coutMu.unlock();

	// A pause just so we can get more concurrent threads, this operation
	// is so quick that usually there aren't more than 1 or 2 concurrent threads.
	// Sleeping 25ms usually gives around 13-15 concurrent threads at a time
	Sleep(25);
	send(clientSocket, buf, byteCount, 0);
	closesocket(clientSocket);
	f.close();

	threadMu.lock();
	coutMu.lock();
	concurrentThreads--;
	std::cout << "Thread " << i << " is closing the connection! concurrentThreads: " << concurrentThreads << std::endl;
	coutMu.unlock();
	threadMu.unlock();
}

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
	std::cout << "Listening for connections..." << std::endl;

	// Keep track of the thread count for concurrency
	int threads = 1;
	while (true) {
		// wait for connection
		sockaddr_in client;
		int clientSize = sizeof(client);

		// accept a new client
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

		if (getnameinfo((sockaddr*)&client, sizeof(client), host, NI_MAXHOST, service, NI_MAXSERV, 0) == 0) {
			coutMu.lock();
			std::cout << host << " connected on port " << service << std::endl;
			coutMu.unlock();
		}
		else
		{
			inet_ntop(AF_INET, &client.sin_addr, host, NI_MAXHOST);
			coutMu.lock();
			std::cout << host << " connected on port " << ntohs(client.sin_port) << std::endl;
			coutMu.unlock();
		}

		// tell the user to request a file
		std::string reqString = "Please request a file: ";
		send(clientSocket, reqString.c_str(), strlen(reqString.c_str()), 0);
		// create the thread for the current client --> server connection
		std::thread th(handleConnection, clientSocket, threads);
		// detach it to keep it running as it leaves scope
		th.detach();
		threads++;
	}

	// close listening socket
	closesocket(listening);
	// Cleanup winsock
	WSACleanup();
}