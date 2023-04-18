//
// Created by Omer Itach on 10/04/2023.
//
#include "uthreads.h"
#include <iostream>
#include <csetjmp>
#include "vector"
#include "map"
#include "set"
#include <sys/time.h>
#include <signal.h>
#include <algorithm>



//////// ERROR ///////////
#define FAIL -1
#define SUCCESS 0
#define INVALID_QUANTUM_SIZE "thread library error: Invalid quantum size- less than 0"
#define THREADS_OVERLOAD "system error: The Threads capacity exceeded"
#define INVALID_THREAD_ID "thread library error:  Thread Id is not in the valid range (0 to MAX_THREAD) "
#define THREAD_ID_NOT_IN_USE "thread library error:  Thread Id is not in use "
#define INVALID_THREAD_BLOCK_ITSELF "thread library error: Invalid Thread Id- The Thread is already blocked"
#define INVALID_THREAD_ID_RESUME_RUNNING "thread library error: Invalid Thread Id- The Thread is already running"
#define INVALID_THREAD_ID_RESUME_READY "thread library error: Invalid Thread Id- The Thread is already ready"
#define INVALID_QUANTUM_NUM "thread library error: Invalid Quantum size for blocking the thread "
#define RUNNING_THREAD_IS_NULL "system error: There is no running thread currently "
#define NOT_YET_INITIALIZE_THREADS "system error: First Initialize threads"
#define SET_TIMER_FAIL "set timer failed"
#define SIGNAL_SET_ERROR "system error: failed to create signal set"
#define SIGNAL_ACTION_ERROR "system error: failed to action signal "



//////////////// Additional Signals ///////////////
#define SIG_TERMINATE (SIGVTALRM + 1)
#define SIG_SLEEP (SIGVTALRM + 2)
#define SIG_BLOCKED_HANDLE (SIGVTALRM + 3)

///////////// thread setup helpers/////////////
typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7
 address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
                 "rol    $0x9,%0\n"
            : "=g" (ret)
            : "0" (addr));
    return ret;
}
///////////////////////////////////////

///////GLOBALS/////////////////////
int processQuantumCount = 0;
// insering the available threads number
std::set<int> availableThreadsId;
////////////////////////////////////

//////////////////Schedule VARS /////////////////
struct sigaction sa = {0};
struct itimerval timer;
sigset_t sigSet;
// MASKS
#define MASK_ACTIVATE sigprocmask(SIG_BLOCK, &sigSet, nullptr);
#define MASK_DEACTIVATE sigprocmask(SIG_UNBLOCK, &sigSet, nullptr);




/**
 * Enum for the 3 optional states of thread
 */
typedef enum State{
    READY, BLOCKED, RUNNING
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
        this->entryPoint = entryPoint;
        this->threadId= threadId;
        this->state = READY;
        this->sleepingCount = 0;
        this->threadQuantum = 0;
        this->threadStack = new char[STACK_SIZE];
        setup_thread(this->entryPoint);
        // TODO add setup function
    }

    bool isSleep(){
        return sleepingCount > 0;
    }

    ~Thread(){
        delete[] this->threadStack;
    }

    void setup_thread( thread_entry_point entry_point)
    {
        sigsetjmp(env, 1);
        address_t sp = (address_t) threadStack + STACK_SIZE - sizeof(address_t);
        address_t pc = (address_t) entry_point;
        (env->__jmpbuf)[JB_SP] = translate_address(sp);
        (env->__jmpbuf)[JB_PC] = translate_address(pc);
        sigemptyset(&(env->__saved_mask));
    }
}  ;

/////////////////////////// Threads Management //////////////////
std::map<int,Thread*> threadsMap; // curr
std::vector<Thread*> readyThreads; // queue
std::map<int, Thread*> sleepingThreads; //sleeping
Thread* runningThread;
////////////////////////////////////////////////////////////////////

////////////////////// HELPERS ////////////////////
bool inValid(int tid){

    if(tid < 0  || tid >= MAX_THREAD_NUM){
        std::cerr << INVALID_THREAD_ID << std::endl;
        return true;
    }
    if (availableThreadsId.find(tid) != availableThreadsId.end())
    {
        std::cerr << THREAD_ID_NOT_IN_USE << std::endl;
        return true;
    }
    return false;
}


