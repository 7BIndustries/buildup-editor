#include <stdio.h>
#include <sys/time.h>
#include <time.h>

/*
 * Provides a timestamp that can be used for a timer.
 */
static long timestamp(void)
{
    struct timeval tv;
    if (gettimeofday(&tv, NULL) < 0) return 0;
    return (long)((long)tv.tv_sec * 1000 + (long)tv.tv_usec/1000);
}

/*
 * Adds a delay into the UI loop of Nuklear to keep from maxing out the CPU.
 */
static void sleep_for(long t)
{
    struct timespec req;
    const time_t sec = (int)(t/1000);
    const long ms = t - (sec * 1000);
    req.tv_sec = sec;
    req.tv_nsec = ms * 1000000L;
    while(-1 == nanosleep(&req, &req));
}