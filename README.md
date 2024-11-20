# Concurrent Hash Table Server with Shared Memory

This repository contains a simple implementation of a concurrent hash table server that uses shared memory for communication with clients.

## Features

*   Hash table with separate chaining for collision resolution.
*   Concurrent operations (insert, read, delete) using multithreading.
*   Readers-writer lock for efficient and safe data access.
*   POSIX shared memory for inter-process communication.

## How to Run

1.  **Clone the repository:**

    ```bash
    git clone [https://github.com/your-username/concurrent-hash-table.git](https://github.com/your-username/concurrent-hash-table.git)
    cd concurrent-hash-table
    ```

2.  **Compile:**

    ```bash
    make
    ```

3.  **Run the server:**

    ```bash
    ./server <table_size> <shm_key>
    ```

    Replace `<table_size>` with the desired size of the hash table and `<shm_key>` with a unique integer key for the shared memory segment.

4.  **Run the client (in a separate terminal):**

    ```bash
    ./client <shm_key>
    ```

    Use the same `<shm_key>` as used for the server.

## Analysis of Results

To analyze the performance and behavior of this system, you can consider the following:

*   **Vary the number of client requests:** Observe how the server's throughput (number of requests processed per second) changes as you increase the number of client requests. This will help you understand the server's concurrency handling capabilities.
*   **Measure response times:**  Track the time it takes for the server to respond to client requests, especially read requests. This will give you insights into the efficiency of the hash table and the locking mechanism.
*   **Experiment with different table sizes:** Try different hash table sizes to see how it affects performance. A larger table size might reduce collisions but increase memory usage.
*   **Monitor CPU and memory usage:** Observe the server's resource utilization (CPU and memory) under different workloads to identify potential bottlenecks.

By analyzing these factors, you can gain a better understanding of the system's performance characteristics and identify areas for optimization. You can also experiment with different synchronization primitives (e.g., condition variables instead of busy-waiting) and compare their impact on performance.