void setAvailableIndices(){
    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        availableThreadsId.insert(i);
    }
}

void freeAll(){

    for( auto it = threadsMap.begin();  it != threadsMap.end(); it ++)
    {
        delete it->second;
    }
    threadsMap.clear();
    sleepingThreads.clear();
    readyThreads.clear();
    availableThreadsId.clear();
    runningThread = nullptr;
}


/////////////////SCHEDULE////////////////////////////////


void deleteThread(int tid){
    Thread * thread = threadsMap[tid];
    if(thread->state == READY){
        auto pos = std::find(readyThreads.begin(), readyThreads.end(), thread);
        readyThreads.erase(pos);
    }

    if(thread->sleepingCount > 0){
        sleepingThreads.erase(tid);
    }

    //removing from map
    threadsMap.erase(tid);
    // delete the boject
    delete thread;
    //adding to available id's
    availableThreadsId.insert(tid);
}

void runTimer() {
    if (setitimer(ITIMER_VIRTUAL, &timer, NULL)) {

        std::cerr << SET_TIMER_FAIL << std::endl;
        freeAll();
        exit(EXIT_FAILURE);
    }
}
Thread* switchThreadsByQuantum(){
    // handeling sleeping threads
    for(auto it= sleepingThreads.begin(); it!= sleepingThreads.end(); it++) {
        it->second->sleepingCount--;
        if (it->second->sleepingCount == 0) {
            sleepingThreads.erase(it->first);
            if (it->second->state == READY) {
                readyThreads.push_back(it->second);
            }
        }
    }
// initialize the first thread from the ready map to be the running
    Thread * oldRunningThread = runningThread;
    runningThread = readyThreads.front();
    runningThread->state = RUNNING;
    runningThread->threadQuantum ++;
    readyThreads.erase(readyThreads.begin());
    processQuantumCount++;
    return oldRunningThread;
}

void setLongThreads(Thread * curThread, Thread * nextThread, bool setRequire){
    if (setRequire){
        sigsetjmp(curThread->env, 1);
    }
    runTimer();
    siglongjmp(nextThread->env, 1);

}




/**
 *
 * @param quantum
 */
void initTimer(int quantum) {

    // init for the first interval of the timer, before start running
    timer.it_value.tv_sec = quantum / 1000000;        // first time interval, seconds part
    timer.it_value.tv_usec = quantum % 1000000;        // first time interval, microseconds part

    timer.it_interval.tv_sec = quantum / 1000000;;    // following time intervals, seconds part
    timer.it_interval.tv_usec = quantum % 1000000;;    // following time intervals, microseconds part
    runTimer();
}

void signalHandler(int sig){
    Thread* oldRunning =  switchThreadsByQuantum();

    if (sig==SIGVTALRM){
        // running thread was update in switchThreads
        oldRunning->state = READY;
        readyThreads.push_back(oldRunning);
    }

    if(sig == SIG_SLEEP){
        oldRunning->state = READY;
        sleepingThreads[oldRunning->threadId] = oldRunning;
    }

    if(sig == SIG_TERMINATE){
        deleteThread(oldRunning->threadId);
        setLongThreads(nullptr, runningThread, false);
    }

    else{
        setLongThreads(oldRunning, runningThread, true);

    }


    // no need to handel anymore with block or to handle with terminate as well

}


void initScheduler(int quantum){

    sigemptyset(&sigSet);
    sigaddset(&sigSet, SIGVTALRM);
// timer initialization
    sa.sa_handler = &signalHandler;
    if(sigaction(SIGVTALRM, &sa, nullptr) < 0) {
        std::cerr << SIGNAL_ACTION_ERROR  << std::endl;
        freeAll();
        exit(EXIT_FAILURE);
    }
    initTimer(quantum);


}


/////////////////////////////////////////////////////////



///////////////////////////FUNCTION/////////////////////////
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

    setAvailableIndices(); // set all the available
