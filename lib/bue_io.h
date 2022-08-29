/******************************************************************************
 * bue_ui -- Encapsulates functions related to file and directory I/O.        * 
 *                                                                            *
 * Author: 7B Industries                                                      *
 * License: Apache 2.0                                                        *
 *                                                                            *
 * ***************************************************************************/

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>

char* new_path;
DIR* open_dir;

struct directory_contents {
    char** dir_contents;
    int listing_length;
};

/*
 * Collects the contents of the directory to pass back to the caller.
 * This does not filter any files out as it is expected that the caller
 * will do that.
 */
struct directory_contents list_dir_contents(char* dir_path) {
    int size = -1;  /* The size (length) of the array holding the directory contents */
    size_t capacity = 32;  /* The size (in bytes) of the array holding the directory contents */
    struct directory_contents contents;  /* The strings of the directory contents */

    /* Open the directory and make sure there was not a problem */
    open_dir = opendir(dir_path);
    if (open_dir != NULL) {
        /* Step through the given directory's contents */
        struct dirent *data = NULL;

        while ((data = readdir(open_dir)) != NULL) {
            /* Ignore hidden files and directories */
            if (data->d_name[0] == '.')
                continue;

            /* If dir_contents has not be initialized, do that now. Otherwise expand it to hold the new values. */
            if (size == -1) {
                size = 0;
                contents.dir_contents = (char**)calloc(sizeof(char*), capacity);
            }
            else {
                /* Make room for the new directory entry */
                size++;
                capacity = capacity * 2;
                contents.dir_contents = (char**)realloc(contents.dir_contents, capacity * sizeof(char*));
            }

            /* Add the new directory entry to the directory listing array */
            new_path = strdup(data->d_name);
            contents.dir_contents[size] = new_path;
        }

        /* Store the size of the array in the first array element */
        contents.listing_length = size;
    }
    else if (ENOENT == errno) {
        /* Set the size to a value that lets the caller know the directory does not exist */
        contents.listing_length = -2;
    }
    else {
        contents.listing_length = -1;
    }

    /* Make sure the directory resource is closed if we sucessfully got it open. */
    if (open_dir) closedir(open_dir);

    return contents;
}
