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
#define PTHREAD_JOIN_FAIL "pthread join failed"




struct JobContext {
    pthread_t * threads;
//    JobState  jobState;
    std::atomic<int> atomic_counter; // count the number of input parts
    std::atomic<int> intermediate_counter;
    std::atomic<int> shuffle_vector_size;
    std::atomic<int> reduce_counter;
    ThreadContext* t_context;
    InputVec* inputVec;
    IntermediateVec* shuffle_vector;
    OutputVec * outputVec;
    std::atomic<uint64_t> state_data;
    bool init_state_flag;
    MapReduceClient * client;
    pthread_mutex_t map_mutex;
    pthread_mutex_t reduce_mutex;
    pthread_mutex_t init_mutex;
    pthread_mutex_t emit_mutex;
    pthread_mutex_t  wait_for_job_mutex;
    Barrier* barrier;

};

struct ThreadContext{
    int tid;
//    std::atomic<int>* atomic_counter;
    IntermediateVec* intermediate_vector;
    JobContext* jobContext;

};

void emit2 (K2* key, V2* value, void* context){
    ThreadContext* threadContext = (ThreadContext*) context;
    threadContext->intermediate_vector->push_back(new IntermediatePair(key, value));
    // increase the cursize of the the job data
    threadContext->jobContext->state_data ++;
//    threadContext->jobContext->intermediate_counter ++;
    // todo check if not need fetchadd
    // why do we need the counter above


}
void emit3 (K3* key, V3* value, void* context){

    JobContext* jobContext = (JobContext*) context;

    // added mutex for locking with touching the shared data
    if(pthread_mutex_lock(&jobContext->emit_mutex)!=0){
        std::cerr << MUTEX_LOCK_FAIL << std::endl;
        exit(FAILURE);
    }

    jobContext->outputVec->push_back(new IntermediatePair(key, value));

    if(pthread_mutex_unlock(&jobContext->emit_mutex)!=0){
        std::cerr << MUTEX_LOCK_FAIL << std::endl;
        exit(FAILURE);
    }

    // todo check what counter to change

}

void init_stage(JobContext* jobContext, stage_t stage){
    if(pthread_mutex_lock(&jobContext->init_mutex)!=0){
        std::cerr << MUTEX_LOCK_FAIL << std::endl;
        exit(FAILURE);
    }
//    stage_t stage = jobContext->jobState.stage;
    if (!jobContext->init_state_flag){
        jobContext->init_state_flag = true;
//        jobContext->jobState.stage = stage;
        if(stage == MAP_STAGE){
            jobContext->state_data = ((unsigned long ) (stage) <<62 ) + ((unsigned int) (jobContext->inputVec->size()) <<31 )
        }
        else{
            jobContext->state_data = ((unsigned long ) (stage) <<62 ) + ((unsigned int) (jobContext->shuffle_vector_size) <<31 )
        }
    }
    if(pthread_mutex_unlock(&jobContext->init_mutex)!=0){
        std::cerr << MUTEX_LOCK_FAIL << std::endl;
        exit(FAILURE);
    }
}


// todo no need of this func
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
    init_stage(JobContext* jobContext);
    // return prev counter value

}

void do_map(ThreadContext* threadContext){
    JobContext jobContext = threadContext->jobContext;

    init_stage(jobContext, MAP_STAGE);

    // get avail index

    avail_index = jobContext->atomic_counter.fetch_add(1);

    while (avail_index <= jobContext->inputVec.size()){
        InputPair pair_to_map = jobContext.inputVec[avail_index];
        jobContext.client->map(pair_to_map.first, pair_to_map.second, &threadContext);
        // threadContext->intermediate_vector->push_back(intermediatePair);
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
    JobContext jobContext = threadContext->jobContext;

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
        jobContext.state_data ++;
        }
    }
    for (auto key_to_vals :shuffle_map){
        shuffle_vec->push_back(key_to_vals.second);
    }
    jobContext.shuffle_vector_size = shuffle_vec->size();
    jobContext.init_state_flag = false; // todo check if it is right place
}

