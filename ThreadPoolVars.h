#pragma once
#include <WS2tcpip.h>

#include <atomic>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>

class ThreadPoolVars
{
public:
	ThreadPoolVars() {
		m_concurrentThreads = 0;
		m_totalUsersConnected = 0;
	}
	void incrementConcurrentThreads() { m_concurrentThreads++; }
	void decrementConcurrentThreads() { m_concurrentThreads--; };
	int readConcurrentThreads() { return m_concurrentThreads.load(); };

	void incrementUsersConnected() { m_totalUsersConnected++; };
	int readUsersConnected() { return m_concurrentThreads.load(); };

	void print(std::string s) {
		std::lock_guard<std::mutex> coutLock(m_coutMu);
		std::cout << s << std::endl;
	}

private:
	std::atomic<int> m_totalUsersConnected;
	std::atomic<int> m_concurrentThreads;
	// mutex for std::cout
	std::mutex m_coutMu;
};

