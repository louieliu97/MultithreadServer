# MultithreadServer

A small example of various types of multi-threading in a very simple socket connection. All that happens is the client requests a file by name, if the file exists, the server will return the contents of that file.

There are currently three main examples, ThreadedServer, ThreadPoolServer, and ThreadPoolServerImproved.

## ThreadedServer
The simpler of the two that uses `std::thread` with no limit. As connections come in, a socket is created which spawns a thread to handle the connection. The thread then detaches and dies as the function that handles the connection dies.

The current number of concurrentThreads at any given time is also kept track of, while being thread-safe using a mutex. This value is printed out as a thread is spawned and is about to be despawned. 

Access to cout is also limited by a mutex.

## ThreadPoolServer
This solution introduces using a queue to store created sockets while it waits for the next available thread. The number of spawned threads is determined ahead of time, and in the current code is set to 20. If all twenty are utilized to handle a socket connection, the next available socket S waits in the queue until a thread completes handling a previous socket connection, and pops socket S off the queue to handle that connection.

In this example, the critical section happens between the time the queue is checked for being not empty, getting the front of the queue, then popping that socket off the queue. Thus, that section must use a mutex to lock during that period of time. Additionally, the mutex will lock the queue when pushing to it.

There are a few more shared variables that are kept track of to print, as to better show the behavior and proving that the thread pool works as intended.

1. Total users that have connected to the server in its lifetime.
2. The concurrent threads being used measured at the end of the connections lifetime.

We also keep track on a per thread basis, the number it is.

With these two extra variables, and keeping track of the thread number, we can associate user numbers of thread numbers to ensure as a thread frees up, it takes in the next user in the queue. We also can see the number of concurrentThreads being used at a time, and we expect this value to always be <= MAX_THREADS.

## ThreadPoolServerImproved
The above solution has a glaring problem. In the while loop, it's constantly checking the queue to see whether it's empty or not. For the previous solution, it's the only wait to wait for the queue to be pushed to before kicking off the thread. However, the (huge) downside is that it occupies all of the CPU instructions, making CPU usage 100% and performance extremely bad.

The solution? Use `std::condition_variable`. The idea is to force the thread to wait until it's been notify something has been pushed to the queue. Once it receives the notification, it verifies that the queue is not empty, then takes ownership and pops it from the queue, before relinguishing ownership and notifying another waiting thread that it is no longer being used. The reason it needs to notify again is because the size of the queue might be > 1, so we can't have only pushing to the queue be the notification for a waiting thread to check the status of the queue.

## Client
The Client creates NUM_THREADS connections, currently 100, to the server IP Address and port (Default is 127.0.0.1:54000). It receives a message, then sends a message containing the correct filename to the server. It then receives the contents of that file, and then closes the connection.

## Building And Usage
To build either server, either run it in Visual Studio, or on the Developer CMD for VS 2019, navigate to the folder and run `cl <ServerName>.cpp /EHsc`. After that compiles, run `<Servername>.exe` to start the server.

To compile the client, run `cl Client.cpp /EHsc`, then `Client.exe` to execute. **MAKE SURE THE SERVER IS RUNNING FIRST!**

### Analysis of some Output
There are a few things I want to convince myself of that it's working as intended:

1. Concurrent work is happening
2. The work being done is correct
3. The behavior of the threads as it relates to the queue is correct

#### Is concurrent work happening?
In the following snippet:

