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
#include <sstream>

#define SHM_REQUEST_NAME "/shared_memory_request"
#define NUM_PROCESSING_THREADS 25

HashTable* tablePtr = nullptr;
SharedMemory* sharedMemoryPtr = nullptr;

sem_t req_queue_size;
sem_t req_queue_lock;
sem_t res_queue_size;
sem_t res_queue_lock;
std::queue<Request> requestQueue;
std::queue<Response> responseQueue;

void processRequests() {

    while(true) {

        sem_wait(&req_queue_size);
        sem_wait(&req_queue_lock);
        auto request = requestQueue.front();
        requestQueue.pop();
        sem_post(&req_queue_lock);

        // std::cout<<"Request Dequeued\n";

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
        else if (request.operation == DELETE) {
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

        sem_wait(&res_queue_lock);
        responseQueue.push(response);
        sem_post(&res_queue_lock);
        sem_post(&res_queue_size);

        // std::cout<<"Response queued\n";

    }
}

void enqueueRequests() {
    while(true) {
        sem_wait(&sharedMemoryPtr->req_available);
        Request request = sharedMemoryPtr->request;
        sem_post(&sharedMemoryPtr->req_space_available);

        // std::cout<<"Request Received\n";

        sem_wait(&req_queue_lock);
        requestQueue.push(request);
        sem_post(&req_queue_lock);
        sem_post(&req_queue_size);

        // std::cout<<"Request Queued\n";
    }
}

inline long long getCurrentTimeInNanoseconds() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts); // Get time since the system started (monotonic clock)
    return ts.tv_sec * 1000000000LL + ts.tv_nsec; // Convert seconds to nanoseconds and add nanoseconds
}

void dequeueResponses() {
    std::ostringstream oss;
    while(true) {
        sem_wait(&res_queue_size);
        sem_wait(&res_queue_lock);
        Response response = responseQueue.front();
        responseQueue.pop();
        sem_post(&res_queue_lock);

        // std::cout<<"Response dequeued\n";

        auto response_created_time = getCurrentTimeInNanoseconds();
        sem_wait(&sharedMemoryPtr->res_space_available);
        sharedMemoryPtr->response = response;
        sem_post(&sharedMemoryPtr->res_available);
        auto response_sent_time = getCurrentTimeInNanoseconds();

        auto time_to_send = (response_sent_time - response_created_time);
        oss << time_to_send  << "\n";
        std::cout << oss.str();
        oss.str("");

        // std::cout<<"Response sent\n";
    }
}

void cleanup(int sig) {

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

    sem_init(&sharedMemoryPtr->res_available, 1, 0); 
    sem_init(&sharedMemoryPtr->res_space_available, 1, 1); 

    sem_init(&req_queue_lock, 0, 1);
    sem_init(&res_queue_lock, 0, 1);
    sem_init(&req_queue_size, 0, 0);
    sem_init(&res_queue_size, 0, 0);

    signal(SIGINT, cleanup);

    std::vector<std::thread> threads;
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
