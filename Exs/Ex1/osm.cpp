#include "osm.h"
#include <sys/time.h>


/**
 * function which does nothing
 */
void does_nothing() {}

/* Time measurement function for a simple arithmetic operation.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_operation_time(unsigned int iterations) {
    if (iterations == 0) {
        return -1;
    }
    // basic addition operation on the variable n
    unsigned int n = 0;
    struct timeval current_time;
    gettimeofday(&current_time, nullptr);
    time_t start = current_time.tv_usec + current_time.tv_sec*1000000;
    int x=1;
    for(n = 0; n < iterations; n += 10) {
        x = n + 1;
        x = n + 2;
        x = n + 3;
        x = n + 4;
        x = n + 5;
        x = n + 6;
        x = n + 7;
        x = n + 8;
        x = n + 9;
        x = n + 10;
    }
    gettimeofday(&current_time, nullptr);
    time_t time = (current_time.tv_usec + current_time.tv_sec*1000000) - start;
    // calculate the time and divide by iterations (in nanoseconds)
    return (double) time/iterations * 1000;
}


/* Time measurement function for an empty function call.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_function_time(unsigned int iterations) {
    if (iterations == 0) {
        return -1;
    }
    // basic addition operation on the variable n
    unsigned int n = 0;
    struct timeval current_time;
    gettimeofday(&current_time, nullptr);
    time_t start = current_time.tv_usec + current_time.tv_sec*1000000;
    for(n = 0; n < iterations; n += 10) {
        does_nothing();
        does_nothing();
        does_nothing();
        does_nothing();
        does_nothing();
        does_nothing();
        does_nothing();
        does_nothing();
        does_nothing();
        does_nothing();
    }
    gettimeofday(&current_time, nullptr);
    time_t time = (current_time.tv_usec + current_time.tv_sec*1000000) - start;
    // calculate the time and divide by iterations
    return (double) time/iterations * 1000;
}



/* Time measurement function for an empty trap into the operating system.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_syscall_time(unsigned int iterations) {
    if (iterations == 0) {
        return -1;
    }
    // basic addition operation on the variable n
    unsigned int n = 0;
    struct timeval current_time;
    gettimeofday(&current_time, nullptr);
    time_t start = current_time.tv_usec + current_time.tv_sec*1000000;
    for(n = 0; n < iterations; n += 10) {
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
    }
    gettimeofday(&current_time, nullptr);
    time_t time = (current_time.tv_usec + current_time.tv_sec*1000000) - start;
    // calculate the time and divide by iterations
    return (double) time/iterations * 1000;
}
