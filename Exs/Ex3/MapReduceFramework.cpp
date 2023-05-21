#include "MapReduceFramework.h"
#include <atomic>
#include "pthread.h"
#include "Barrier.h"
#include <algorithm>
#include <map>
#include "vector"
#include <iostream>


#define PTHREAD_CREATE_ERROR "system error: unable to create a thread"
#define FAILURE 1
#define MUTEX_LOCK_FAIL "system error: unable to lock/unlock mutex"
#define PTHREAD_JOIN_FAIL "system error: pthread join failed"
#define MUTEX_DESTROY_FAIL "system error: fail to destroy mutex"
# define BITWISE_31 ((1U<<31) - 1)
# define MAP_FLAG true

/**
 * The struct of the thread, has id and vector of pairs, haas pointer to job context
 */
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
/**
 *Job context object, build to gold all the muteses, the shared data of all the threads etc.
 */
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
    std::atomic<int> shuffle_vector_size;
    bool wait_flag;
//    std::atomic<int> reduce_counter;
};
/**
 * Using in the map stage for each thread to push the pair to the intermediate vector in each thread
 * @param key
 * @param value
 * @param context
 */
void emit2 (K2* key, V2* value, void* context){
    ThreadContext* threadContext = (ThreadContext*) context;
    threadContext->intermediate_vector->push_back(IntermediatePair(key, value));
    // increase the cursize of the the job data
    JobContext* jobContext = (JobContext*) threadContext->jobContext;
    jobContext->shuffle_vector_size ++;
    // todo check if not need fetchadd
    // why do we need the counter above


}
/**
 * using in the reduce phase, pushing to the output vetor, using mutex because going on the shared data
 * @param key key the key from the shuffle vec
 * @param value value from the shuffled vec
 * @param context job context that holds the output vec
 */
void emit3 (K3* key, V3* value, void* context){

    JobContext* jobContext = (JobContext*) context;

    // added mutex for locking with touching the shared data
    if(pthread_mutex_lock(&jobContext->emit_mutex)!=0){
        std::cout << MUTEX_LOCK_FAIL << std::endl;
        exit(FAILURE);
    }
// add the pair to the output vec
    jobContext->outputVec.push_back(OutputPair (key, value));

    if(pthread_mutex_unlock(&jobContext->emit_mutex)!=0){
        std::cout << MUTEX_LOCK_FAIL << std::endl;
        exit(FAILURE);
    }
// using the counter to the stage calculation
    jobContext->atomic_counter++;

}
/**
 * The function uses to initialize the stage and the state in each change of phase- map, shuffle and reduce
 * @param jobContext jobContext
 * @param stage  the stage to switch to
 */
void init_stage(JobContext* jobContext, stage_t stage){

    if(pthread_mutex_lock(&jobContext->init_mutex)!=0){
        std::cout << MUTEX_LOCK_FAIL << std::endl;
        exit(FAILURE);
    }
//    stage_t stage = jobContext->jobState.stage;
    if (!jobContext->init_state_flag){
        jobContext->init_state_flag = true;
//        updating the stage to be map or reduce
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

/**
 * The main function that runs the map phase for each thread. the function takes the pairs from the input vector and
 * working on mapping it to a thread
 * @param threadContext  threadContext the data in the thread to map
 */
void do_map(ThreadContext* threadContext){
    JobContext* jobContext = (JobContext *)threadContext->jobContext;


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


}
/**
 * sort the thread context vector as require in the algorithm
 * @param threadContext threadContext
 */
void do_sort(ThreadContext* threadContext){
    std::sort(threadContext->intermediate_vector->begin(),threadContext->intermediate_vector->end());
}

/**
 * The function make the shuffle in thread 0- updating the stage counter- state data,
 * shuffling the data using compare key of the given keys
 * @param jobContext jobContext
 */
void do_shuffle(JobContext* jobContext){
    struct CompareKeys {
        bool operator()(const K2* first, const K2* second) const {
            return *first<*second;
        }
    };

    // here shuffle vector size holds the number of <key,val> pairs in all data set
    int total_to_shuffle = jobContext->shuffle_vector_size.load(); // get the num of shufled from map phase
    jobContext->state_data = (2UL << 62) + ((unsigned long)(total_to_shuffle) << 31); // updating the state data

    jobContext->init_state_flag = false;
    jobContext->shuffle_vector_size = 0; // make all the counter 0 and flag before reduce phase
    jobContext->atomic_counter = 0;

    ThreadContext** allThreadContext = jobContext->t_context;
    std::map<K2*,IntermediateVec,CompareKeys> shuffle_map;


// looping for all the threads to shuffle the context into the shuffle map
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
            jobContext->state_data ++; // updating the counter of pairs that shuffled
        }
    }
    for (const auto& key_to_vals :shuffle_map){
        jobContext->shuffle_vector.push_back(key_to_vals.second);
    }
    // now shuffle vector size holds the number of unique keys
    jobContext->shuffle_vector_size = jobContext->shuffle_vector.size(); // num of shuffled objects by the vector

}
/**
 * the main function that doing the reduce phase by each thread. The function runs over the shuffled vector from
 * the job context and reduce each one of the pairs from the shuffle phase.
 * @param jobContext jobContext which holds all the shuffled vector
 */
