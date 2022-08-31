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

/******************************************************************************
 * string_ends_with -- Checks whether a string ends with a specified suffix.  *
 *                                                                            *
 * Parameters                                                                 *
 *      str -- The string to be checked to see if it ends in the suffix.      *
 *      suffix -- The ending that is to be checked for in the given string.   *
 *                                                                            *
 * Returns                                                                    *
 *      A boolean specifying whether or not the suffix is present at the end  *
 *      of the given string.                                                  *
 *****************************************************************************/
static bool string_ends_with(const char* str, const char* suffix) {
    bool ends_with = false;  // Flag to track if suffix is present at the end

    // Null check
    if (!str || !suffix)
        return false;

    // If suffix is longer than the str, the string cannot end with the suffix
    int str_length = strlen(str);
    int suffix_length = strlen(suffix);

    // If suffix is longer than str, there is no reason to compare them
    if (suffix_length >  str_length) {
        return false;
    }
    else {
        // Compare the letters at the end of the string with the suffix letters
        for (int i = 0; i < suffix_length; i++) {
            if (str[i + str_length - suffix_length] != suffix[i]) {
                ends_with = false;
                break;
            }

            // Falling through to this point means the string has matched (so far)
            ends_with = true;
        }
    }

    return ends_with;
}