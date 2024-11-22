#include <iostream>
#include <cstring>
#include <unistd.h>
#include <random>
#include <thread>
#include <sys/mman.h>
#include <sys/stat.h>
#include "hash.cpp"
#include "datatypes.hpp"
#include <semaphore.h>
#include <csignal>
#include <fcntl.h> 
#include <atomic>
#include <ctime>

#define SHM_REQUEST_NAME "/shared_memory_request"
#define NUM_CLIENT_THREADS 16

#define MAX_STRING_LEN 6

#define REQUEST_WAIT_TIME 5
#define WAIT_TIME 1
#define RESPONSE_WAIT_TIME 300

SharedMemory* sharedMemoryPtr = nullptr;
std::vector<std::thread> threads;

std::atomic<bool> running(true);
sem_t all_threads_safe_exit;

void cleanup(int sig) {

    running = false;

    for (auto i = 0; i < NUM_CLIENT_THREADS; i++)
        sem_wait(&all_threads_safe_exit);

    for (auto& thread : threads) {
        if(thread.joinable())
            thread.join();
    }

    munmap(sharedMemoryPtr, sizeof(SharedMemory));
    exit(0);
}

//TODO: Do we need req_buffer_lock and res_buffer_lock?

void sendRequestwaitResponse() {

    std::random_device rd;
    std::mt19937_64 generator(rd());
    std::uniform_int_distribution<uint64_t> requestIdDist(1, UINT64_MAX);
    std::uniform_int_distribution<uint8_t> requestStringLength(1, MAX_STRING_LEN);
    std::uniform_int_distribution<int> opTypeDist(0, 2);
    std::uniform_int_distribution<char> charDist('a', 'z');

    Request request;

    while(running) {

        request.requestid = requestIdDist(generator);
        request.operation = static_cast<OperationType>(opTypeDist(generator));
        auto stringLength = requestStringLength(generator);
        for (size_t i = 0; i < stringLength; i++){
            request.value[i] = charDist(generator);
        }
        request.value[stringLength]='\0';

        std::cout<<"Request Created\n";

        struct timespec req_ts, res_ts;
        clock_gettime(CLOCK_REALTIME, &req_ts);
        req_ts.tv_sec += REQUEST_WAIT_TIME;
        res_ts.tv_sec = req_ts.tv_sec + RESPONSE_WAIT_TIME;

        if (sem_timedwait(&sharedMemoryPtr->req_space_available,&req_ts) < 0) continue;
        if (sem_timedwait(&sharedMemoryPtr->req_buffer_lock,&req_ts) < 0) continue;
        // sem_wait(&sharedMemoryPtr->req_space_available);
        // sem_wait(&sharedMemoryPtr->req_buffer_lock);
        sharedMemoryPtr->request=request;
        sem_post(&sharedMemoryPtr->req_buffer_lock);
        sem_post(&sharedMemoryPtr->req_available);

        std::cout<<"Request Sent\n";

        bool responsiveness = true;

        while(running) {

            struct timespec wait_ts, curr_ts;
            clock_gettime(CLOCK_REALTIME, &curr_ts);
            if (curr_ts.tv_sec > res_ts.tv_sec) {responsiveness = false; break;}

            wait_ts.tv_sec = curr_ts.tv_sec + WAIT_TIME;
            if (sem_timedwait(&sharedMemoryPtr->res_available,&wait_ts) < 0) { continue; }
            if (sem_timedwait(&sharedMemoryPtr->res_buffer_lock,&wait_ts) < 0) { continue; }
            // sem_wait(&sharedMemoryPtr->res_available);
            // sem_wait(&sharedMemoryPtr->res_buffer_lock);
            
            if (sharedMemoryPtr->response.requestid == request.requestid) {responsiveness = true; break; }
            
            sem_post(&sharedMemoryPtr->res_buffer_lock);
            sem_post(&sharedMemoryPtr->res_available);
        }
        if (!running) { break; }
        if (!responsiveness) {
            std::cout<<"Response NOT Received. Timeout\n";            
            continue;
        }
        std::cout<<"Response Received\n";
        sem_post(&sharedMemoryPtr->res_buffer_lock);
        sem_post(&sharedMemoryPtr->res_space_available);
    }
    sem_post(&all_threads_safe_exit);
    return;

}

int main(int argc, char* argv[]) {

    int shm_fd = shm_open(SHM_REQUEST_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }

    void* shm_ptr = mmap(NULL, sizeof(SharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    sharedMemoryPtr = (SharedMemory*)shm_ptr;
    sem_init(&all_threads_safe_exit, 0, 0);
    signal(SIGINT, cleanup);

    for (int i = 0; i < NUM_CLIENT_THREADS; ++i) { 
        threads.emplace_back(&sendRequestwaitResponse);
    }

    for (auto& thread : threads) {
        if(thread.joinable())
            thread.join();
    }

    return 0;

}