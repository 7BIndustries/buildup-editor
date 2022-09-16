/******************************************************************************
 * bue_ui -- Encapsulates functions related to file and directory I/O.        * 
 *                                                                            *
 * Author: 7B Industries                                                      *
 * License: Apache 2.0                                                        *
 * ***************************************************************************/

// #include <stdio.h>
// #include <string.h>
#include <dirent.h>
#include <errno.h>

/* Filesystem path separators vary by OS */
#ifdef __linux__
    const char* PATH_SEP = "/";  // The filesystem path separator for Linux
#elif defined __unix__
    const char* PATH_SEP = "/";  // The filesystem path separator for this Unix
#elif defined _WIN32
    const char* PATH_SEP = "\\";  // The filesystem path separator for this Windows
#elif defined __APPLE__ && __MACH__
    const char* PATH_SEP = "/";  // The filesystem path separator for this OS
#endif

char* new_path;  // String of the path to the project directory
DIR* open_dir;  // DIRENT struct holding information on the open directory

// Holds the types of files we are working with
enum file_types {directory = 4, file = 8};

// struct holding the listed directory contents and extra information
struct directory_contents {
    char** dir_contents;
    int listing_length;
    int listing_capacity;
    int listing_type[255];
    short parent_dir[255];
};

/******************************************************************************
 * dir_free_list -- Frees the memory associated with the directory listing.   *
 *                                                                            *
 * Parameters                                                                 *
 *      list -- The array of strings holding the directory listing.           *
 *      size -- The length of the array.                                      *
 *                                                                            *
 * Returns                                                                    *
 *      void                                                                  *
 *****************************************************************************/
static void dir_free_list(char **list, size_t size)
{
    size_t i;
    for (i = 0; i < size; ++i)
        free(list[i]);
    free(list);
}

/******************************************************************************
 * check_for_buildup_files -- Makes a check to be sure that a few needed      *
 *                            project files are present in the directory      *
 *                            selected by the caller.                         *
 *                                                                            *
 * Parameters                                                                 *
 *      contents -- struct listing the contents of the directory that is      *
 *                  alleged to be a BuildUp project directory.                *
 *                                                                            *
 * Returns                                                                    *
 *      A boolean defining whether or not the given directory is a BuildUp    *
 *      project directory.                                                    *
 *****************************************************************************/
static bool check_for_buildup_files(struct directory_contents contents) {
    bool result = false;
    bool buildconf_found = false;
    bool big_p_parts_found = false;
    bool small_p_parts_found = false;
    bool big_t_tools_found = false;
    bool small_t_tools_found = false;

    // Check for a few of the typical files
    for (int i = 0; i <= contents.listing_length; i++) {
        // Check to see if the current name matches the buildconf.yaml file
        if (strcmp(contents.dir_contents[i], "buildconf.yaml") == 0) buildconf_found = true;
        if (strcmp(contents.dir_contents[i], "Parts.yaml") == 0) big_p_parts_found = true;
        if (strcmp(contents.dir_contents[i], "parts.yaml") == 0) small_p_parts_found = true;
        if (strcmp(contents.dir_contents[i], "Tools.yaml") == 0) big_t_tools_found = true;
        if (strcmp(contents.dir_contents[i], "tools.yaml") == 0) small_t_tools_found = true;
    }

    // Make sure everything was found that is required
    result = buildconf_found && (big_p_parts_found || small_p_parts_found) && (big_t_tools_found || small_t_tools_found);

    return result;
}

/******************************************************************************
 * sort_compare -- Allows an array of strings to be sorted in alpha-numeric   *
 *                 order.                                                     *
 * Parameters                                                                 *
 *      str1 -- Char pointer for the first string to compare for the sort.    *
 *      str2 -- Char pointer for the second string to compare for the sort.   *
 *                                                                            *
 * Returns                                                                    *
 *      The result of the strcmp call, 0 if strings are equal.                *
 *****************************************************************************/
int sort_compare(const void *str1, const void *str2) {
    char *const *pp1 = str1;
    char *const *pp2 = str2;

    return strcmp(*pp1, *pp2);
}

