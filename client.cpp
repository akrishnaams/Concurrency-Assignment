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

#define SHM_REQUEST_NAME "/shared_memory_request"
#define NUM_CLIENT_THREADS 1

#define MAX_STRING_LEN 6

SharedMemory* sharedMemoryPtr = nullptr;
std::vector<std::thread> threads;

void cleanup(int sig) {

    sem_post(&sharedMemoryPtr->req_buffer_lock);
    sem_post(&sharedMemoryPtr->res_buffer_lock);
    sem_post(&sharedMemoryPtr->res_space_available);
    sem_post(&sharedMemoryPtr->req_available);

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

    while(true) {

        request.requestid = requestIdDist(generator);
        request.operation = static_cast<OperationType>(opTypeDist(generator));
        auto stringLength = requestStringLength(generator);
        for (size_t i = 0; i < stringLength; i++){
            request.value[i] = charDist(generator);
        }
        request.value[stringLength]='\0';

        std::cout<<"Request Created\n";

        sem_wait(&sharedMemoryPtr->req_space_available);
        sem_wait(&sharedMemoryPtr->req_buffer_lock);
        sharedMemoryPtr->request=request;
        sem_post(&sharedMemoryPtr->req_buffer_lock);
        sem_post(&sharedMemoryPtr->req_available);

        std::cout<<"Request Sent\n";

        while(true) {
            sem_wait(&sharedMemoryPtr->res_available);
            sem_wait(&sharedMemoryPtr->res_buffer_lock);
            if (sharedMemoryPtr->response.requestid == request.requestid) break;
            sem_post(&sharedMemoryPtr->res_buffer_lock);
            sem_post(&sharedMemoryPtr->res_available);
        }
        if (sharedMemoryPtr->response.returntype==SUCCESS) {
            // Processed sample;
            std::cout<<"Response Received\n";
        }
        sem_post(&sharedMemoryPtr->res_buffer_lock);
        sem_post(&sharedMemoryPtr->res_space_available);
    }

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