void do_reduce( JobContext* jobContext){

    // todo stage and state
    init_stage(jobContext, REDUCE_STAGE);
    while (true){
        // using mutex before the the reduce- so each one of the threads will pop out 1 element from the shared data-
        // the shuffled vector form the job context
        if(pthread_mutex_lock(&jobContext->reduce_mutex)!=0){
            std::cout << MUTEX_LOCK_FAIL << std::endl;
            exit(FAILURE);
        }
        if(!(jobContext->shuffle_vector.empty())){
            auto popped_vec_pointer = jobContext->shuffle_vector.back();
            jobContext->shuffle_vector.pop_back(); // pop_back() for

            if(pthread_mutex_unlock(&jobContext->reduce_mutex)!=0){
                std::cout << MUTEX_LOCK_FAIL << std::endl;
                exit(FAILURE);
            }
            //reducing the pair
            jobContext->client.reduce(&popped_vec_pointer, (void *) jobContext);
            jobContext->state_data++; // using the shared data counter

        }
        else{
            if(pthread_mutex_unlock(&jobContext->reduce_mutex)!=0){
                std::cout << MUTEX_LOCK_FAIL << std::endl;
                exit(FAILURE);
            }
            break; // finish the reduce phase if all the elements from the reduce phase
        }

    }

}
/**
 * The main function that runs the process
 * @param arg a pointer to the thread that holds data for running the process of map-reduce
 * @return
 */
void* run_map_reduce(void* arg){
    ThreadContext* t_context = (ThreadContext*) arg;
    JobContext* jobContext = (JobContext*) t_context->jobContext;
    init_stage(jobContext, MAP_STAGE); // initialize the map stage - doing once by using flag
    do_map(t_context); // do the map phase

    // do sort
    do_sort(t_context);

    // do barrier and hold until the shuffle- all threads will wait here until all other will arrive
    jobContext->barrier->barrier();

    // counter == 0 for shuffle part
    // do shuffle
    if (t_context->tid == 0){
        do_shuffle(jobContext);
    }

    jobContext->barrier->barrier();
    // do reduce phase
    do_reduce(jobContext);

    return nullptr;

}
/**
 * The function initialize the Job context struct that is the main object of the job- also the thread context and the
 * pthread it self.
 * @param client client
 * @param inputVec  inputVec
 * @param outputVec  outputVec
 * @param multiThreadLevel  multiThreadLevel
 * @return Job handle - casting of the job context for all the process.
 */
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
    // creating thread context
    for (int i=0; i<multiThreadLevel; i++){
        jobContext->t_context[i] = new ThreadContext(i, new IntermediateVec(), jobContext);
//        jobContext->t_context[i] = new ThreadContext.tid = i,.intermediate_vector = new IntermediateVec(), .jobContext=jobContext};
    }
    // creating threads for each thread context
    for (int i=0; i<multiThreadLevel; i++){

        if (pthread_create(&(jobContext->threads[i]), nullptr, run_map_reduce,jobContext->t_context[i])!=0){
            std::cout << PTHREAD_CREATE_ERROR << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    JobHandle jobHandle = static_cast<JobHandle>(jobContext);
    return jobHandle;

}
/**
 * The function wait for the main job to finish before close it. The function join the p_threads
 * @param job jobContext
 */
void waitForJob(JobHandle job){
    JobContext* jobContext = (JobContext*) job;

    if(! jobContext->wait_flag){
        jobContext->wait_flag = true;
        for (int i=0; i< jobContext->threads_num; i++){
            if(pthread_join(jobContext->threads[i], nullptr) !=0){
                std::cout << PTHREAD_JOIN_FAIL << std::endl;
                exit(FAILURE);
            }
        }
    }
}

/**
 * the function calculate the current state of the process- the phase and the percentage of progress
 * @param job job Context
 * @param state the current state - job state object
 */
void getJobState(JobHandle job, JobState* state){
    JobContext* jobContext = (JobContext*) job;
    unsigned long int job_data = jobContext->state_data.load();
    stage_t stage = static_cast<stage_t> (job_data >> 62);
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
/**
 * the function destroy the job- all the mutexes, barrier and delete the allocations
 * @param job
 */
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


