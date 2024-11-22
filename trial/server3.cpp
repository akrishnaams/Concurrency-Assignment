#include <iostream>
#include <list>
#include <vector>
#include <thread>
#include <cstring>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "hash.cpp"
#include "datatypes.hpp"
#include <semaphore.h>
#include <queue>
#include <csignal>
#include <fcntl.h> 
#include <ctime>
#include <atomic>

#define SHM_REQUEST_NAME "/shared_memory_request"
#define NUM_PROCESSING_THREADS 4

#define WAIT_TIME 1
#define ENQUEUE_TIME 1
#define PROCESS_TIME 3
#define RESPONSE_WAIT_TIME 1

HashTable* tablePtr = nullptr;
SharedMemory* sharedMemoryPtr = nullptr;

sem_t req_queue_size;
sem_t req_queue_lock;
sem_t res_queue_size;
sem_t res_queue_lock;
std::queue<Request> requestQueue;
std::queue<Response> responseQueue;

std::vector<std::thread> threads;
std::atomic<bool> running(true);
sem_t all_threads_safe_exit;

void processRequests() {

    while(running) {

        struct timespec wait_ts, process_ts;
        clock_gettime(CLOCK_REALTIME, &wait_ts);
        wait_ts.tv_sec += WAIT_TIME;
        process_ts.tv_sec = wait_ts.tv_sec + PROCESS_TIME;

        // sem_wait(&req_queue_size);
        // sem_wait(&req_queue_lock);
        if (sem_timedwait(&req_queue_size, &wait_ts) < 0) continue;
        if (sem_timedwait(&req_queue_lock, &wait_ts) < 0) continue;
        auto request = requestQueue.front();
        requestQueue.pop();
        sem_post(&req_queue_lock);

        std::cout<<"Request Dequeued\n";

        std::string input_string(request.value);
        Response response;
        if (request.operation == INSERT) {
            tablePtr->insert(input_string);
            response.requestid = request.requestid;
            response.returntype = SUCCESS;
            response.result = true;
        } 
        else if (request.operation == READ) {
            response.requestid = request.requestid;
            response.returntype = SUCCESS;
            response.result = tablePtr->read(input_string);
        } 
        else if (request.operation == INSERT) {
            tablePtr->remove(input_string);
            response.requestid = request.requestid;
            response.returntype = SUCCESS;
            response.result = true;
        } 
        else {
            response.requestid = request.requestid;
            response.returntype = FAILURE;
            response.result = false;
        } 

        // sem_wait(&res_queue_lock);
        if (sem_timedwait(&res_queue_lock,&process_ts) < 0) continue;
        responseQueue.push(response);
        sem_post(&res_queue_lock);
        sem_post(&res_queue_size);

        std::cout<<"Response queued\n";
    }

    std::cout<<"Processing Exited\n";
    sem_post(&all_threads_safe_exit);
    return;
}

void enqueueRequests() {

    while(running) {

        struct timespec wait_ts, enq_ts;
        clock_gettime(CLOCK_REALTIME, &wait_ts);
        wait_ts.tv_sec += WAIT_TIME;
        enq_ts.tv_sec = wait_ts.tv_sec + ENQUEUE_TIME;

        if (sem_timedwait(&sharedMemoryPtr->req_available,&wait_ts) < 0) continue;
        if (sem_timedwait(&sharedMemoryPtr->req_buffer_lock,&wait_ts) < 0) continue;
        // sem_wait(&sharedMemoryPtr->req_available);
        // sem_wait(&sharedMemoryPtr->req_buffer_lock);
        Request request = sharedMemoryPtr->request;
        sem_post(&sharedMemoryPtr->req_buffer_lock);
        sem_post(&sharedMemoryPtr->req_space_available);

        std::cout<<"Request Received\n";

        if (sem_timedwait(&req_queue_lock,&enq_ts) < 0) continue;
        // sem_wait(&req_queue_lock);
        requestQueue.push(request);
        sem_post(&req_queue_lock);
        sem_post(&req_queue_size);

        std::cout<<"Request Queued\n";
    }

    std::cout<<"Request Exited\n";
    sem_post(&all_threads_safe_exit);
    return;

}

