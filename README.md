# MultithreadServer

A small example of various types of multi-threading in a very simple socket connection. All that happens is the client requests a file by name, if the file exists, the server will return the contents of that file.

There are two main examples, ThreadPoolServer and ThreadedServer.

## ThreadedServer
The simpler of the two that uses `std::thread` with no limit. As connections come in, a socket is created which spawns a thread to handle the connection. The thread then detaches and dies as the function that handles the connection dies.

The current number of concurrentThreads at any given time is also kept track of, while being thread-safe using a mutex. This value is printed out as a thread is spawned and is about to be despawned. 

Access to cout is also limited by a mutex.

##ThreadPoolServer
This solution introduces using a queue to store created sockets while it waits for the next available thread. The number of spawned threads is determined ahead of time, and in the current code is set to 20. If all twenty are utilized to handle a socket connection, the next available socket S waits in the queue until a thread completes handling a previous socket connection, and pops socket S off the queue to handle that connection.

In this example, the critical section happens between the time the queue is checked for being not empty, getting the front of the queue, then popping that socket off the queue. Thus, that section must use a mutex to lock during that period of time. Additionally, the mutex will lock the queue when pushing to it.

There are a few more shared variables that are kept track of to print, as to better show the behavior and proving that the thread pool works as intended.

1. Total users that have connected to the server in its lifetime.
2. The concurrent threads being used measured at the end of the connections lifetime.

We also keep track on a per thread basis, the number it is.

With these two extra variables, and keeping track of the thread number, we can associate user numbers of thread numbers to ensure as a thread frees up, it takes in the next user in the queue. We also can see the number of concurrentThreads being used at a time, and we expect this value to always be <= MAX_THREADS.
