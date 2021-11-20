#include <WS2tcpip.h>

#include <condition_variable >
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <shared_mutex>
#include <string>
#include <sstream>

#pragma comment (lib, "ws2_32.lib")

// Number of ms to sleep to assist in multithreading as the operations
// are very fast.
static constexpr int SLEEPY_TIME = 250;
// With SLEEPY_TIME == 25:
// If MAX_THREADS == 5, the queue is consistently full
// If MAX_THREADS == 10, the queue is sometimes fully, but usually hovering between 7 and 9 concurrent threads
// IF MAX_THREADS == 20, The queue is never fully, and on average has 5-6 concurrent threads
static constexpr int MAX_THREADS = 5;

// mutex for the total users that have connected over the lifetime of the server
std::mutex usersMu;
static int totalUsersConnected = 0;

// mutex for concurrentThreads being run out of MAX_THREADS
std::mutex concThreadsMu;
static int concurrentThreads = 0;

// mutex for the wrapped socket queue
std::mutex socketMu;
static std::queue<SOCKET> socketQueue;
// We use a condition variable to notify threads when the queue has been added to
// instead of the threads constantly checking whether the queue is empty or not, which
// consumes a lot of CPU
std::condition_variable  socketCV;

// mutex for std::cout
std::mutex coutMu;

// get the current total users connected
int readUserNumber() {
	std::unique_lock<std::mutex> sharedLock(usersMu);
	return totalUsersConnected;
}

// increment the total users connected
void incrementUserNumber() {
	std::unique_lock<std::mutex> lock(usersMu);
	totalUsersConnected++;
}

// get the current number of concurrent threads
int readConcurrentThreads() {
	std::unique_lock<std::mutex> sharedLock(concThreadsMu);
	return concurrentThreads;
}

// increment the number of concurrent threads
void incrementConcurrentThreads() {
	std::unique_lock<std::mutex> sharedLock(concThreadsMu);
	concurrentThreads++;
}

// decrement the number of concurrent threads
void decrementConcurrentThreads() {
	std::unique_lock<std::mutex> sharedLock(concThreadsMu);
	concurrentThreads--;
}

// Function that handles the connection once popped from the queue
void handleConnection(SOCKET clientSocket, int userNumber, int threadNumber) {
	incrementConcurrentThreads();
	coutMu.lock();
	std::cout << "User number " << userNumber << " on thread " << threadNumber << " has connected." << std::endl;
	coutMu.unlock();

	// tell the user to request a file
	std::string reqString = "Please request a file: ";
	send(clientSocket, reqString.c_str(), strlen(reqString.c_str()), 0);

	const int BUFSIZE = 4096;
	char buf[BUFSIZE];
	ZeroMemory(buf, BUFSIZE);

	// Receive a message from the client
	int byteCount = recv(clientSocket, buf, BUFSIZE, 0);

	coutMu.lock();
	std::cout << "User number " << userNumber << " on thread " << threadNumber << " received message" << std::endl;
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
	f.close();

	// Send the contents of the file back to the client
	strcpy_s(buf, outText.c_str());
	byteCount = strlen(outText.c_str());

	coutMu.lock();
	std::cout << "User number " << userNumber << " on thread " << threadNumber << " is sending message" << std::endl;
	coutMu.unlock();

	// A pause just so we can get more concurrent threads, this operation
	// is so quick that usually there aren't more than 1 or 2 concurrent threads naturally.
	Sleep(SLEEPY_TIME);
	send(clientSocket, buf, byteCount, 0);
	closesocket(clientSocket);

	int concurrentThreadsNow = readConcurrentThreads();
	coutMu.lock();
	std::cout << "User number " << userNumber << " on thread " << threadNumber << " is closing the connection!" << std::endl;
	std::cout << "concurrentThreads: " << concurrentThreadsNow << std::endl;
	if (concurrentThreadsNow > MAX_THREADS) {
		throw std::runtime_error("Error! concurrent threads exceeds MAX_THREADS, something has gone wrong!");
	}
	std::cout << "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Ending handleConnection on thread " << threadNumber << std::endl;
	coutMu.unlock();
	// Thread is dying, decrement the counter of concurrent threads.
	decrementConcurrentThreads();
}

void handleQueue(int threadNumber) {
	while (true) {
		/*
		 * Part of the improvement from the previous thread pool solution is we can use a condition_variable
		 * along with the predicate to only check the queue after something has been pushed to it.
		 * That way we're not consuming all of the CPU constantly checking the state of the queue.
		 * */
		std::unique_lock<std::mutex> socketLock(socketMu);
		// We wait for something to push to the queue. We add the predicate as a sanity check to ensure
		// the queue is not empty, as sometimes conditional waits can errorneously pass without a predicate.
		socketCV.wait(socketLock, [] { return !socketQueue.empty(); });
		// Thread begins
		SOCKET client = socketQueue.front(); // Get the client socket
		socketQueue.pop();
		coutMu.lock();
		std::cout << "Popping from queue, queue size: " << socketQueue.size() << std::endl;
		coutMu.unlock();
		socketLock.unlock();
		// We pop from the queue, and notify a waiting thread to check if the queue is empty, as this thread is
		// done with access to the queue.
		socketCV.notify_one();


		// increment the total users that have connected to the server
		incrementUserNumber();
		int userNumber = readUserNumber();
		coutMu.lock();
		std::cout << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Starting handleConnection on thread  " << threadNumber << std::endl;
		coutMu.unlock();
		handleConnection(client, userNumber, threadNumber); // let this function handle it now.
	}
}

int main() {
	// initialize winsock
	WSADATA wsData;
	WORD ver = MAKEWORD(2, 2);

	int wsOk = WSAStartup(ver, &wsData);
	if (wsOk != 0) {
		std::cerr << "can't initialize winsock! quitting!" << std::endl;
		return 0;
	}

	// create a socket
	// SOCK_STREAM is TCP socket
	SOCKET listening = socket(AF_INET, SOCK_STREAM, 0);
	if (listening == INVALID_SOCKET) {
		std::cerr << "Can't create a socket, quitting" << std::endl;
		WSACleanup();
		return 0;
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

	std::vector<std::thread> threads;
	for (int i = 0; i < MAX_THREADS; i++) {
		std::cout << "create thread: " << i + 1 << std::endl;
		threads.emplace_back(std::thread(handleQueue, i + 1));
	}

	while (true) {
		// wait for connection
		sockaddr_in client;
		int clientSize = sizeof(client);

		// accept a new client
		SOCKET clientSocket = accept(listening, (sockaddr*)&client, &clientSize);
		if (clientSocket == INVALID_SOCKET) {
			WSACleanup();
			return 0;
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
		// we got a connection, insert into thread pool

		coutMu.lock();
		std::cout << "pushing to queue" << std::endl;
		{
			std::lock_guard<std::mutex> socketLock(socketMu);
			socketQueue.push(clientSocket);
			std::cout << "pushed to queue. Queue size: " << socketQueue.size() << std::endl;

		}
		socketCV.notify_one();
		coutMu.unlock();
	}

	// close listening socket
	closesocket(listening);
	// Cleanup winsock
	WSACleanup();
	return 0;
}