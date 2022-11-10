#include <sys/time.h>
#include <time.h>

/******************************************************************************
 * timestamp - Provides a timestamp that can be used for a timer.             *
 *                                                                            *
 * Parameters                                                                 *
 *      None                                                                  *
 *                                                                            *
 * Returns                                                                    *
 *      A long iteger representing the current number of milliseconds that    *
 *      have elapsed today.                                                   *
 *****************************************************************************/
static long timestamp(void)
{
    struct timeval tv;
    if (gettimeofday(&tv, NULL) < 0) return 0;
    return (long)((long)tv.tv_sec * 1000 + (long)tv.tv_usec/1000);
}

/******************************************************************************
 * sleep_for -- Adds a delay into the UI loop of Nuklear to keep from maxing  *
 *              out the CPU.                                                  *
 *                                                                            *
 * Parameters                                                                 *
 *      t -- A long integer representing the number of milliseconds to sleep  *
 *          for.                                                              *
 *                                                                            *
 * Returns                                                                    *
 *      Nothing                                                               *
 *****************************************************************************/
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

/******************************************************************************
 * append_char_to_string -- Adds a specified single character onto the end of *
 *                          a string.                                         *
 *                                                                            *
 * Parameters                                                                 *
 *      prefix -- The string (char array) that the character will be added to.*
 *      suffix -- The single character to add to the prefix.                  *
 *                                                                            *
 * Returns                                                                    *
 *      Nothing                                                               *
 *****************************************************************************/
void append_char_to_string(char* prefix, char suffix) {
    // Save the original length of the string
    int len = strlen(prefix);

    // Resize the string, add the suffix, and terminate
    prefix = realloc(prefix, len + 1);
    prefix[len] = suffix;
    prefix[len + 1] = '\0';
}

/******************************************************************************
 * split_string -- Cuts a given string at the first occurence of the char.    *
 *                                                                            *
 * Parameters                                                                 *
 *      string -- The string to search for the delimiter character.           *
 *      delimiter -- The character to use to cut the string by adding a null  *
 *                   zero.                                                    *
 *                                                                            *
 * Returns                                                                    *
 *      Nothing                                                               *
 *****************************************************************************/
void cut_string(char* string, char delimiter) {
    // Step through all of the characters in the given string
    for (long unsigned int i = 0; i < strlen(string); i++) {
        if (string[i] == delimiter) {
            string[i] = '\0';
            break;
        }
    }
}

/******************************************************************************
 * cut_string_last -- Cuts a given string at the last occurence of the char.  *
 *                                                                            *
 * Parameters                                                                 *
 *      string -- The string to search in reverse for the delimiter char.     *
 *      delimiter -- The character to use to cut the string by adding a null  *
 *                   zero.                                                    *
 *                                                                            *
 * Returns                                                                    *
 *      Nothing                                                               *
 *****************************************************************************/
void cut_string_last(char* string, char delimiter) {
    // Step throuugh the string in reverse order
    for (long unsigned int i = strlen(string) - 1; i > 0; i--) {
        if (string[i] == delimiter) {
            string[i] = '\0';
            break;
        }
    }
}

/******************************************************************************
 * replace_file_extension -- Replaces whatever the end of the file name ext-  *
 *                           ension is with the ending .html. The purpose of  *
 *                           this is is to convert .md file links to .html.   *
 *                                                                            *
 * Parameters                                                                 *
 *      string -- The string holding the file name to change the ending of.   *
 *      new_ending -- The new file extension to replace the end with.         *
 *                                                                            *
 * Returns                                                                    *
 *      A string which holds the file name with the extension changed.        *
 *****************************************************************************/
char* replace_file_extension(char* string, char* new_ending) {
    // Find the last instance of the period character
    char* extension = strrchr(string, '.');

    char* new_filename = NULL;

    // See if the string needs to be resized
    if (strlen(extension) < strlen(new_ending)) {
        new_filename = malloc(sizeof(string) + sizeof(new_ending));
        memcpy(new_filename, string, strlen(string) + 1);
    }
    else {
        new_filename = strdup(string);
    }

    // Change the line ending
    cut_string_last(new_filename, '.');
    strcat(new_filename, new_ending);

    return new_filename;
}

/******************************************************************************
 * str_size -- Given a character pointer, gives the size of the string, in    *
 *             bytes, including the null zero at the end.                     *
 *                                                                            *
 * Parameters                                                                 *
 *      string -- Character pointer that holds the string to get the true     *
 *                byte-size of.                                               *
 *                                                                            *
 * Returns                                                                    *
 *      A size_t representing the total size of the string, including the     *
 *      null zero.                                                            *
 *****************************************************************************/
size_t str_size(const char* string) {
    // Calculate the size based on the size of a char and add one for the null zero
    size_t sz = (strlen(string) * sizeof(char)) + (1 * sizeof(char));

    return sz;
}
