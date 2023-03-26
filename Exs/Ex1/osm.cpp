#include <sys/time.h>
#include "osm.h"
#include <cstddef>

/* Time measurement function for a simple arithmetic operation.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */


double osm_operation_time(unsigned int iterations)
{
    struct timeval t1, t2; // holding datatype of type timavals which is struct
    unsigned int i;
    unsigned int a = 1;

    gettimeofday(&t1, NULL); // using gettimeofday for the first iteration
    for (i = 0; i < iterations; i++) {
        a += 1; // single addition
    }
    gettimeofday(&t2, NULL); // using gettimeofday for the end of iteration

    /* compute the elapsed time in nano-seconds */
    return ((t2.tv_sec * 1000000 + t2.tv_usec) - (t1.tv_sec * 1000000 + t1.tv_usec)) * 1000.0 / iterations;
}


/* Time measurement function for an empty function call.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */


double osm_function_time(unsigned int iterations)
{
    struct timeval t1, t2;
    unsigned int i;
    auto empty_func =[](){};

  //declare a function that return void and has no arguments

    gettimeofday(&t1, NULL);
    for (i = 0; i < iterations; i++) {
        /* call an empty function */
        empty_func();
    }
    gettimeofday(&t2, NULL);

    /* compute the elapsed time in nano-seconds */
    return ((t2.tv_sec * 1000000 + t2.tv_usec) - (t1.tv_sec * 1000000 + t1.tv_usec)) * 1000.0 / iterations;
}


/* Time measurement function for an empty trap into the operating system.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_syscall_time(unsigned int iterations)
{
    struct timeval t1, t2;
    unsigned int i;

    gettimeofday(&t1, NULL);
    for (i = 0; i < iterations; i++) {
        /* trigger a system call that does nothing */
        OSM_NULLSYSCALL;
    }
    gettimeofday(&t2, NULL);

    /* compute the elapsed time in nano-seconds */
    return ((t2.tv_sec * 1000000 + t2.tv_usec) - (t1.tv_sec * 1000000 + t1.tv_usec)) * 1000.0 / iterations;
}