void do_reduce( JobContext* jobContext){

    // todo stage and state
    init_stage(jobContext, REDUCE_STAGE);
    while (true){
//        JobContext jobContext = threadContext->jobContext;
        if(pthread_mutex_lock(&jobContext->reduce_mutex)!=0){
            std::cerr << MUTEX_LOCK_FAIL << std::endl;
            exit(FAILURE);
        }
        if(jobContext.reduce_counter == jobContext.shuffle_vector_size){
            if(pthread_mutex_unlock(&jobContext->reduce_mutex)!=0){
                std::cerr << MUTEX_LOCK_FAIL << std::endl;
                exit(FAILURE);
            }
            break;
        }
        auto popped_vec_pointer = jobContext.shuffle_vector->back();
//        jobContext.reduce_counter ++;
        jobContext->state_data++; // using the shared data counter
        if(pthread_mutex_unlock(&jobContext->reduce_mutex)!=0){
            std::cerr << MUTEX_LOCK_FAIL << std::endl;
            exit(FAILURE);
        }
//        jobContext.shuffle_vector->pop_back();
        jobContext.client->reduce(&popped_vec_pointer, (void *) jobContext);
    }

}

void * run_map_reduce(void* arg){
    ThreadContext* t_context = (ThreadContext*) arg;

    do_map(t_context);

    // do sort
    do_sort(t_context);

    // do barrier
    t_context->jobContext->barrier->barrier();



    // counter == 0 for shuffle part
    // do shuffle
    if (t_context->tid == 0){
        t_context->jobContext->state_data >> 31;
        t_context->jobContext->state_data << 31;
        do_shuffle(t_context);
    }

    t_context->jobContext->barrier->barrier();
    // do reduce
    do_reduce(t_context->jobContext);
    // check if ok

    return nullptr;


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
    .shuffle_vector_size = {0},
    .reduce_counter = {0},
    .intermediate_counter = {0},
//    .jobState = new JobState {UNDEFINED_STAGE, 0.0}, // not needed
    .atomic_counter = {0},
    .map_mutex = {0},
    .reduce_mutex = {0},
    .emit_mutex = {0},
    .wait_for_job_mutex = {0},
    .state_data = {0},
    .init_state_flag = false,
    .init_mutex = {0}
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

void waitForJob(JobHandle job){
    JobContext jobContext = (JobContext) job;
    if(pthread_mutex_lock(&jobContext.wait_for_job_mutex)!=0){
        std::cerr << MUTEX_LOCK_FAIL << std::endl;
        exit(FAILURE);
    }
    for (pthread_t* thread : jobContext.threads){
        if(pthread_join(thread, nullptr) !=0){
            std::cerr << PTHREAD_JOIN_FAIL << std::endl;
            exit(FAILURE);
        }
    }
    if(pthread_mutex_unlock(&jobContext.wait_for_job_mutex)!=0){
        std::cerr << MUTEX_LOCK_FAIL << std::endl;
        exit(FAILURE);
    }

}
void getJobState(JobHandle job, JobState* state){
    JobContext* jobContext = (JobContext *) job;
    std::atomic<uint64_t> job_data = jobContext->state_data.load();
    stage_t stage = static_cast<stage_t> job_data>>62; // todo if need static cast or regular
    long int total_size = (job_data >> 31) & (1U<<31 - 1);
    long int cur_size = (job_data) & (1U<<31 - 1);
    float percentage = cur_sreducize / total_size;
    state->percentage = percentage;
    state->stage = stage;

}

void closeJobHandle(JobHandle job){
    waitForJob(job); // waitin for job to finish
    JobContext jobContext = (JobContext*) job;

    if (pthread_mutex_destroy(&(jobContext->reduce_mutex)) != 0) {
        std::cerr << MUTEX_DESTROY_FAIL << std::endl;
        exit(FAILURE);
    }
    if (pthread_mutex_destroy(&(jobContext->map_mutex)) != 0) {
        std::cerr << MUTEX_DESTROY_FAIL << std::endl;
        exit(FAILURE);
    }
    if (pthread_mutex_destroy(&(jobContext->emit_mutex)) != 0) {
        std::cerr << MUTEX_DESTROY_FAIL << std::endl;
        exit(FAILURE);
    }
    if (pthread_mutex_destroy(&(jobContext->init_mutex)) != 0) {
        std::cerr << MUTEX_DESTROY_FAIL << std::endl;
        exit(FAILURE);
    }
    if (pthread_mutex_destroy(&(jobContext->wait_for_job_mutex)) != 0) {
        std::cerr << MUTEX_DESTROY_FAIL << std::endl;
        exit(FAILURE);
    }
//    pthread_mutex_destroy(jobContext.reduce_mutex);
//    pthread_mutex_destroy(jobContext.map_mutex);
//    pthread_mutex_destroy(jobContext.emit_mutex);
//    pthread_mutex_destroy(jobContext.init_mutex);
//    pthread_mutex_destroy(jobContext.wait_for_job_mutex);

    delete[] jobContext.t_context;
    delete[] jobContext.threads;
    delete job_context;




}


