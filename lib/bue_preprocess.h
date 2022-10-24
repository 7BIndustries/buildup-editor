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

bool check_for_step_link(char* line) {
    // Check to see if there is a step link
    char* link_tag = strstr(line, "{step}");
    if (link_tag != NULL) {
        return true;
    }

    return false;
}

char* handle_step_link(char* line, char* base_path) {
    // Make sure that this line contains a step link
    char* link_tag = strstr(line, "[");
    char* new_line = malloc(sizeof(line) + 250);
    new_line[0] = '\0';

    // Make sure that we really did get a step link
    if (link_tag != NULL) {
        // Title of the link
        char md_title[200];
        md_title[0] = '\0';

        // File that the link points to
        char md_file[200];
        md_file[0] = '\0';

        // Attempt to deconstruct the step link structure
        sscanf(link_tag, "[%s](%s){step}", md_title, md_file);

        // If there is a linked markdown file, pull the title from that
        if (md_title[0] == '.') {
            printf("%s\n", base_path);
            // Open the documentation file and make sure that the file opened properly
            FILE* doc_file;
            doc_file = fopen(base_path, "r");
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