//    processQuantumCount = 0; // should increase to 1 in the scheduler - first time iteration
    //  initialize the first thread and push it to the threads vector
    int tid =  *(availableThreadsId.begin());
    threadsMap[tid] = new Thread(nullptr, tid);
    //fixing the set the first id
    availableThreadsId.erase(tid);

    runningThread = threadsMap[0];
    runningThread->state = RUNNING;
    runningThread->threadQuantum++;

    processQuantumCount = 1;

    return EXIT_SUCCESS;

    //  TODO anyhing else? hanging the count?
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
    MASK_ACTIVATE;
    if (threadsMap.size() >= MAX_THREAD_NUM){
        std::cerr << THREADS_OVERLOAD << std::endl;
        return FAIL;
    }
    int avail_id = *(availableThreadsId.begin());
    Thread* thread = new Thread(entry_point, avail_id);
    // TODO : maybe add a check if thread allocation succeed
    availableThreadsId.erase(avail_id);
    threadsMap[avail_id] = thread;
    readyThreads.push_back(thread);
    MASK_DEACTIVATE;
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
    // if the given id is in the available means no anle to kill it

  MASK_ACTIVATE;
    if (inValid(tid))
    {
        return FAIL;
    }

    if(tid == 0)
    {
        freeAll();
        exit(EXIT_SUCCESS);
    }

    deleteThread(tid);
    MASK_DEACTIVATE;
    return EXIT_SUCCESS;

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
int uthread_block(int tid){
    MASK_ACTIVATE;
    if (inValid(tid))
    {
        return FAIL;
    }
    Thread* thread = threadsMap[tid];

    if(thread->state == BLOCKED){
        std::cerr << INVALID_THREAD_BLOCK_ITSELF << std::endl;
        return FAIL;
    }

    if(thread->threadId == runningThread->threadId)
    {
        thread->state = BLOCKED;
        signalHandler(SIG_BLOCKED_HANDLE);
    }
    else{
        //  If a thread blocks itself, a scheduling decision should be made handel that case THe same case
        //  as- handel if we block the running

        // not sleeping and ready
        if(thread->state == READY && !thread->isSleep()){
            auto pos = std::find(readyThreads.begin(), readyThreads.end(), thread);
            readyThreads.erase(pos);
        }

        // ready and sleeping
        thread->state = BLOCKED;

    }
    MASK_DEACTIVATE;
    return EXIT_SUCCESS;

}

/**
 * @brief Resumes a blocked thread with ID tid and moves it to the READY state.
 *
 * Resuming a thread in a RUNNING or READY state has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid){

    if (inValid(tid))
    {
        return FAIL;
    }

    Thread* thread = threadsMap[tid];

    if(thread->state == READY){
        std::cerr << INVALID_THREAD_ID_RESUME_READY << std::endl;
        return FAIL;
    }
    if(thread->state == RUNNING){
        std::cerr << INVALID_THREAD_ID_RESUME_RUNNING << std::endl;
        return FAIL;
    }
    if(thread->state == BLOCKED){
        if(thread->sleepingCount == 0){
            readyThreads.push_back(thread);
        }
        thread->state= READY;
    }

    return EXIT_SUCCESS;

}


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
    MASK_ACTIVATE;
  if(num_quantums <= 0){
      std::cerr << INVALID_QUANTUM_SIZE << std::endl;
      return FAIL;
  }

  // updating the running thread sleeping time and updating it
  runningThread->sleepingCount = num_quantums;

  signalHandler(SIG_SLEEP);

  MASK_DEACTIVATE;
  return EXIT_SUCCESS;


}


/**
 * @brief Returns the thread ID of the calling thread.
 *
 * @return The ID of the calling thread.
*/
int uthread_get_tid(){
    if(runningThread == nullptr){
        std::cerr << RUNNING_THREAD_IS_NULL << std::endl;
        return FAIL;
    }
    return runningThread->threadId;
}


/**
 * @brief Returns the total number of quantums since the library was initialized, including the current quantum.
 *
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number should be increased by 1.
 *
 * @return The total number of quantums.
*/
int uthread_get_total_quantums()
{
    if(runningThread == 0){
        std::cerr << NOT_YET_INITIALIZE_THREADS << std::endl;
        return FAIL;
    }
    return processQuantumCount;
}


/**
 * @brief Returns the number of quantums the thread with ID tid was in RUNNING state.
 *
 * On the first time a thread runs, the function should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state when this function is called, include
 * also the current quantum). If no thread with ID tid exists it is considered an error.
 *
 * @return On success, return the number of quantums of the thread with ID tid. On failure, return -1.
*/
int uthread_get_quantums(int tid){
    if(inValid(tid))
    {
        return FAIL;
    }
    return threadsMap[tid]->threadQuantum;
}
