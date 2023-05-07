#include "MapReduceFramework.h"
#include <atomic>
#include "pthread.h"
#include "Barrier.h"
#include <algorithm>

///////

#define PTHREAD_CREATE_ERROR "unable to create a thread"
#define FAILURE 1
#define MUTEX_LOCK_FAIL "unable to lock/unlock mutex"




struct JobContext {
    pthread_t * threads;
    JobState  jobState;
    std::atomic<int> atomic_counter;
    ThreadContext t_context;
    InputVec* inputVec;
    OutputVec * outputVec;
    MapReduceClient * client;
    pthread_mutex_t map_mutex;
    Barrier* barrier;
};

struct ThreadContext{
    int tid;
    std::atomic<int>* atomic_counter;
    IntermediateVec* intermediate_vector;
    JobContext* jobContext;

};

void emit2 (K2* key, V2* value, void* context);
void emit3 (K3* key, V3* value, void* context);

void start_map_stage(JobContext* jobContext){
    // mutex lock
    if(pthread_mutex_lock(&jobContext->map_mutex)!=0){
        std::cerr << MUTEX_LOCK_FAIL << std::endl;
        exit(FAILURE);
    };
    //get counter value
    if (jobContext->jobState.stage == UNDEFINED_STAGE){
        jobContext->jobState.stage = MAP_STAGE;

    }
    //counter++

    // mutex unlock
    if(pthread_mutex_unlock(&jobContext->map_mutex)!=0){
        std::cerr << MUTEX_LOCK_FAIL << std::endl;
        exit(FAILURE);
    }
    // return prev counter value

}

void do_map(ThreadContext* threadContext){
    JobContext jobContext = threadContext->jobContext;

    start_map_stage(jobContext);

    // get avail index

    avail_index = jobContext->atomic_counter.fetch_add(1);

    while (avail_index <= jobContext->inputVec.size()){
        InputPair pair_to_map = jobContext.inputVec[avail_index];
        jobContext.client->map(pair_to_map.first, pair_to_map.second, &threadContext);
        // threadContext->intermediate_vector->push_back(intermediatePair);
        // TODO: do in emit2
    }

    //create the intermediate vectors # TODO : check if needed

}

void do_sort(ThreadContext* threadContext){
    std::sort(threadContext->intermediate_vector->begin(),threadContext->intermediate_vector->end())
}



void * run_map_reduce(void* arg){
    ThreadContext* t_context = (ThreadContext*) arg;

    do_map(t_context);

    // do sort
    do_sort(t_context);

    // do barrier
    t_context->jobContext->barrier->barrier();

    // do shuffle


    // do reduce


}

JobHandle startMapReduceJob(const MapReduceClient& client,
                            const InputVec& inputVec, OutputVec& outputVec,
                            int multiThreadLevel){
//TODO: add fields to job context
    JobContext jobContext = new JobContext {
    .t_context = new ThreadContext[multiThreadLevel],
    .threads  = new pthread_t[multiThreadLevel],
    .inputVec = inputVec,
    .outputVec = outputVec,
    .client = client,
    .jobState = new JobState {UNDEFINED_STAGE, 0.0},
    .atomic_counter = {0},
    .map_mutex = {0},
    .barrier = new Barrier(multiThreadLevel)}; // check syntax

    for (int i=0; i<multiThreadLevel; i++){
        jobContext.t_context[i] = ThreadContext{.tid = i, .atomic_counter = &jobContext.atomic_counter,.intermediate_vectors = new IntermediateVec(), .jobContext=jobContext};
    }
    for (int i=0; i<multiThreadLevel; i++){
        if (pthread_create(jobContext.threads + i, NULL, run_map_reduce,jobContext.t_context[i])!=0){
//TODO: be aware of jobContext.threads + i instead of jobContext.threads[i]
            std::cerr << PTHREAD_CREATE_ERROR << std::endl;
            exit(FAILURE);
        }
    }

    JobHandle jobHandle = static_cast<JobHandle>(jobContext);
    return jobHandle;

}

void waitForJob(JobHandle job);
void getJobState(JobHandle job, JobState* state);
void closeJobHandle(JobHandle job);


