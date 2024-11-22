# Concurrent Hash Table Server with Shared Memory
This repository contains a simple implementation of a concurrent hash table server that uses POSIX shared memory for communication with clients.

## Question:
* Server program:
     - Initialize a hash table of given size (command line)
     - Support insertion of items in the hash table
     - Hash table collisions are resolved by maintaining a linked list for each bucket/entry in the hash table
     - Supports concurrent operations (multithreading) to perform (insert, read, delete operations on the hash table)
     - Use readers-writer lock to ensure safety of concurrent operations, try to optimize the granularity
     - Communicates with the client program using shared memory buffer (POSIX shm)

* Client program:
     - Enqueue requests/operations (insert, read a bucket, delete) to the server (that will operate on the the hash table) via shared memory buffer (POSIX shm)

## How to Run
1.  **Clone the repository:**

    ```bash
    git clone [https://github.com/akrishnaams/Concurrency-Assignment.git]([https://github.com/your-username/concurrent-hash-table.git](https://github.com/akrishnaams/Concurrency-Assignment.git)
    cd Concurrency-Assignment
    ```
2.  **Compile:**
    ```bash
    make all
    ```
3.  **Run the server:**
    ```bash
    ./server <table_size> 
    ```
    Replace `<table_size>` with the desired size of the hash table
4.  **Run the client in a separate terminal:**
    ```bash
    ./client
    ```

## How does the server and client interact?

The server and client interact through a POSIX Shared Memory space of the structure
```C++
struct SharedMemory {
    Request request;
    sem_t req_available;
    sem_t req_space_available;

    Response response;
    sem_t res_available;
    sem_t res_space_available;
};
```
The semaphore elements act as signals for a successful handshake process between the server and client. A successful request-response is transferred between server and client in the following manner:
1.  Client waits until a space to write the request is available `sem_wait(req_space_available)`
2.  Once the space is available, Client writes the `request`
3.  Client then lets the server know that the request is available `sem_post(req_available)`
4.  Server waits until a request is available `sem_wait(req_available);`
5.  Server obtains the `request` and processes the `response`.
6.  Server copies the `response`, and signals that the space to write a new request is available `sem_post(req_space_available)`
`Note:` The SHM contains only one request and one response at any time.

### Client
Each thread in the client creates and sends a new request and waits until a response is received from the server. The number of threads spawned is set by the user. Once a client thread creates a request, it waits until the  `Request SHM` is available to write the request. Once the request is written into `Request SHM`, it continuously tries to access the `Response SHM` in a loop. As soon as any response is put in the `Response SHM`, the client thread checks whether the response belonds to the same request ID. If it is the response for the sent request, it copies the response and releases the `Response SHM`.

### Server
The Server consists of three different stages:
1.  Request thread: This thread obtains the request from the `Request SHM` and enqueues it to the `request queue`.
2.  Processing threads: Each processing thread dequeues the request from the `request queue`, processes it and enqueues the response to the `response queue`
3.  Response thread: Whenever a response is available in the `response queue`, it dequeues it and writes it to the `Response SHM`
The enqueue, dequeue processes of the `request queue` and `response queue` are safely synchronized using lock mechanisms.
Each bin of the hash table has separate reader-writer lock to ensure safety of concurrent operations. This enables multiple bins to be accessed at the same time by the processing threads enabling concurrency. The processing threads support INSERTION (O(1)), READ(O(n)) and REMOVE(O(n)) element operations.

Although the functionality is achieved, the current code has following issues in it:
1.  Safe exit for server is not achieved. Segmentation fault arises on SIGINT.
2.  Currently if the client tries to exit on SIGINT, it waits until all the locks are released from its end. However this is not true for server.
I approached various methods involving `sem_timedwait` instead of `sem_wait` for the safe exits of client and server, however I am facing some issue in realizing it. I will be happy to explain my approach when we meet.  





