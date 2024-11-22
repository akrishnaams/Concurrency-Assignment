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
#define NUM_CLIENT_THREADS 4

#define MAX_STRING_LEN 6

#define REQUEST_WAIT_TIME  10       // If the request is not accepted within this time limit
#define ITERATION_TIME     2      // Client should get access to the response buffer every ITERATION_TIME preferably to check whether the response has arrived or not.
#define RESPONSE_WAIT_TIME   90     // Once the request is accepted, if the response is not given within this time limit

//LATENCY = REQUEST_WAIT_TIME + RESPONSE_WAIT_TIME

SharedMemory* sharedMemoryPtr = nullptr;
std::vector<std::thread> threads;

std::atomic<bool> running(true);
sem_t num_threads_exited;

void cleanup(int sig) {

    running = false;

    for (auto i = 0; i < NUM_CLIENT_THREADS; i++)
        sem_wait(&num_threads_exited);

    for (auto& thread : threads) {
        if(thread.joinable())
            thread.join();
    }

    munmap(sharedMemoryPtr, sizeof(SharedMemory));
    exit(0);
}

//TODO: Do we need req_buffer_lock and res_buffer_lock?

void sendRequestwaitResponse(int threadid) {

    std::random_device rd;
    std::mt19937_64 generator(rd());
    // std::uniform_int_distribution<uint64_t> requestIdDist(1, UINT64_MAX);
    std::uniform_int_distribution<uint64_t> requestIdDist(1, UINT16_MAX);
    std::uniform_int_distribution<uint8_t> requestStringLength(1, MAX_STRING_LEN);
    std::uniform_int_distribution<int> opTypeDist(0, 2);
    std::uniform_int_distribution<char> charDist('a', 'z');

    Request request;

    std::ostringstream oss;

    struct timespec req_ts, res_ts, curr_ts, iter_ts;

    while(running) {

        request.requestid = requestIdDist(generator);
        request.operation = static_cast<OperationType>(opTypeDist(generator));
        auto stringLength = requestStringLength(generator);
        for (size_t i = 0; i < stringLength; i++){
            request.value[i] = charDist(generator);
        }
        request.value[stringLength]='\0';

        // oss << threadid << " Request Created " << request.requestid << "\n";
        // std::cout << oss.str();
        // oss.str("");

        clock_gettime(CLOCK_REALTIME, &curr_ts);
        req_ts.tv_sec = curr_ts.tv_sec + REQUEST_WAIT_TIME;
        res_ts.tv_sec = curr_ts.tv_sec + REQUEST_WAIT_TIME + RESPONSE_WAIT_TIME;

        bool is_request_sent = true;
        if (sem_timedwait(&sharedMemoryPtr->req_space_available, &req_ts) < 0) {
            is_request_sent = false;
        }
        if (sem_timedwait(&sharedMemoryPtr->req_buffer_lock, &req_ts) < 0) {
            if (is_request_sent) sem_post(&sharedMemoryPtr->req_space_available);
            is_request_sent = false;
        }
        if (!is_request_sent) {
            oss << threadid  << " Request NOT Sent " << request.requestid << "\n";
            std::cout << oss.str();
            oss.str("");
            continue;
        }
        sharedMemoryPtr->request=request;
        sem_post(&sharedMemoryPtr->req_buffer_lock);
        sem_post(&sharedMemoryPtr->req_available);

        oss << threadid  << " Request Sent " << request.requestid << "\n";
        std::cout << oss.str();
        oss.str("");

        bool is_response_rcvd = true;

        while(running) {

            // clock_gettime(CLOCK_REALTIME, &curr_ts);
            // if (curr_ts.tv_sec > res_ts.tv_sec) {is_response_rcvd = false; break;}

            // bool responsiveness = true;
            // iter_ts.tv_sec = curr_ts.tv_sec + ITERATION_TIME; 

            // if (sem_timedwait(&sharedMemoryPtr->res_available, &iter_ts) < 0) {
            //     responsiveness = false;
            // }
            // if (sem_timedwait(&sharedMemoryPtr->res_buffer_lock, &iter_ts) < 0) {
            //     if (responsiveness) {
            //         // oss << threadid  << " Response Lock res_available locked!" << "\n";
            //         // std::cout << oss.str();
            //         // oss.str("");
            //         sem_post(&sharedMemoryPtr->res_available);
            //         // oss << threadid  << " Response Lock res_available released!" << "\n";
            //         // std::cout << oss.str();
            //         // oss.str("");
            //     } 
            //     responsiveness = false;
            // }
            // if (!responsiveness) {
            //     oss << threadid  << " Response Iteration. Timeout Warning!" << "\n";
            //     std::cout << oss.str();
            //     oss.str("");
            //     continue;
            // }

            // oss << threadid  << " Response Lock res_buffer_lock res_available locked!" << "\n";
            // std::cout << oss.str();
            // oss.str("");

            sem_wait(&sharedMemoryPtr->res_available);
            sem_wait(&sharedMemoryPtr->res_buffer_lock);
            if (sharedMemoryPtr->response.requestid == request.requestid) { is_response_rcvd = true; break;}
            sem_post(&sharedMemoryPtr->res_buffer_lock);
            sem_post(&sharedMemoryPtr->res_available);

            // oss << threadid  << " Response Lock res_buffer_lock res_available released!" << "\n";
            // std::cout << oss.str();
            // oss.str("");

        }
        if (!running) { break; }
        if (!is_response_rcvd) {
            oss << threadid  << " Response NOT Received. Timeout Error! " << request.requestid << "\n";
            std::cout << oss.str();
            oss.str("");
            continue;
        }
        sem_post(&sharedMemoryPtr->res_buffer_lock);
        sem_post(&sharedMemoryPtr->res_space_available);
        oss << threadid  << " Response Received " << request.requestid << "\n";
        std::cout << oss.str();
        oss.str("");

        // oss << threadid  << " Response Lock res_buffer_lock res_space_available released!" << "\n";
        // std::cout << oss.str();
        // oss.str("");

    }

    sem_post(&num_threads_exited);
    oss << threadid  << " Exited Process\n";
    std::cout << oss.str();
    oss.str("");
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
    signal(SIGINT, cleanup);

    for (int i = 0; i < NUM_CLIENT_THREADS; ++i) { 
        threads.emplace_back(&sendRequestwaitResponse, i);
    }

    for (auto& thread : threads) {
        if(thread.joinable())
            thread.join();
    }

    return 0;

}