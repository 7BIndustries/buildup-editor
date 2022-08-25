/******************************************************************************
 * bue_ui -- Encapsulates functions related to file and directory I/O.        * 
 *                                                                            *
 * Author: 7B Industries                                                      *
 * License: Apache 2.0                                                        *
 *                                                                            *
 * ***************************************************************************/

#include <stdio.h>
#include <dirent.h>

DIR* open_dir;

/*
 * Collects the contents of the directory to pass back to the caller.
 * This does not filter any files out as it is expected that the caller
 * will do that.
 */
static char** list_dir_contents(char* dir_path) {
    char** dir_contents = NULL;

    /* Open the directory and make sure there was not a problem */
    open_dir = opendir(dir_path);
    if (open_dir != NULL) {
        /* Attempt to actually read the contents of the directory */
        struct dirent *data = readdir(open_dir);

    }

    /* Make sure the directory resource is closed if we sucessfully got it open. */
    if (open_dir) closedir(open_dir);
}