```
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Starting handleConnection on thread  1
User number 11 on thread 1 has connected.
User number 7 on thread 3 is closing the connection!
concurrentThreads: 5
<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Ending handleConnection on thread 3
User number 11 on thread 1 received message
User number 8 on thread 4 is closing the connection!
concurrentThreads: 4
<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Ending handleConnection on thread 4
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Starting handleConnection on thread  4
User number 13 on thread 4 has connected.
User number 6 on thread 2 is closing the connection!
concurrentThreads: 4
<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Ending handleConnection on thread 2
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Starting handleConnection on thread  2
User number 14 on thread 2 has connected.
User number 9 on thread 5 is closing the connection!
concurrentThreads: 5
<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Ending handleConnection on thread 5
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Starting handleConnection on thread  5
User number 11 on thread 1 is sending message
User number 13 on thread 4 received message
User number 14 on thread 2 received message
User number 15 on thread 5 has connected.
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Starting handleConnection on thread  3
User number 12 on thread 3 has connected.
User number 13 on thread 4 is sending message
User number 15 on thread 5 received message
User number 14 on thread 2 is sending message
User number 12 on thread 3 received message
User number 15 on thread 5 is sending message
User number 12 on thread 3 is sending message
User number 15 on thread 5 is closing the connection!
concurrentThreads: 5
<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Ending handleConnection on thread 5
User number 14 on thread 2 is closing the connection!
concurrentThreads: 5
<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Ending handleConnection on thread 2
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Starting handleConnection on thread  2
User number 17 on thread 2 has connected.
User number 17 on thread 2 received message
User number 13 on thread 4 is closing the connection!
concurrentThreads: 5
<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Ending handleConnection on thread 4
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Starting handleConnection on thread  4
User number 18 on thread 4 has connected.
User number 18 on thread 4 received message
User number 17 on thread 2 is sending message
User number 18 on thread 4 is sending message
User number 11 on thread 1 is closing the connection!
concurrentThreads: 5
<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Ending handleConnection on thread 1
```

The first line prints that on thread 1, we have entered `handleConnection()`, and the last line prints that we've exited `handleConnection()` on the same thread. In those bounds, we can see other threads, sending, receiving, and closing connections as well, completely independent of thread 1's `handleConnection()`. This is enough to show me that concurrent work is definitely happening.

#### The work being done is correct
This is easy enough to verify. Given I know the contents of the text file that I am expecting, we can verify in the client code that what is returned is the exact contents. If it doesn't return it, we throw an exception.

#### The behavior of the threads as it relates to the queue
We're looking for a few things as it relates to the queue.

1. As connections come in, they are put into the queue
2. As threads free up and complete work, connections are popped out of the queue and worked on
3. The sizes of the queue increment and decrement as new work comes in and old work gets completed.

Let's look at the following snippet, modified for brevity:

```
// First NUM_THREAD users are connected and being worked on. Queue size is 0.
DESKTOP-68HHDKO connected on port 49347
pushing to queue
pushed to queue. Queue size: 1
DESKTOP-68HHDKO connected on port 49345
pushing to queue
pushed to queue. Queue size: 2
DESKTOP-68HHDKO connected on port 49348
pushing to queue
pushed to queue. Queue size: 3
...
... // Lots of connections being pushed due to the 250ms sleep in the ThreadPoolServerImproved example
...
DESKTOP-68HHDKO connected on port 49439
pushing to queue
pushed to queue. Queue size: 94
DESKTOP-68HHDKO connected on port 49440
pushing to queue
pushed to queue. Queue size: 95
concurrentThreads: 5
<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Ending handleConnection on thread 2
Popping from queue, queue size: 94
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Starting handleConnection on thread  2
User number 6 on thread 2 has connected.
User number 5 on thread 5 is closing the connection!
concurrentThreads: 5
<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Ending handleConnection on thread 5
Popping from queue, queue size: 93
...
...
...

User number 97 on thread 4 has connected.
Popping from queue, queue size: 2
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Starting handleConnection on thread  2
User number 98 on thread 2 has connected.
Popping from queue, queue size: 1
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Starting handleConnection on thread  1
User number 99 on thread 1 has connected.
User number 94 on thread 3 is closing the connection!
concurrentThreads: 5
<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Ending handleConnection on thread 3
User number 96 on thread 5 received message
User number 97 on thread 4 received message
User number 98 on thread 2 received message
Popping from queue, queue size: 0
```

We can see that that as the first 5 threads are being worked on, 95 extra connections are pushed to the queue, as expected.
Next, after popping from the queue, the size is always going down. An interesting extension here could be to sleep when sending connections, that way we could see the effect of new connections being added throughout the process.
Thus, in combination with the fact the results are as expected and proof that concurrent work is happening, we can be fairly certain that the overall behavior is correct and as intended.
