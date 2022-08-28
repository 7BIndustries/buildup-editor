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

DIR* open_dir;

static char* str_duplicate(const char *src)
{
    char *ret;
    size_t len = strlen(src);
    if (!len) return 0;
    ret = (char*)malloc(len+1);
    if (!ret) return 0;
    memcpy(ret, src, len);
    ret[len] = '\0';
    return ret;
}

/*
 * Collects the contents of the directory to pass back to the caller.
 * This does not filter any files out as it is expected that the caller
 * will do that.
 */
static char** list_dir_contents(char* dir_path) {
    size_t size;  /* The size (length) of the array holding the directory contents */
    size_t capacity = 32;  /* The size (in bytes) of the array holding the directory contents */
    char** dir_contents = NULL;  /* The strings of the directory contents */

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
            if (dir_contents == NULL) {
                size = 0;
                dir_contents = (char**)calloc(sizeof(char*), capacity);
            }
            else {
                /* Make room for the new directory entry */
                size++;
                capacity = capacity * 2;
                dir_contents = (char**)realloc(dir_contents, capacity * sizeof(char*));
            }

            /* Add the new directory entry to the directory listing array */
            char* new_path = str_duplicate(data->d_name);
            dir_contents[size] = new_path;

            printf("%s\n", data->d_name);
        }
    }

    /* Make sure the directory resource is closed if we sucessfully got it open. */
    if (open_dir) closedir(open_dir);

    return dir_contents;
}
