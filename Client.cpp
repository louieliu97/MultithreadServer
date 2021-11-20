#include <WS2tcpip.h>

#include <exception>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#pragma comment (lib, "ws2_32.lib")

// used to lock and unlock std::cout for pretty-printing to console/file
std::mutex mu;

static const std::string EXPECTED_FILE_CONTENTS = "Hello, this is some random text that I've generated for the purpose of a multithreaded server test.";

void createConnection(int id) {
	const std::string IP_ADDRESS = "127.0.0.1";
	const int PORT = 54000;
	const std::string FILENAME = "RandomText.txt";

	// Create tcp socket
	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET)
	{
		WSACleanup();
		throw std::runtime_error("Can't create socket");
	}

	sockaddr_in hint;
	hint.sin_family = AF_INET;
	hint.sin_port = htons(PORT);
	inet_pton(AF_INET, IP_ADDRESS.c_str(), &hint.sin_addr);

	// create the connection to the server
	int connResult = connect(sock, (sockaddr*)&hint, sizeof(hint));
	if (connResult == SOCKET_ERROR)
	{
		closesocket(sock);
		WSACleanup();
		throw std::runtime_error("Can't connect to server");
	}
	
	mu.lock();
	std::cout << "Thread with id: " << id << " connected to server!" << std::endl;
	mu.unlock();

	const int BUFSIZE = 4096;
	char buf[BUFSIZE];
	ZeroMemory(buf, BUFSIZE);

	// Receive message from server asking to provide a filename
	int byteCount = recv(sock, buf, BUFSIZE, 0);
	if (byteCount == SOCKET_ERROR) {
		throw std::runtime_error("Error receiving message");
	}

	if (byteCount == 0) {
		throw std::runtime_error("Server disconnected");
	}

	// send the name of the known file
	strcpy_s(buf, FILENAME.c_str());
	byteCount = strlen(FILENAME.c_str());
	send(sock, buf, byteCount, 0);

	// receive the message
	byteCount = recv(sock, buf, BUFSIZE, 0);
	std::string fileContents = std::string(buf);
	mu.lock();
	std::cout << "Thread with id: " << id << " received: " << fileContents << std::endl;
	mu.unlock();

	if (fileContents != EXPECTED_FILE_CONTENTS) {
		throw std::runtime_error("The contents of the file are unexpected!");
	}
}

void main() {
	WSADATA wsData;
	WORD ver = MAKEWORD(2, 2);
	const int NUM_THREADS = 100;

	int wsOk = WSAStartup(ver, &wsData);
	if (wsOk != 0) {
		std::cerr << "can't initialize winsock! quitting!" << std::endl;
		return;
	}

	// create a vector of threads
	std::vector<std::unique_ptr<std::thread>> threads;
	for (int i = 1; i <= NUM_THREADS; i++) {
		// spawn a new thread and create a new connection
		threads.emplace_back(new std::thread(createConnection, i));
	}

	// join all the threads after spawning
	for (int i = 0; i < NUM_THREADS; i++) {
		threads.at(i)->join();
		mu.lock();
		std::cout << "Destroying thread with id: " << i+1 << std::endl;
		mu.unlock();
	}
}