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
#include <sstream>

#define SHM_REQUEST_NAME "/shared_memory_request"

#define NUM_REQUESTS 400000
#define NUM_CLIENT_THREADS 25

#define MAX_STRING_LEN 6

SharedMemory* sharedMemoryPtr = nullptr;
std::vector<std::thread> threads;
std::atomic<bool> running(true);
sem_t threads_safe_exit;

void cleanup(int sig) {

    running = false;
    for (int i = 0; i < NUM_CLIENT_THREADS; i++) 
        sem_wait(&threads_safe_exit);

    munmap(sharedMemoryPtr, sizeof(SharedMemory));
    exit(0);
}

inline long long getCurrentTimeInNanoseconds() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts); // Get time since the system started (monotonic clock)
    return ts.tv_sec * 1000000000LL + ts.tv_nsec; // Convert seconds to nanoseconds and add nanoseconds
}


void sendRequestwaitResponse() {

    std::random_device rd;
    std::mt19937_64 generator(rd());
    std::uniform_int_distribution<uint64_t> requestIdDist(1, UINT64_MAX);
    std::uniform_int_distribution<uint8_t> requestStringLength(1, MAX_STRING_LEN);
    std::uniform_int_distribution<int> opTypeDist(0, 2);
    std::uniform_int_distribution<char> charDist('a', 'z');

    Request request;

    uint32_t num_requests = 0;
    std::ostringstream oss;

    while((running) && (num_requests < NUM_REQUESTS)) {

        request.requestid = requestIdDist(generator);
        request.operation = static_cast<OperationType>(opTypeDist(generator));
        auto stringLength = requestStringLength(generator);
        for (size_t i = 0; i < stringLength; i++){
            request.value[i] = charDist(generator);
        }
        request.value[stringLength]='\0';

        auto request_created_time = getCurrentTimeInNanoseconds();
        sem_wait(&sharedMemoryPtr->req_space_available);
        sharedMemoryPtr->request=request;
        sem_post(&sharedMemoryPtr->req_available);
        auto request_sent_time = getCurrentTimeInNanoseconds();

        while(true) {
            sem_wait(&sharedMemoryPtr->res_available);
            if (sharedMemoryPtr->response.requestid == request.requestid) break;
            sem_post(&sharedMemoryPtr->res_available);
        }
        sem_post(&sharedMemoryPtr->res_space_available);

        auto time_to_send = (request_sent_time - request_created_time);
        oss << time_to_send  << "\n";
        std::cout << oss.str();
        oss.str("");

        num_requests++;
    }

    sem_post(&threads_safe_exit);
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
    sem_init(&threads_safe_exit, 0, 0);
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