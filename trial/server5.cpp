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
#define NUM_PROCESSING_THREADS 4

HashTable* tablePtr = nullptr;
SharedMemory* sharedMemoryPtr = nullptr;

sem_t req_queue_size;
sem_t req_queue_lock;
sem_t res_queue_size;
sem_t res_queue_lock;
std::queue<Request> requestQueue;
std::queue<Response> responseQueue;

void processRequests() {

    std::ostringstream oss;

    while(true) {

        sem_wait(&req_queue_size);
        sem_wait(&req_queue_lock);
        auto request = requestQueue.front();
        requestQueue.pop();
        sem_post(&req_queue_lock);

        oss << "Request Dequeued " << request.requestid << "\n";
        std::cout << oss.str();
        oss.clear();

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

        sem_wait(&res_queue_lock);
        responseQueue.push(response);
        sem_post(&res_queue_lock);
        sem_post(&res_queue_size);

        oss << "Response Enqueued " << response.requestid << "\n";
        std::cout << oss.str();
        oss.clear();

    }
}

void enqueueRequests() {

    std::ostringstream oss;

    while(true) {
        sem_wait(&sharedMemoryPtr->req_available);
        sem_wait(&sharedMemoryPtr->req_buffer_lock);
        Request request = sharedMemoryPtr->request;
        sem_post(&sharedMemoryPtr->req_buffer_lock);
        sem_post(&sharedMemoryPtr->req_space_available);

        oss << "Request received " << request.requestid << "\n";
        std::cout << oss.str();
        oss.clear();

        sem_wait(&req_queue_lock);
        requestQueue.push(request);
        sem_post(&req_queue_lock);
        sem_post(&req_queue_size);

        oss << "Request Enqueued " << request.requestid << "\n";
        std::cout << oss.str();
        oss.clear();
    }
}

void dequeueResponses() {

    std::ostringstream oss;

    while(true) {
        sem_wait(&res_queue_size);
        sem_wait(&res_queue_lock);
        Response response = responseQueue.front();
        responseQueue.pop();
        sem_post(&res_queue_lock);

        oss << "Response Dequeued " << response.requestid << "\n";
        std::cout << oss.str();
        oss.clear();

        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1;

        bool is_response_sent = true;
        if (sem_timedwait(&sharedMemoryPtr->res_space_available,&ts) < 0) {
            is_response_sent = false;
        }
        if (sem_timedwait(&sharedMemoryPtr->res_buffer_lock,&ts) < 0) {
            if (is_response_sent) {
                sem_post(&sharedMemoryPtr->res_space_available);
            }
            is_response_sent = false;
        }
        if (!is_response_sent) {
            oss << "Response NOT Sent " << response.requestid << "\n";
            std::cout << oss.str();
            oss.clear();
            continue;
        }
        sharedMemoryPtr->response = response;
        sem_post(&sharedMemoryPtr->res_buffer_lock);
        sem_post(&sharedMemoryPtr->res_available);

        oss << "Response Sent " << response.requestid << "\n";
        std::cout << oss.str();
        oss.clear();

    }
}

void cleanup(int sig) {

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
