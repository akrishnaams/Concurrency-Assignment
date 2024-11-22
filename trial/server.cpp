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
#include <atomic>

#define SHM_REQUEST_NAME "/shared_memory_request"
#define NUM_PROCESSING_THREADS 4

#define WAIT_TIME 2
#define ITERATION_TIME 1
#define PROCESS_TIME 3


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
sem_t all_threads_exited;

void enqueueRequests() {

    std::ostringstream oss;
    struct timespec iter_ts, enq_ts, curr_ts;

    while(running) {

        clock_gettime(CLOCK_REALTIME, &curr_ts);
        iter_ts.tv_sec = curr_ts.tv_sec + ITERATION_TIME;
        enq_ts.tv_sec = curr_ts.tv_sec + ITERATION_TIME + WAIT_TIME;

        if (sem_timedwait(&sharedMemoryPtr->req_available, &iter_ts) < 0) {continue; };
        Request request = sharedMemoryPtr->request;
        sem_post(&sharedMemoryPtr->req_space_available);

        oss << "Request received " << request.requestid << "\n";
        std::cout << oss.str();
        oss.str("");

        if (sem_timedwait(&req_queue_lock, &enq_ts) < 0) {
            oss << "Request ignored. Not enqueued " << request.requestid << "\n";
            std::cout << oss.str();
            oss.str("");
            continue;
        }
        requestQueue.push(request);
        sem_post(&req_queue_lock);
        sem_post(&req_queue_size);

        oss << "Request Enqueued " << request.requestid << "\n";
        std::cout << oss.str();
        oss.str("");

    }

    oss << "Enqueue Reqeuest thread exited " << "\n";
    std::cout << oss.str();
    oss.str("");

    sem_post(&all_threads_exited);
    return;
}

void processRequests(int threadid) {

    std::ostringstream oss;
    struct timespec iter_ts, process_ts, curr_ts;

    while(running) {

        clock_gettime(CLOCK_REALTIME, &curr_ts);
        iter_ts.tv_sec = curr_ts.tv_sec + ITERATION_TIME;
        process_ts.tv_sec = curr_ts.tv_sec + PROCESS_TIME + ITERATION_TIME;

        bool responsiveness = true;
        if (sem_timedwait(&req_queue_size, &curr_ts) < 0) {responsiveness = false;}
        if (sem_timedwait(&req_queue_lock, &curr_ts) < 0) {
            if (responsiveness) sem_post(&req_queue_size);
            responsiveness = false;
        };
        if (!responsiveness) continue;
        auto request = requestQueue.front();
        requestQueue.pop();
        sem_post(&req_queue_lock);

        // sem_wait(&req_queue_size);
        // sem_wait(&req_queue_lock);
        // auto request = requestQueue.front();
        // requestQueue.pop();
        // sem_post(&req_queue_lock);


        oss << "Request Dequeued " << request.requestid << "\n";
        std::cout << oss.str();
        oss.str("");

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

        if (sem_timedwait(&res_queue_lock, &process_ts) < 0) {
            oss << "Response not Enqueued. Timeout " << response.requestid << "\n";
            std::cout << oss.str();
            oss.str("");
            continue;
        }
        responseQueue.push(response);
        sem_post(&res_queue_lock);
        sem_post(&res_queue_size);

        // sem_wait(&res_queue_lock);
        // responseQueue.push(response);
        // sem_post(&res_queue_lock);
        // sem_post(&res_queue_size);

        oss << "Response Enqueued " << response.requestid << "\n";
        std::cout << oss.str();
        oss.str("");

    }

    oss << "Processs thread" << threadid << "exited " << "\n";
    std::cout << oss.str();
    oss.str("");

    sem_post(&all_threads_exited);
    return;

}

void dequeueResponses() {

    std::ostringstream oss;
    struct timespec res_ts, curr_ts, iter_ts;

    while(running) {

        clock_gettime(CLOCK_REALTIME, &curr_ts);
        iter_ts.tv_sec = curr_ts.tv_sec + ITERATION_TIME;
        res_ts.tv_sec = curr_ts.tv_sec + ITERATION_TIME + WAIT_TIME;

        bool responsiveness = true;
        if (sem_timedwait(&res_queue_size, &curr_ts) < 0) {responsiveness = false;}
        if (sem_timedwait(&res_queue_lock, &curr_ts) < 0) {
            if (responsiveness) sem_post(&res_queue_size);
            responsiveness = false;
        };
        if (!responsiveness) continue;
        Response response = responseQueue.front();
        responseQueue.pop();
        sem_post(&res_queue_lock);

        // sem_wait(&res_queue_size);
        // sem_wait(&res_queue_lock);
        // Response response = responseQueue.front();
        // responseQueue.pop();
        // sem_post(&res_queue_lock);

        oss << "Response Dequeued " << response.requestid << "\n";
        std::cout << oss.str();
        oss.str("");




        if (sem_timedwait(&sharedMemoryPtr->res_space_available,&res_ts) < 0) {
            oss << "Response Space Unavailable. Forcefully making space " << response.requestid << "\n";
            std::cout << oss.str();
            oss.str("");
        }
        sharedMemoryPtr->response = response;
        sem_post(&sharedMemoryPtr->res_available);

        // sem_wait(&sharedMemoryPtr->res_space_available)
        // sharedMemoryPtr->response = response;
        // sem_post(&sharedMemoryPtr->res_available);

        oss << "Response Sent " << response.requestid << "\n";
        std::cout << oss.str();
        oss.str("");

    }

    oss << "Response Dequeue thread exited " << "\n";
    std::cout << oss.str();
    oss.str("");

    sem_post(&all_threads_exited);
    return;

}

void cleanup(int sig) {

    running = false;
    for (auto i = 0; i < NUM_PROCESSING_THREADS+2; i++)
        sem_wait(&all_threads_exited);

    for (auto& thread : threads) {
        if(thread.joinable())
            thread.join();
    }

    sem_destroy(&sharedMemoryPtr->req_available);
    sem_destroy(&sharedMemoryPtr->req_space_available);
    sem_destroy(&sharedMemoryPtr->res_available);
    sem_destroy(&sharedMemoryPtr->res_space_available);

    sem_destroy(&req_queue_lock);
    sem_destroy(&res_queue_lock);
    sem_destroy(&req_queue_size);
    sem_destroy(&res_queue_size);
    sem_destroy(&all_threads_exited);

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

    sem_init(&all_threads_exited, 0, 0);
    signal(SIGINT, cleanup);

    threads.emplace_back(&dequeueResponses);
    for (int i = 0; i < NUM_PROCESSING_THREADS; ++i) { 
        threads.emplace_back(&processRequests, i);
    }
    threads.emplace_back(&enqueueRequests);

    for (auto& thread : threads) {
        if(thread.joinable())
            thread.join();
    }

    return 0;
}