/******************************************************************************
 * directory_contents -- Collects the contents of the directory to pass back  *
 *                       to the caller. This does not filter any files out as *
 *                       it is expected that the caller will do that.         *
 * Parameters                                                                 *
 *      dir_path -- A char pointer with the path to the directory to list.    *
 *      sort -- A boolean that sets whether or not to spend time on sorting   *
 *              the entries.                                                  *
 *                                                                            *
 * Returns                                                                    *
 *      A struct holding the directory listing and additional information     *
 *      about the listing items.                                              *
 *****************************************************************************/
struct directory_contents list_dir_contents(char* dir_path, bool sort) {
    int size = -1;  // The size (length) of the array holding the directory contents
    struct directory_contents contents;  // The strings of the directory contents

    // Keep track of the size of this listing
    contents.listing_capacity = 32;

    // Open the directory and make sure there was not a problem
    open_dir = opendir(dir_path);
    if (open_dir != NULL) {
        // Step through the given directory's contents
        struct dirent *data = NULL;

        while ((data = readdir(open_dir)) != NULL) {
            // Ignore hidden files and directories
            if (data->d_name[0] == '.')
                continue;

            // Make sure to only list files that we care about and directories
            if (data->d_type != directory && !string_ends_with(data->d_name, ".md") && !string_ends_with(data->d_name, ".yaml") && !string_ends_with(data->d_name, ".png") && !string_ends_with(data->d_name, ".jpeg") && !string_ends_with(data->d_name, ".jpg")) {
                continue;
            }

            // If dir_contents has not be initialized, do that now. Otherwise expand it to hold the new values.
            if (size == -1) {
                size = 0;
                contents.dir_contents = (char**)calloc(sizeof(char*), contents.listing_capacity);
            }
            else {
                // Make room for the new directory entry
                size++;
                contents.listing_capacity = contents.listing_capacity * 2;
                contents.dir_contents = (char**)realloc(contents.dir_contents, contents.listing_capacity * sizeof(char*));
            }

            // Add the new directory entry to the directory listing array
            new_path = strdup(data->d_name);
            contents.dir_contents[size] = new_path;

            // Ensure the caller can tell if this entry is a directory or file
            if (data->d_type == directory) {
                contents.listing_type[size] = directory;
            }
            else {
                contents.listing_type[size] = file;
            }

            // Set which parent this entry belongs to
            contents.parent_dir[size] = -1;
        }

        // Store the size of the array in the first array element
        contents.listing_length = size;
    }
    else if (ENOENT == errno) {
        // Set the size to a value that lets the caller know the directory does not exist
        contents.listing_length = -2;
    }
    else {
        contents.listing_length = -1;
    }

    // If the caller has requested a sort, do that now
    if (sort) {
        int num_dirs = -1;  // Number of directories found
        int num_files = -1;  // Number of files fouund
        char** dirs_listing;  // Listing of directories
        char** files_listing;  // Listing of files
        size_t dir_capacity = 32;
        size_t file_capacity = 32;

        // Initialize the separate directory listing arrays
        dirs_listing = (char**)calloc(sizeof(char*), dir_capacity);
        files_listing = (char**)calloc(sizeof(char*), file_capacity);

        // Step through the entire listing and separate the directories from the files
        for (int i = 0; i <= contents.listing_length; i++) {
            // See if we have a directory or a file
            if (contents.listing_type[i] == directory) {
                // Handle initialization and resizing of the memory for the array
                if (num_dirs == -1) {
                    num_dirs = 0;
                }
                else {
                    num_dirs++;
                    dir_capacity = dir_capacity * 2;
                    dirs_listing = (char**)realloc(dirs_listing, dir_capacity * sizeof(char*));
                }

                // Store a copy of the directory name
                dirs_listing[num_dirs] = strdup(contents.dir_contents[i]);
            }
            else {
                // Handle initialization and resizing of the memory for the array
                if (num_files == -1) {
                    num_files = 0;
                }
                else {
                    num_files++;
                    file_capacity = file_capacity * 2;
                    files_listing = (char**)realloc(files_listing, file_capacity * sizeof(char*));
                }

                // Store a copy of the file name
                files_listing[num_files] = contents.dir_contents[i];
            }
        }

        // Sort the directory and file entries so they are in alphabetical order
        if (num_dirs >= 0)
            qsort(dirs_listing, num_dirs, sizeof(*dirs_listing), sort_compare);
        if (num_files >= 0)
            qsort(files_listing, num_files, sizeof(*files_listing), sort_compare);

        // Refill the dir_contents array with the sorted entries
        for (int i = 0; i <= num_dirs; i++) {
            contents.listing_type[i] = directory;
            contents.dir_contents[i] = strdup(dirs_listing[i]);
        }
        for (int i = num_dirs + 1; i <= num_files; i++) {
            contents.listing_type[i] = file;
            contents.dir_contents[i] = strdup(files_listing[i - num_dirs - 1]);
        }

        // Free the memory from our temporary arrays
        free(*dirs_listing);
        free(*files_listing);
    }

