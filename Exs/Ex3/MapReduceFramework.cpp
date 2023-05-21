#include "MapReduceFramework.h"
#include <atomic>
#include "pthread.h"
#include "Barrier.h"
#include <algorithm>
#include <map>
#include "vector"
#include <iostream>

///////

#define PTHREAD_CREATE_ERROR "system error: unable to create a thread"
#define FAILURE 1
#define MUTEX_LOCK_FAIL "system error: unable to lock/unlock mutex"
#define PTHREAD_JOIN_FAIL "system error: pthread join failed"
#define MUTEX_DESTROY_FAIL "system error: fail to destroy mutex"
# define BITWISE_31 ((1U<<31) - 1)
# define MAP_FLAG true


struct ThreadContext{
    int tid;
    IntermediateVec* intermediate_vector;
    void* jobContext;

    ThreadContext(int tid, IntermediateVec* intermediate_vector,  void* jobContext){
        this->tid = tid;
        this->intermediate_vector = intermediate_vector;
        this->jobContext = jobContext;
    }

};

struct JobContext {
    pthread_t* threads;
//    JobState  jobState;
    ThreadContext** t_context;
    const InputVec& inputVec;
    std::vector<IntermediateVec>  shuffle_vector;
    OutputVec& outputVec;
    const MapReduceClient& client;
    std::atomic<uint64_t> state_data;
    bool init_state_flag;
    pthread_mutex_t map_mutex;
    pthread_mutex_t reduce_mutex;
    pthread_mutex_t init_mutex;
    pthread_mutex_t emit_mutex;
    pthread_mutex_t  wait_for_job_mutex;
    Barrier* barrier;
    int threads_num;
    std::atomic<int> atomic_counter; // count the number of input parts
    std::atomic<int> intermediate_counter;
    std::atomic<int> shuffle_vector_size;
    bool wait_flag;
//    std::atomic<int> reduce_counter;
};
typedef std::pair<JobContext* , int> JobTID;

void emit2 (K2* key, V2* value, void* context){
    ThreadContext* threadContext = (ThreadContext*) context;
    threadContext->intermediate_vector->push_back(IntermediatePair(key, value));
    // increase the cursize of the the job data
    JobContext* jobContext = (JobContext*) threadContext->jobContext;
    jobContext->shuffle_vector_size ++;
    // todo check if not need fetchadd
    // why do we need the counter above


}
void emit3 (K3* key, V3* value, void* context){

    JobContext* jobContext = (JobContext*) context;

    // added mutex for locking with touching the shared data
    if(pthread_mutex_lock(&jobContext->emit_mutex)!=0){
        std::cout << MUTEX_LOCK_FAIL << std::endl;
        exit(FAILURE);
    }

    jobContext->outputVec.push_back(OutputPair (key, value));

    if(pthread_mutex_unlock(&jobContext->emit_mutex)!=0){
        std::cout << MUTEX_LOCK_FAIL << std::endl;
        exit(FAILURE);
    }

    jobContext->atomic_counter++;

}

void init_stage(JobContext* jobContext, stage_t stage){
    if(pthread_mutex_lock(&jobContext->init_mutex)!=0){
        std::cout << MUTEX_LOCK_FAIL << std::endl;
        exit(FAILURE);
    }
//    stage_t stage = jobContext->jobState.stage;
    if (!jobContext->init_state_flag){
        jobContext->init_state_flag = true;
//        jobContext->jobState.stage = stage;
        if(stage == MAP_STAGE){
            jobContext->state_data = ((unsigned long ) (stage) <<62 ) + ((unsigned long) (jobContext->inputVec.size()) <<31 );
        }
        else{
            jobContext->state_data = ((unsigned long ) (stage) <<62 ) + ((unsigned long) (jobContext->shuffle_vector_size) <<31 );
        }
    }
    if(pthread_mutex_unlock(&jobContext->init_mutex)!=0){
        std::cout << MUTEX_LOCK_FAIL << std::endl;
        exit(FAILURE);
    }
}


