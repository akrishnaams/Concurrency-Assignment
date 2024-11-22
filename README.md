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
I approached various methods involving `sem_timedwait` instead of `sem_wait` for the safe exits of client and server, however I am facing some issue in realizing it. I will be happy to explain my approach when we meet. I have attached my trial-codes in the `trial` repository 

# Analysis

## Bottleneck: Interaction between Server and Client threads through SHM
Server and Client interact with each other through a single Shared Memory. The client threads fight for writing the request or reading the response to/from the same single shared memory. As a result, the latency is expected to increase if the number of client threads increases. Following experiment was conducted to prove this hypothesis.

### Latency in Writing Request
To determine the latencies(ns) in writing a request, we monitor the time taken by a client thread between a request creation and the time by which it is sent. We vary the number of client threads and observe how the latency increases increasing the number of client threads.

```
Percentiles Request time NUM_THREADS = 1:
1%: 461.0   5%: 479.0    25%: 507.0   50%: 529.0   75%: 561.0    95%: 858.0   99%: 923.0

Percentiles Request time NUM_THREADS = 3:
1%: 74.0    5%: 89.0    25%: 532.0   50%: 688.0    75%: 848.0    95%: 3711.0   99%: 7818.0

Percentiles Request time NUM_THREADS = 5: 
1%: 79.0    5%: 113.0   25%: 603.0   50%: 766.0    75%: 889.0     95%: 3625.0    99%: 8191.0

Percentiles Request time NUM_THREADS = 9:
1%: 84.0    5%: 113.0    25%: 658.0    50%: 818.0    75%: 978.0   95%: 4925.0    99%: 13988.0

Percentiles Request time NUM_THREADS = 13:
1%: 88.0    5%: 118.0    25%: 784.0    50%: 1021.0    75%: 1472.0    95%: 10186.149     99%: 27780.66

Percentiles Request time NUM_THREADS = 17:
1%: 90.0    5%: 121.0    25%: 883.0    50%: 1251.0    75%: 2903.0    95%: 19724.79999999993    99%: 132374.60000000033

Percentiles Request time NUM_THREADS = 21:
1%: 88.0    5%: 117.0    25%: 1004.0    50%: 1677.0    75%: 5571.0    95%: 65426.60     99%: 256637.42

Percentiles Request time NUM_THREADS = 25:
1%: 93.0     5%: 130.0    25%: 1523.0    50%: 2508.0     75%: 12458.0    95%: 153974.25     99%: 401460.09
```

![image](https://github.com/user-attachments/assets/36ad0e11-d28c-4d7a-a59e-8a35e2e0aa93)

### Latency in Reading response
To determine the latencies(ns) in reading a response, we monitor the time taken by the server thread between a response creation and the time by which it is sent. We vary the number of client threads and observe how the latency increases increasing the number of client threads.
```

Percentiles Response time NUM_THREADS = 1:
1%: 445.0     5%: 463.0    25%: 539.0    50%: 593.0    75%: 778.0    95%: 890.0   99%: 1018.0

Percentiles Response time NUM_THREADS = 3:
1%: 478.0    5%: 554.0    25%: 1699.0    50%: 3792.0   75%: 4379.0   95%: 6549.0   99%: 8352.0

Percentiles Response time NUM_THREADS = 5:
1%: 546.0    5%: 869.0    25%: 3718.0    50%: 4558.0    75%: 6383.0   95%: 8202.0    99%: 10525.0

Percentiles Response time NUM_THREADS = 9:
1%: 562.0    5%: 740.0    25%: 4523.0    50%: 6927.0     75%: 9284.0    95%: 13968.0    99%: 159773.75

Percentiles Response time NUM_THREADS = 13:
1%: 602.0    5%: 813.0     25%: 5529.0    50%: 8848.0    75%: 12982.0    95%: 231411.95    99%: 715134.4900000009

Percentiles Response time NUM_THREADS = 17:
1%: 654.0    5%: 877.0    25%: 5050.0    50%: 9944.0    75%: 50972.0    95%: 463865.79    99%: 1026351.53

Percentiles Response time NUM_THREADS = 21:
1%: 706.0    5%: 951.0    25%: 4154.0    50%: 12199.0    75%: 89949.5    95%: 427808.29    99%: 905579.76

Percentiles Response time NUM_THREADS = 25:
1%: 791.0    5%: 1059.0    25%: 3101.0    50%: 17641.0    75%: 146127.0   95%: 671251.40    99%: 1493179.52
```
![image](https://github.com/user-attachments/assets/d745420e-00d4-4ee1-b04c-b2a1aa6f332d)
From the above results, we can safely conclude that increasing the number of client threads can become a bottleneck to the system. Increasing the number of client threads also increases tail latency drastically thereby affecting the quality of service offered. 

We can also observe that latency in reading response is quite high compared to writing the request. This is because, once a response is put into the SHM, each client thread has to check whether the written response is the response to the request it sent. This results in a polling behavior among all client threads, until the response is read, increasing the reading response latency drastically 




