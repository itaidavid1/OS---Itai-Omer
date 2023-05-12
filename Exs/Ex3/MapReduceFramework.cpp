#include "MapReduceFramework.h"
#include <atomic>
#include "pthread.h"
#include "Barrier.h"
#include <algorithm>
#include <map>

///////

#define PTHREAD_CREATE_ERROR "unable to create a thread"
#define FAILURE 1
#define MUTEX_LOCK_FAIL "unable to lock/unlock mutex"




struct JobContext {
    pthread_t * threads;
    JobState  jobState;
    std::atomic<int> atomic_counter;
    std::atomic<int> shuffle_vector_size;
    std::atomic<int> reduce_counter;
    ThreadContext* t_context;
    InputVec* inputVec;
    IntermediateVec* shuffle_vector;
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
    //TODO: create the weird vector with the bit shifting

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

void do_shuffle(ThreadContext* threadContext){
    struct CompareKeys {
        bool operator()(const K2* first, const K2* second) const {
            return *first<*second;
        }
    };
    IntermediateVec* shuffle_vec = threadContext->jobContext->shuffle_vector;
    ThreadContext* allThreadContext = threadContext->jobContext->t_context;
    std::map<K2*,IntermediateVec,CompareKeys> shuffle_map;

    for (ThreadContext* cur_t_context : allThreadContext){
        IntermediateVec temp_vec = cur_t_context->intermediate_vector;
        for (IntermediatePair K_V_pair : temp_vec){
            if (shuffle_map.find(K_V_pair.first) == shuffle_map.end()){
                shuffle_map[K_V_pair.first] = new IntermediateVec();
                shuffle_map.at(K_V_pair.first).push_back(K_V_pair);
            }
            else{
                shuffle_map.at(K_V_pair.first).push_back(K_V_pair);
            }
        //TODO : add atomic counter
        }
    }
    for (auto key_to_vals :shuffle_map){
        shuffle_vec->push_back(key_to_vals.second);
    }

}

void do_reduce(ThreadContext* threadContext){



}

void * run_map_reduce(void* arg){
    ThreadContext* t_context = (ThreadContext*) arg;

    do_map(t_context);

    // do sort
    do_sort(t_context);

    // do barrier
    t_context->jobContext->barrier->barrier();

    // do shuffle
    if (t_context->tid == 0){
        do_shuffle(t_context);
    }

    t_context->jobContext->barrier->barrier();
    // do reduce
    do_reduce(t_context);


}

JobHandle startMapReduceJob(const MapReduceClient& client,
                            const InputVec& inputVec, OutputVec& outputVec,
                            int multiThreadLevel){
//TODO: add fields to job context
    JobContext jobContext = new JobContext {
    .t_context = new ThreadContext[multiThreadLevel],
    .threads  = new pthread_t[multiThreadLevel],
    .inputVec = inputVec,
    .shuffle_vector = new IntermediateVec(),
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