// todo no need of this func
//void start_map_stage(JobContext* jobContext){
//    // mutex lock
//    if(pthread_mutex_lock(&jobContext->map_mutex)!=0){
//        std::cout << MUTEX_LOCK_FAIL << std::endl;
//        exit(FAILURE);
//    };
//    //get counter value
//    if (jobContext->jobState.stage == UNDEFINED_STAGE){
//        jobContext->jobState.stage = MAP_STAGE;
//    }
//    //TODO: create the weird vector with the bit shifting
//    // mutex unlock
//    if(pthread_mutex_unlock(&jobContext->map_mutex)!=0){
//        std::cout << MUTEX_LOCK_FAIL << std::endl;
//        exit(FAILURE);
//    }
//    init_stage(JobContext* jobContext);
//    // return prev counter value
//
//}

void do_map(ThreadContext* threadContext){
    JobContext* jobContext = (JobContext *)threadContext->jobContext;

//    init_stage(jobContext, MAP_STAGE);

    // old version

    // get avail index

//    long int avail_index = jobContext->atomic_counter.fetch_add(1);
//
//    while (avail_index <= (long int) jobContext->inputVec.size()){
//        jobContext->state_data ++;
//        InputPair pair_to_map = jobContext->inputVec[avail_index];
//        jobContext->client.map(pair_to_map.first, pair_to_map.second, threadContext);
//        // threadContext->intermediate_vector->push_back(intermediatePair);
//    }

//  new version

    while (MAP_FLAG){
        long int avail_index = jobContext->atomic_counter.fetch_add(1);
        if (avail_index >= (long int) jobContext->inputVec.size()){
            break;
        }
        jobContext->state_data ++;
        InputPair pair_to_map = jobContext->inputVec[avail_index];
        auto key = pair_to_map.first;
        auto val = pair_to_map.second;
        jobContext->client.map(key, val, threadContext);
        // threadContext->intermediate_vector->push_back(intermediatePair);
    }

    //create the intermediate vectors # TODO : check if needed

}

void do_sort(ThreadContext* threadContext){
    std::sort(threadContext->intermediate_vector->begin(),threadContext->intermediate_vector->end());
}

void do_shuffle(JobContext* jobContext){
    struct CompareKeys {
        bool operator()(const K2* first, const K2* second) const {
            return *first<*second;
        }
    };
//    JobContext* jobContext = (JobContext*) threadContext->jobContext;
//    int num_shuffle = 0;
    // here shuffle vector size holds the number of <key,val> pairs in all data set
    int total_to_shuffle = jobContext->shuffle_vector_size.load(); // get the num of shufled from map phse
    jobContext->state_data = (2UL << 62) + ((unsigned long)(total_to_shuffle) << 31);

    jobContext->init_state_flag = false;
    jobContext->shuffle_vector_size = 0; // make all the counter 0 and flag before reduce phase
    jobContext->atomic_counter = 0;
//    std::vector<IntermediateVec> shuffle_vec = jobContext->shuffle_vector;
    ThreadContext** allThreadContext = jobContext->t_context;
    std::map<K2*,IntermediateVec,CompareKeys> shuffle_map;
//    ThreadContext* cur_t_context : allThreadContext


    for (int i=0; i< jobContext->threads_num; i++){
        IntermediateVec* temp_vec = allThreadContext[i]->intermediate_vector;
        for (auto K_V_pair : *temp_vec){
            if (shuffle_map.find(K_V_pair.first) == shuffle_map.end()){
                shuffle_map[K_V_pair.first] = IntermediateVec();
                shuffle_map.at(K_V_pair.first).push_back(K_V_pair);
            }
            else{
                shuffle_map.at(K_V_pair.first).push_back(K_V_pair);
            }
            //TODO : add atomic counter
            jobContext->state_data ++;
        }
    }
    for (const auto& key_to_vals :shuffle_map){
        jobContext->shuffle_vector.push_back(key_to_vals.second);
    }
    // now shuffle vector size holds the number of unique keys
    jobContext->shuffle_vector_size = jobContext->shuffle_vector.size(); // maybe should be different
//    jobContext->init_state_flag = false; // todo check if it is right place
}

