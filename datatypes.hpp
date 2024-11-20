#ifndef DATATYPES_H
#define DATATYPES_H

#include <semaphore.h>

enum OperationType {
    INSERT,
    READ,
    DELETE
};

enum ReturnType {
    SUCCESS,
    FAILURE
};

struct Response {
    uint64_t requestid;
    ReturnType returntype;
    bool result;
};

struct Request {
    uint64_t requestid;
    OperationType operation;
    char value[256];
};

#define FIFO_DEPTH 256

struct SharedMemory {
    Request request;
    sem_t req_available;
    sem_t req_space_available;
    sem_t req_buffer_lock;

    Response response;
    sem_t res_available;
    sem_t res_space_available;
    sem_t res_buffer_lock;
};

#endif



/******
 * server:
 * 
 * sem_wait(req_available);
 * sem_wait(req_queue_lock);
 * //process queue
 * sem_post(req_queue_lock);
 * sem_post(req_space_available);
 * 
 * 
 * 
 * 
 * client
 * sem_wait(req_space_available);
 * sem_wait(req_queue_lock);
 * //process queue
 * sem_post(req_queue_lock);
 * sem_post(req_available);
 * 
 */



/******
 * client:
 * 
 * sem_wait(res_available);
 * sem_wait(res_queue_lock);
 * //process queue
 * sem_post(res_queue_lock);
 * sem_post(res_space_available);
 * 
 * 
 * 
 * 
 * server
 * sem_wait(res_space_available);
 * sem_wait(res_queue_lock);
 * //process queue
 * sem_post(res_queue_lock);
 * sem_post(res_available);
 * 
 */