    // Make sure the directory resource is closed if we sucessfully got it open.
    if (open_dir) closedir(open_dir);

    return contents;
}

/******************************************************************************
 * list_project_dir -- List all of the directories and files that are present *
 *                     in the given project directory.                        *
 * Parameters                                                                 *
 *      dir_path -- The path to the project directory.                        *
 *                                                                            *
 * Returns                                                                    *
 *      A struct holding the project directory listing and additional         *
 *      information about the listing items.                                  *
 *****************************************************************************/
struct directory_contents list_project_dir(char* dir_path) {
    struct directory_contents contents;  // The strings of the directory contents
    struct directory_contents temp_contents;  // The strings of sub-directory contents

    // Get the sorted listing of the root project directory
    contents = list_dir_contents(dir_path, true);

    // This listing length may change, so save the current value for use later
    int list_len = contents.listing_length;

    // Insert any subdirectory contents into the main contents list
    for (int i = 0; i <= contents.listing_length; i++) {
        // Make sure we are dealing with a directory
        if (contents.listing_type[i] == directory) {
            /* Assemble the path of this sub-directory */
            char subdir_path[1024];
            subdir_path[0] = '\0';
            char* new_path = contents.dir_contents[0];
            strcat(subdir_path, dir_path);
            strcat(subdir_path, PATH_SEP);
            strcat(subdir_path, new_path);

            // List the subdirectory and insert its contents in the main listing
            temp_contents = list_dir_contents(subdir_path, true);

            int skip_ahead = 0;

            // Step through the sublisting in reverse order so that it will come out in forward order after inserts
            for (int j = temp_contents.listing_length; j >= 0; j--) {
                // Insert the subdirectory entry into the main listing
                contents.listing_length++;
                contents.listing_capacity = contents.listing_capacity * 2;
                contents.dir_contents = (char**)realloc(contents.dir_contents, contents.listing_capacity * sizeof(char*));

                // Move everything else down
                for(int k = contents.listing_length; k > i; k--) {
                    contents.dir_contents[k + 1] = contents.dir_contents[k];
                    contents.listing_type[k + 1] = contents.listing_type[k];
                    contents.parent_dir[k + 1] = contents.parent_dir[k];
                }

                skip_ahead++;

                // Save this subdirectory listing underneath the directory where it belongs
                contents.dir_contents[i + 1] = strdup(temp_contents.dir_contents[j]);

                // Save the directory that this listing belongs to
                contents.parent_dir[i + 1] = i;

                // Save the type of this file/directory
                contents.listing_type[i + 1] = temp_contents.listing_type[j];
            }

            i += skip_ahead;

            dir_free_list(temp_contents.dir_contents, temp_contents.listing_length);
        }
    }

    // Make sure that we are dealing with a BuildUp project directory
    if (!check_for_buildup_files(contents)) {
        contents.listing_length = -3;
    }

    // Check to see if there were any errors with the listing
    if (contents.listing_length < 0)
        return contents;

    return contents;
}