void do_reduce( JobContext* jobContext){

    // todo stage and state
    init_stage(jobContext, REDUCE_STAGE);
    while (true){
//        JobContext jobContext = threadContext->jobContext;
        if(pthread_mutex_lock(&jobContext->reduce_mutex)!=0){
            std::cout << MUTEX_LOCK_FAIL << std::endl;
            exit(FAILURE);
        }
        // old version

//        if((int) (jobContext->state_data.load() & BITWISE_31) == jobContext->shuffle_vector_size){
////        if((int) (jobContext->state_data.load() & BITWISE_31) == jobContext->shuffle_vector_size){
//            if(pthread_mutex_unlock(&jobContext->reduce_mutex)!=0){
//                std::cout << MUTEX_LOCK_FAIL << std::endl;
//                exit(FAILURE);
//            }
//            break;
//        }
//        auto popped_vec_pointer = jobContext->shuffle_vector.back();
//        jobContext->shuffle_vector.pop_back(); // pop_back() for
////        jobContext.reduce_counter ++;
//        if(pthread_mutex_unlock(&jobContext->reduce_mutex)!=0){
//            std::cout << MUTEX_LOCK_FAIL << std::endl;
//            exit(FAILURE);
//        }
//        jobContext->client.reduce(&popped_vec_pointer, (void *) jobContext);
//        jobContext->state_data++; // using the shared data counter
//    }

//             new version
        if(!(jobContext->shuffle_vector.empty())){
            auto popped_vec_pointer = jobContext->shuffle_vector.back();
            jobContext->shuffle_vector.pop_back(); // pop_back() for

            if(pthread_mutex_unlock(&jobContext->reduce_mutex)!=0){
                std::cout << MUTEX_LOCK_FAIL << std::endl;
                exit(FAILURE);
            }
            jobContext->client.reduce(&popped_vec_pointer, (void *) jobContext);
            jobContext->state_data++; // using the shared data counter

        }
        else{
            if(pthread_mutex_unlock(&jobContext->reduce_mutex)!=0){
                std::cout << MUTEX_LOCK_FAIL << std::endl;
                exit(FAILURE);
            }
            break;
        }

    }


}

void* run_map_reduce(void* arg){
    ThreadContext* t_context = (ThreadContext*) arg;
    JobContext* jobContext = (JobContext*) t_context->jobContext;
    init_stage(jobContext, MAP_STAGE);
    do_map(t_context);

    // do sort
    do_sort(t_context);

    // do barrier
    jobContext->barrier->barrier();

    // counter == 0 for shuffle part
    // do shuffle
    if (t_context->tid == 0){
//       jobContext->state_data = jobContext->state_data >> 31;
//       jobContext->state_data  = jobContext->state_data << 31;
        do_shuffle(jobContext);
    }

    jobContext->barrier->barrier();
    // do reduce
    do_reduce(jobContext);
    // check if ok

    return nullptr;

}

