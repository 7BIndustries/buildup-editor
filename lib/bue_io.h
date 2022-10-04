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

/* Define the maximum number of directories and files in a filesystem listing layer */
#define MAX_NUM_DIRS 50
#define MAX_NUM_FILES 250

DIR* open_dir;  // DIRENT struct holding information on the open directory

// Holds the types of files we are working with
enum file_types {directory = 4, file = 8};

// Holds the types of errors that we can see when working with directories
enum dir_errors {no_error = 0, general_error = 1, does_not_exist = 2, not_a_buildup_directory = 3, dir_structure_too_deep = 4, exceeded_max_dirs = 5, exceeded_max_files = 6};

struct file_entry {
    char* name;
    char* path;
    int selected;
    int prev_selected;
};

typedef struct directory_contents {
    char* name;
    int number_directories;
    int number_files;
    int selected;
    int prev_selected;
    int error;
    struct directory_contents* dirs[MAX_NUM_DIRS];
    struct file_entry files[MAX_NUM_FILES];
} dir_contents;

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
    for (i = 0; i < size; i++)
        free(list[i]);
    // free(list);
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
static bool check_for_buildup_files(dir_contents contents) {
    bool result = false;
    bool buildconf_found = false;
    bool big_p_parts_found = false;
    bool small_p_parts_found = false;
    bool big_t_tools_found = false;
    bool small_t_tools_found = false;

    // Check for a few of the typical files
    for (int i = 0; i < contents.number_files; i++) {
        // Check to see if the current name matches the buildconf.yaml file
        if (strcmp(contents.files[i].name, "buildconf.yaml") == 0) buildconf_found = true;
        if (strcmp(contents.files[i].name, "Parts.yaml") == 0) big_p_parts_found = true;
        if (strcmp(contents.files[i].name, "parts.yaml") == 0) small_p_parts_found = true;
        if (strcmp(contents.files[i].name, "Tools.yaml") == 0) big_t_tools_found = true;
        if (strcmp(contents.files[i].name, "tools.yaml") == 0) small_t_tools_found = true;
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
 * list_dir_contents -- Collects the contents of the directory to pass back   *
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
dir_contents list_dir_contents(char* dir_path, bool sort) {
    dir_contents contents;  // The strings of the directory contents

    // Initialize the variables that track the number of directories and the number of files
    contents.number_directories = -1;
    contents.number_files = -1;

    // Start out with no error
    contents.error = no_error;

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

            // Determine whether we are working with a directory or a file
            if (data->d_type == directory) {
                // Refuse to add any directories over the maximum amount
                if (contents.number_directories >= MAX_NUM_DIRS) {
                    contents.error = exceeded_max_dirs;
                }
                else {
                    // Keep track of the number of directories, starting the count at 1
                    if (contents.number_directories == -1) {
                        contents.number_directories = 1;
                    }
                    else {
                        contents.number_directories++;
                    }

                    // Keep track of this directory's information as a new array element
                    contents.dirs[contents.number_directories - 1] = (dir_contents *)malloc(sizeof(dir_contents));
                    contents.dirs[contents.number_directories - 1]->name = strdup(data->d_name);
                    contents.dirs[contents.number_directories - 1]->selected = 0;
                    contents.dirs[contents.number_directories - 1]->prev_selected = 0;
                }
            }
            else {
                // Refuse to add any files over the maximum amount
                if (contents.number_files >= MAX_NUM_FILES) {
                    contents.error = exceeded_max_files;
                }
                else {
                    // Keep track of the number of files, starting the count at 1
                    if (contents.number_files == -1) {
                        contents.number_files = 1;
                    }
                    else {
                        contents.number_files++;
                    }

                    // Keep track of this file name and path
                    char new_path[strlen(dir_path) + strlen(PATH_SEP) + strlen(data->d_name) + 1];
                    new_path[0] = '\0';
                    strcat(new_path, dir_path);
                    strcat(new_path, PATH_SEP);
                    strcat(new_path, data->d_name);
                    struct file_entry ent = {.name=strdup(data->d_name), .selected=0, .prev_selected=0, .path=strdup(new_path)};
                    contents.files[contents.number_files - 1] = ent;
                }
            }
        }
    }
    else if (ENOENT == errno) {
        // Set the size to a value that lets the caller know the directory does not exist
        contents.error = does_not_exist;
    }
    else {
        contents.error = general_error;
    }

    // If the caller has requested a sort, do that now
    if (sort) {
        printf("Sorting...\n");
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
dir_contents list_project_dir(char* dir_path) {
    dir_contents contents;  // The strings of the directory contents
    dir_contents temp_level_1;  // The first sub-directory's contents
    dir_contents temp_level_2;  // The second sub-directory's contents
    dir_contents temp_level_3;  // The third sub-directory's contents
    dir_contents temp_level_4;  // The fourth sub-directory's contents
    dir_contents temp_level_5;  // The fifth sub-directory's contents

    // Get the sorted listing of the root project directory
    contents = list_dir_contents(dir_path, true);

    // Step through all of the directories in the root project directory and get their listings
    for (int i = 0; i < contents.number_directories; i++) {
        // Assemble the subdirectory path
        char sub_path[strlen(dir_path) + strlen(PATH_SEP) + strlen(contents.dirs[i]->name) + 1];
        sub_path[0] = '\0';
        strcat(sub_path, dir_path);
        strcat(sub_path, PATH_SEP);
        strcat(sub_path, contents.dirs[i]->name);

        // List the subdirectory's contents
        temp_level_1 = list_dir_contents(sub_path, true);

        // If there is an error, go ahead and return the current contents
        if (temp_level_1.error > 0 && contents.error == 0) {
            contents.error = temp_level_1.error;
        }

        // Add the subdirectory's contents to the main listing of contents
        contents.dirs[i]->number_directories = temp_level_1.number_directories;
        contents.dirs[i]->number_files = temp_level_1.number_files;
        if (temp_level_1.number_directories > 0) {
            for (int j = 0; j < temp_level_1.number_directories; j++) {
                contents.dirs[i]->dirs[j] = temp_level_1.dirs[j];

                // List the second level directory's contents
                char sub_path_2[strlen(sub_path) + strlen(PATH_SEP) + strlen(contents.dirs[i]->dirs[j]->name) + 1];
                sub_path_2[0] = '\0';
                strcat(sub_path_2, sub_path);
                strcat(sub_path_2, PATH_SEP);
                strcat(sub_path_2, contents.dirs[i]->dirs[j]->name);

                // List the second level subdirectory
                temp_level_2 = list_dir_contents(sub_path_2, true);

                // If there is an error, go ahead and return the current contents
                if (temp_level_2.error > 0 && contents.error == 0) {
                    contents.error = temp_level_2.error;
                }

                // Add the second level subdirectory's contents to the main listing
                contents.dirs[i]->dirs[j]->number_directories = temp_level_2.number_directories;
                contents.dirs[i]->dirs[j]->number_files = temp_level_2.number_files;
                if (temp_level_2.number_directories > 0) {
                    for (int k = 0; k < temp_level_2.number_directories; k++) {
                        contents.dirs[i]->dirs[j]->dirs[k] = temp_level_2.dirs[k];

                        // List the third level directory's contents
                        char sub_path_3[strlen(sub_path_2) + strlen(PATH_SEP) + strlen(contents.dirs[i]->dirs[j]->dirs[k]->name) + 1];
                        sub_path_3[0] = '\0';
                        strcat(sub_path_3, sub_path_2);
                        strcat(sub_path_3, PATH_SEP);
                        strcat(sub_path_3, contents.dirs[i]->dirs[j]->dirs[k]->name);

                        // List the third level subdirectory
                        temp_level_3 = list_dir_contents(sub_path_3, true);

                        // If there is an error, go ahead and return the current contents
                        if (temp_level_3.error > 0 && contents.error == 0) {
                            contents.error = temp_level_3.error;
                        }

                        // Add the third level subdirectory's contents to the main listing
                        contents.dirs[i]->dirs[j]->dirs[k]->number_directories = temp_level_3.number_directories;
                        contents.dirs[i]->dirs[j]->dirs[k]->number_files = temp_level_3.number_files;
                        if (temp_level_3.number_directories > 0) {
                            for (int l = 0; l < temp_level_3.number_directories; l++) {
                                contents.dirs[i]->dirs[j]->dirs[k]->dirs[l] = temp_level_3.dirs[k];

                                // Create the path to the forth level directory's contents
                                char sub_path_4[strlen(sub_path_3) + strlen(PATH_SEP) + strlen(contents.dirs[i]->dirs[j]->dirs[k]->dirs[l]->name) + 1];
                                sub_path_4[0] = '\0';
                                strcat(sub_path_4, sub_path_3);
                                strcat(sub_path_4, PATH_SEP);
                                strcat(sub_path_4, contents.dirs[i]->dirs[j]->dirs[k]->dirs[l]->name);

                                // List the fourth level subdirectory
                                temp_level_4 = list_dir_contents(sub_path_4, true);

                                // If there is an error, go ahead and return the current contents
                                if (temp_level_4.error > 0 && contents.error == 0) {
                                    contents.error = temp_level_4.error;
                                }

                                // Add the fourth level subdirectory's contents to the main listing
                                contents.dirs[i]->dirs[j]->dirs[k]->dirs[l]->number_directories = temp_level_4.number_directories;
                                contents.dirs[i]->dirs[j]->dirs[k]->dirs[l]->number_files = temp_level_4.number_files;
                                if (temp_level_4.number_directories > 0) {
                                    for (int m = 0; m < temp_level_4.number_directories; m++) {
                                        contents.dirs[i]->dirs[j]->dirs[k]->dirs[l]->dirs[m] = temp_level_4.dirs[m];

                                        // Create the path to the fifth level directory's contents
                                        char sub_path_5[strlen(sub_path_4) + strlen(PATH_SEP) + strlen(contents.dirs[i]->dirs[j]->dirs[k]->dirs[l]->dirs[m]->name) + 1];
                                        sub_path_5[0] = '\0';
                                        strcat(sub_path_5, sub_path_4);
                                        strcat(sub_path_5, PATH_SEP);
                                        strcat(sub_path_5, contents.dirs[i]->dirs[j]->dirs[k]->dirs[l]->dirs[m]->name);

                                        // List the fifth level subdirectory
                                        temp_level_5 = list_dir_contents(sub_path_5, true);

                                        // If there is an error, go ahead and set it in the current contents
                                        if (temp_level_5.error > 0 && contents.error == 0) {
                                            contents.error = temp_level_5.error;
                                        }

                                        // Add the fifth level subdirectory's contents to the main listing
                                        contents.dirs[i]->dirs[j]->dirs[k]->dirs[l]->dirs[m]->number_directories = temp_level_5.number_directories;
                                        contents.dirs[i]->dirs[j]->dirs[k]->dirs[l]->dirs[m]->number_files = temp_level_5.number_files;
                                        if (temp_level_5.number_directories > 0) {
                                            // Add the directories of this sublisting to the main listing
                                            for (int n = 0; n < temp_level_5.number_directories; n++) {
                                                contents.dirs[i]->dirs[j]->dirs[k]->dirs[l]->dirs[m]->dirs[n] = temp_level_5.dirs[n];
                                            }
                                        }
                                        if (temp_level_5.number_files > 0) {
                                            for (int n = 0; n < temp_level_5.number_files; n++) {
                                                contents.dirs[i]->dirs[j]->dirs[k]->dirs[l]->dirs[m]->files[n] = temp_level_5.files[n];
                                            }
                                        }
                                    }
                                }
                                if (temp_level_4.number_files > 0) {
                                    for (int m = 0; m < temp_level_4.number_files; m++) {
                                        contents.dirs[i]->dirs[j]->dirs[k]->dirs[l]->files[m] = temp_level_4.files[m];
                                    }
                                }
                            }
                        }
                        if (temp_level_3.number_files > 0) {
                            for (int l = 0; l < temp_level_3.number_files; l++) {
                                contents.dirs[i]->dirs[j]->dirs[k]->files[l] = temp_level_3.files[l];
                            }
                        }
                    }
                }
                if (temp_level_2.number_files > 0) {
                    for (int k = 0; k < temp_level_2.number_files; k++) {
                        contents.dirs[i]->dirs[j]->files[k] = temp_level_2.files[k];
                    }
                }
            }
        }
        if (temp_level_1.number_files > 0) {
            for (int j = 0; j < temp_level_1.number_files; j++) {
                contents.dirs[i]->files[j] = temp_level_1.files[j];
            }
        }
    }

    // Make sure that we are dealing with a BuildUp project directory
    if (!check_for_buildup_files(contents)) {
        contents.error = not_a_buildup_directory;
    }

    return contents;
}
