/******************************************************************************
 * bue_preprocess-- Converts any BuildUp-specific tags to markdown, including *
 *          searching through and collating data from all files.              *
 *                                                                            *
 * Author: 7B Industries                                                      *
 * License: Apache 2.0                                                        *
 *                                                                            *
 * ***************************************************************************/

/******************************************************************************
 * build_link -- Builds a markdown link given a title and file name. Can      *
 *               create an image link if is_image is true.                    *
 *                                                                            *
 * Parameters                                                                 *
 *      dest -- char pointer that the completed link text will be stored in.  *
 *      md_title -- The text to use in the title portion of the link.         *
 *      md_file -- The text to use in the file portioin of the link.          *
 *      is_image -- Whether or not to turn this into an image link.           *
 *                                                                            *
 * Returns                                                                    *
 *      Nothing                                                               *
 *****************************************************************************/
void build_link(char* dest, char* md_title, char* md_file, bool is_image) {
    // Turn this into an image link
    if (is_image)
        strcat(dest, "!");

    // Assemble the link markdown
    strcat(dest, "[");
    strcat(dest, md_title);
    strcat(dest, "]");
    strcat(dest, "(");
    strcat(dest, md_file);
    strcat(dest, ")");
}

/******************************************************************************
 * strip_title_text -- Removes the hash(es) and extra spaces from a section   *
 *                     header title.                                          *
 *                                                                            *
 * Parameters                                                                 *
 *      title -- A charcter pointer holding the full markdown section title.  *
 *                                                                            *
 * Returns                                                                    *
 *      Nothing                                                               *
 *****************************************************************************/
void strip_title_text(char* title) {
    // Remove the has character from the title
    memmove(title, title+1, strlen(title));

    // Remove any leading whitespace
    while (title[0] == ' ')
        memmove(title, title+1, strlen(title));

    // Remove the newline character, if it exists
    if (title[strlen(title) - 1] == '\n')
        title[strlen(title) - 1] = '\0';
}

/******************************************************************************
 * check_for_step_link -- Given a line of markdown, determines whether or not *
 *                        that line contains a step link.                     *
 *                                                                            *
 * Parameters                                                                 *
 *      line -- A character pointer representing the line of text to check    *
 *              for the step link.                                            *
 *                                                                            *
 * Returns                                                                    *
 *      A boolean representing whether or not a line contains a step link.    *
 *****************************************************************************/
bool check_for_step_link(char* line) {
    // Check to see if there is a step link
    char* link_tag = strstr(line, "{step}");
    if (link_tag != NULL) {
        return true;
    }

    return false;
}

/******************************************************************************
 * handle_step_link -- Given a line that contains a step link, returns a      *
 *                     properly constructed step link with the title filled   *
 *                     in.                                                    *
 *                                                                            *
 * Parameters                                                                 *
 *      line -- Character pointer representing the markdown line to transform.*
 *      base_path -- Path to the current project so that a referenced file's  *
 *                   title can be pulled from its contents.                   *
 *                                                                            *
 * Returns                                                                    *
 *      A character pointer for the transformed line, with the title properly *
 *      filled in.                                                            *
 *****************************************************************************/
char* handle_step_link(char* line, char* base_path) {
    // Make sure that this line contains a step link
    // char* link_tag = strstr(line, "[");
    char* new_line = malloc(sizeof(line) + 250);
    new_line[0] = '\0';

    // Make sure that we really did get a step link
    if (line != NULL) {
        // Attempt to find the link title
        char* md_title = strdup(line);
        md_title = strchr(md_title, '[');
        md_title = md_title + 1;
        cut_string(md_title, ']');

        // Attempt to find the link file
        char* md_file = strdup(line);
        md_file = strchr(md_file, '(');
        md_file = md_file + 1;
        cut_string(md_file, ')');

        // Construct the path to the linked file
        char* path_start = strdup(base_path);
        cut_string_last(path_start, PATH_SEP[0]);
        if (strlen(base_path) < strlen(path_start) + strlen(md_file)) {
            path_start = realloc(path_start, sizeof(path_start) + sizeof(md_file) + sizeof(PATH_SEP) + 1);
        }
        strcat(path_start, PATH_SEP);
        strcat(path_start, md_file);

        // If there is a linked markdown file, pull the title from that
        if (md_title[0] == '.') {
            // Open the documentation file and make sure that the file opened properly
            FILE* doc_file;
            doc_file = fopen(path_start, "r");
            if (doc_file == NULL) {
                printf("Could not open the required file: %s.\n", md_file);
            }
            else {
                // Get a line from the file and see if it is a title
                char* line;
                char line_temp[1000];
                line = fgets(line_temp, sizeof(line_temp), doc_file);
                if (line == NULL) {
                    printf("The given file was not readable.\n");
                }
                else {
                    // Check to see if there is a title line
                    if (line[0] == '#') {
                        // We want only the text of the title, and not the hash or newline
                        strip_title_text(line);

                        // Build the updated link with the file name
                        build_link(new_line, line, md_file, false);
                    }
                    else {
                        line = fgets(line_temp, sizeof(line_temp), doc_file);
                        if (line != NULL && line[0] == '#') {
                            // We want only the text of the title, and not the hash or newline
                            strip_title_text(line);

                            // Build the updated link with the file name
                            build_link(new_line, line, md_file, false);
                        }
                    }
                }
            }

            // Make sure that we release the resources associated with the doc file
            fclose(doc_file);
        }
        else {
            // Build the updated link with the file name
            build_link(new_line, md_title, md_file, false);
        }
    }

    return new_line;
}

/******************************************************************************
 * preprocess -- The primary function that is called that delagates to other  *
 *               functions to handle the different types of BuildUp tags that *
 *               can be used.                                                 *
 *                                                                            *
 * Parameters                                                                 *
 *      buildup_md -- Character pointer holding the markdown with BuildUp     *
 *                    tags embedded within it.                                *
 *                                                                            *
 * Returns                                                                    *
 *      A character pointer to a string with all of the BuildUp tags replaced *
 *      with their collated markdown data.                                    *
 *****************************************************************************/
void preprocess(char* new_md, char* buildup_md, char* base_path) {
    char* line;
    char* old_md = strdup(buildup_md);
    new_md[0] = '\0';

    // Get the first line
    line = strtok(old_md, NEWLINE);

    // Step through each line of the content
    while (line != NULL) {
        // Handle the step link
        if (check_for_step_link(line)) {
            strcat(new_md, handle_step_link(line, base_path));
            strcat(new_md, NEWLINE);
        }
        else {
            strcat(new_md, line);
            strcat(new_md, NEWLINE);
        }

        // Get the next line
        line = strtok(NULL, NEWLINE);
   }
}