JobHandle startMapReduceJob(const MapReduceClient& client,
                            const InputVec& inputVec, OutputVec& outputVec,
                            int multiThreadLevel){
//TODO: add fields to job context


    JobContext* jobContext = new JobContext {
            .threads  = new pthread_t[multiThreadLevel],
            .t_context = new ThreadContext*[multiThreadLevel],
            .inputVec = inputVec,
            .shuffle_vector = *(new std::vector<IntermediateVec>),
            .outputVec = outputVec,
            .client = client,
            .state_data = {0},
            .init_state_flag = false,
            .map_mutex = PTHREAD_MUTEX_INITIALIZER,
            .reduce_mutex = PTHREAD_MUTEX_INITIALIZER,
            .init_mutex = PTHREAD_MUTEX_INITIALIZER,
            .emit_mutex = PTHREAD_MUTEX_INITIALIZER,
            .wait_for_job_mutex = PTHREAD_MUTEX_INITIALIZER,
            .barrier = new Barrier(multiThreadLevel),
            .threads_num = multiThreadLevel,
            .atomic_counter = {0},
            .shuffle_vector_size = {0},
            .wait_flag = false
    };


    for (int i=0; i<multiThreadLevel; i++){
        jobContext->t_context[i] = new ThreadContext(i, new IntermediateVec(), jobContext);
//        jobContext->t_context[i] = new ThreadContext.tid = i,.intermediate_vector = new IntermediateVec(), .jobContext=jobContext};
    }


    for (int i=0; i<multiThreadLevel; i++){
//        ThreadContext* threadContext = jobContext->t_context[i];
//        if (pthread_create(&(jobContext->threads[i]),
//                           nullptr,
//                           run_map_reduce,
//                           &threadContext) != 0) {
//            std::cout << "kk" << std::endl;
//            exit(1);
//        }
        // (void*) new JobTID(jobContext, i)

        if (pthread_create(&(jobContext->threads[i]), nullptr, run_map_reduce,jobContext->t_context[i])!=0){
//TODO: be aware of jobContext.threads + i instead of jobContext.threads[i]
            std::cout << PTHREAD_CREATE_ERROR << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    JobHandle jobHandle = static_cast<JobHandle>(jobContext);
    return jobHandle;

}

void waitForJob(JobHandle job){
    JobContext* jobContext = (JobContext*) job;
//    if(pthread_mutex_lock(&jobContext->wait_for_job_mutex)!=0){
//        std::cout << MUTEX_LOCK_FAIL << std::endl;
//        exit(FAILURE);
//    }
//    for (auto thread : *(jobContext->threads)){
    if(! jobContext->wait_flag){
        jobContext->wait_flag = true;
        for (int i=0; i< jobContext->threads_num; i++){
            if(pthread_join(jobContext->threads[i], nullptr) !=0){
                std::cout << PTHREAD_JOIN_FAIL << std::endl;
                exit(FAILURE);
            }
        }
    }

//    if(pthread_mutex_unlock(&jobContext->wait_for_job_mutex)!=0){
//        std::cout << MUTEX_LOCK_FAIL << std::endl;
//        exit(FAILURE);
//    }

}
void getJobState(JobHandle job, JobState* state){
    JobContext* jobContext = (JobContext*) job;
    unsigned long int job_data = jobContext->state_data.load();
    stage_t stage = static_cast<stage_t> (job_data >> 62); // todo if need static cast or regular
    auto  total_size =  (job_data >> 31) &BITWISE_31 ;
    auto cur_size = (job_data) & BITWISE_31;
    if(total_size == 0 ){
        state->percentage = 0;
    }
    else{
        float percentage = ((float) cur_size / total_size) * 100;
        state->percentage = percentage;
    }
    state->stage = stage;

}

void closeJobHandle(JobHandle job){
    waitForJob(job); // waitin for job to finish
    JobContext* jobContext = (JobContext*) job;

    if (pthread_mutex_destroy(&(jobContext->reduce_mutex)) != 0) {
        std::cout << MUTEX_DESTROY_FAIL << std::endl;
        exit(FAILURE);
    }
    if (pthread_mutex_destroy(&(jobContext->map_mutex)) != 0) {
        std::cout << MUTEX_DESTROY_FAIL << std::endl;
        exit(FAILURE);
    }
    if (pthread_mutex_destroy(&(jobContext->emit_mutex)) != 0) {
        std::cout << MUTEX_DESTROY_FAIL << std::endl;
        exit(FAILURE);
    }
    if (pthread_mutex_destroy(&(jobContext->init_mutex)) != 0) {
        std::cout << MUTEX_DESTROY_FAIL << std::endl;
        exit(FAILURE);
    }
    if (pthread_mutex_destroy(&(jobContext->wait_for_job_mutex)) != 0) {
        std::cout << MUTEX_DESTROY_FAIL << std::endl;
        exit(FAILURE);
    }
    pthread_mutex_destroy(&jobContext->reduce_mutex);
    pthread_mutex_destroy(&jobContext->map_mutex);
    pthread_mutex_destroy(&jobContext->emit_mutex);
    pthread_mutex_destroy(&jobContext->init_mutex);
    pthread_mutex_destroy(&jobContext->wait_for_job_mutex);

    delete jobContext->barrier;
    for (int i = 0; i < jobContext->threads_num; ++i) {
        delete[] jobContext->t_context[i];
    }
    delete[] jobContext->t_context;
    delete[] jobContext->threads;
    delete jobContext;




}