void dequeueResponses() {

    while(running) {

        struct timespec wait_ts;
        clock_gettime(CLOCK_REALTIME, &wait_ts);
        wait_ts.tv_sec += WAIT_TIME;

        if (sem_timedwait(&res_queue_size, &wait_ts) < 0) continue;
        if (sem_timedwait(&res_queue_lock, &wait_ts) < 0) continue;
        // sem_wait(&res_queue_size);
        // sem_wait(&res_queue_lock);
        Response response = responseQueue.front();
        responseQueue.pop();
        sem_post(&res_queue_lock);

        std::cout<<"Response dequeued\n";

        struct timespec res_recv_ts;
        clock_gettime(CLOCK_REALTIME, &res_recv_ts);
        res_recv_ts.tv_sec += RESPONSE_WAIT_TIME;
        bool responsiveness = true;

        if (sem_timedwait(&sharedMemoryPtr->res_space_available,&res_recv_ts) < 0) { responsiveness = false; }
        if (sem_timedwait(&sharedMemoryPtr->res_buffer_lock,&res_recv_ts) < 0) { responsiveness = false; };
        if (!responsiveness) {std::cout<<"Response ready. But cannot be sent\n"; continue; }
        // sem_timedwait(&sharedMemoryPtr->res_space_available,&res_recv_ts);
        // sem_timedwait(&sharedMemoryPtr->res_buffer_lock,&res_recv_ts);
        sharedMemoryPtr->response = response;
        sem_post(&sharedMemoryPtr->res_buffer_lock);
        sem_post(&sharedMemoryPtr->res_available);

        std::cout<<"Response sent\n";
    }
    
    std::cout<<"Response Exited\n";
    sem_post(&all_threads_safe_exit);
    return;
}

void cleanup(int sig) {

    running = false;
    for (auto i = 0; i < NUM_PROCESSING_THREADS+2; i++)
        sem_wait(&all_threads_safe_exit);

    for (auto& thread : threads) {
        if(thread.joinable())
            thread.join();
    }

    sem_destroy(&sharedMemoryPtr->req_available);
    sem_destroy(&sharedMemoryPtr->req_space_available);
    sem_destroy(&sharedMemoryPtr->req_buffer_lock);
    sem_destroy(&sharedMemoryPtr->res_available);
    sem_destroy(&sharedMemoryPtr->res_space_available);
    sem_destroy(&sharedMemoryPtr->res_buffer_lock);

    munmap(sharedMemoryPtr, sizeof(SharedMemory));
    shm_unlink(SHM_REQUEST_NAME);

    delete tablePtr;
    exit(0);
}

int main(int argc, char* argv[]) {

    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <table_size>" << std::endl;
        return 1;
    }
    int tableSize = std::stoi(argv[1]);
    tablePtr = new HashTable(tableSize);

    int shm_fd = shm_open(SHM_REQUEST_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }
    ftruncate(shm_fd, sizeof(SharedMemory));

    void* shm_ptr = mmap(NULL, sizeof(SharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    sharedMemoryPtr = (SharedMemory*)shm_ptr;

    sem_init(&sharedMemoryPtr->req_available, 1, 0); 
    sem_init(&sharedMemoryPtr->req_space_available, 1, 1); 
    sem_init(&sharedMemoryPtr->req_buffer_lock, 1, 1); 

    sem_init(&sharedMemoryPtr->res_available, 1, 0); 
    sem_init(&sharedMemoryPtr->res_space_available, 1, 1); 
    sem_init(&sharedMemoryPtr->res_buffer_lock, 1, 1);

    sem_init(&req_queue_lock, 0, 1);
    sem_init(&res_queue_lock, 0, 1);
    sem_init(&req_queue_size, 0, 0);
    sem_init(&res_queue_size, 0, 0);

    signal(SIGINT, cleanup);

    threads.emplace_back(&dequeueResponses);
    for (int i = 0; i < NUM_PROCESSING_THREADS; ++i) { 
        threads.emplace_back(&processRequests);
    }
    threads.emplace_back(&enqueueRequests);

    for (auto& thread : threads) {
        if(thread.joinable())
            thread.join();
    }

    return 0;
}
