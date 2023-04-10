//
// Created by Omer Itach on 10/04/2023.
//
#include "uthreads.h"
#include <iostream>
#include <csetjmp>
#include "vector"
#include "map"
#include "set"



//////// ERROR ///////////
#define FAIL -1
#defing SUCCESS 0
#define INVALID_QUANTUM_SIZE "thread library error: Invalid quantum size- less than 0"
#define THREADS_OVERLOAD "system error: The Threads capacity exceeded"
#define INVALID_THREAD_ID "thread library error: Invalid Thread Id"

///////GLOBALS////////
int processQuantumCount;

// insering the available threads number
std::set<int> availableThreadsId;
for (int i = 0; i < MAX_THREAD_NUM; i++) {
availableThreadsId.insert(i);
}

// Threads vector




/**
 * Enum for the 3 optional states of thread
 */
typedef enum State{
    READY, BLOCK, RUNNING
} State;



/**
 *
 */
struct Thread{

    int threadId;
    State state;
    thread_entry_point entryPoint;
    char* threadStack;
    int sleepingCount;
    int threadQuantum;
    sigjmp_buf env;

    Thread(thread_entry_point entryPoint, int threadId){
        this.entryPoint = entryPoint;
        this.threadId= threadId;
        this->state = READY;
        this->sleepingCount = 0;
        this->threadQuantum = 0;
        this->threadStack = new char[STACK_SIZE];
        // TODO add setup function
    }

    ~Thread(){
        delete[] this->threadStack;
    }

}  ;

/////////////////////////// Threads Management //////////////////
std::map<int,Thread*> threadsVector; // curr
std::map<int, Thread*> readyThreads; // queue
std::map<int, Thread*> sleepingThreads; //sleeping

Thread* runningThread

/**
 * @brief initializes the thread library.
 *
 * Once this function returns, the main thread (tid == 0) will be set as RUNNING. There is no need to
 * provide an entry_point or to create a stack for the main thread - it will be using the "regular" stack and PC.
 * You may assume that this function is called before any other thread library function, and that it is called
 * exactly once.
 * The input to the function is the length of a quantum in micro-seconds.
 * It is an error to call this function with non-positive quantum_usecs.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_init(int quantum_usecs)
{
    if(quantum_usecs <= 0){
        std::cerr << INVALID_QUANTUM_SIZE << std::endl;
        return FAIL;
    }
    quantumCount = 0;
    //  initialize the first thread and push it to the threads vector
    int tid =  availableThreadsId.begin()
    threadsVector[tid] = new Thread(nullptr, tid);
    //fixing the set the first id
    availableThreadsId.erase(tid);

    runningThread = threadsVector[0];
    runningThread->state = RUNNING;
    runningThread->threadQuantum++;
    processQuantumCount ++;

    return EXIT_SUCCESS

    //  TODO anything else? changing the count?

}

/**
 * @brief Creates a new thread, whose entry point is the function entry_point with the signature
 * void entry_point(void).
 *
 * The thread is added to the end of the READY threads list.
 * The uthread_spawn function should fail if it would cause the number of concurrent threads to exceed the
 * limit (MAX_THREAD_NUM).
 * Each thread should be allocated with a stack of size STACK_SIZE bytes.
 * It is an error to call this function with a null entry_point.
 *
 * @return On success, return the ID of the created thread. On failure, return -1.
*/
int uthread_spawn(thread_entry_point entry_point)
{
    if (threadsVector.size() >= MAX_THREAD_NUM){
        std::cerr << THREADS_OVERLOAD << std::endl;
        return FAIL;
    }
    int avail_id = availableThreadsId.begin();
    Thread thread = new Thread(entry_point, avail_id);
    // TODO : maybe add a check if thread allocation succeed
    availableThreadsId.erase(avail_id);
    threadsVector[avail_id] = thread;
    readyThreads[avail_id] = thread;
    return avail_id;
}


/**
 * @brief Terminates the thread with ID tid and deletes it from all relevant control structures.
 *
 * All the resources allocated by the library for this thread should be released. If no thread with ID tid exists it
 * is considered an error. Terminating the main thread (tid == 0) will result in the termination of the entire
 * process using exit(0) (after releasing the assigned library memory).
 *
 * @return The function returns 0 if the thread was successfully terminated and -1 otherwise. If a thread terminates
 * itself or the main thread is terminated, the function does not return.
*/
int uthread_terminate(int tid){
    if (availableThreadsId.find(tid) != availableThreadsId.end())
    {
        std::cerr << INVALID_THREAD_ID << std::endl;
        return FAIL;
    }


}


/**
 * @brief Blocks the thread with ID tid. The thread may be resumed later using uthread_resume.
 *
 * If no thread with ID tid exists it is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision should be made. Blocking a thread in
 * BLOCKED state has no effect and is not considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_block(int tid);


/**
 * @brief Resumes a blocked thread with ID tid and moves it to the READY state.
 *
 * Resuming a thread in a RUNNING or READY state has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid);


/**
 * @brief Blocks the RUNNING thread for num_quantums quantums.
 *
 * Immediately after the RUNNING thread transitions to the BLOCKED state a scheduling decision should be made.
 * After the sleeping time is over, the thread should go back to the end of the READY queue.
 * If the thread which was just RUNNING should also be added to the READY queue, or if multiple threads wake up
 * at the same time, the order in which they're added to the end of the READY queue doesn't matter.
 * The number of quantums refers to the number of times a new quantum starts, regardless of the reason. Specifically,
 * the quantum of the thread which has made the call to uthread_sleep isn’t counted.
 * It is considered an error if the main thread (tid == 0) calls this function.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_sleep(int num_quantums)
{
    // treadVector[0].sleep = num_quantums
//treadVector[0].status = BLOCK
}


/**
 * @brief Returns the thread ID of the calling thread.
 *
 * @return The ID of the calling thread.
*/
int uthread_get_tid();


/**
 * @brief Returns the total number of quantums since the library was initialized, including the current quantum.
 *
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number should be increased by 1.
 *
 * @return The total number of quantums.
*/
int uthread_get_total_quantums();


/**
 * @brief Returns the number of quantums the thread with ID tid was in RUNNING state.
 *
 * On the first time a thread runs, the function should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state when this function is called, include
 * also the current quantum). If no thread with ID tid exists it is considered an error.
 *
 * @return On success, return the number of quantums of the thread with ID tid. On failure, return -1.
*/
int uthread_get_quantums(int tid);
