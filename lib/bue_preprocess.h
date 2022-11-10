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
 * get_link_title -- Extracts the title from the given markdown link text.    *
 *                                                                            *
 * Parameters                                                                 *
 *      link_line -- The line of text containing the markdown link.           *
 *                                                                            *
 * Returns                                                                    *
 *      A string representing the extracted link title.                       *
 *****************************************************************************/
char* get_link_title(char* link_line) {
    char* md_title = strdup(link_line);

    // Attempt to find the link title
    md_title = strchr(md_title, '[');
    md_title = md_title + 1;
    cut_string(md_title, ']');

    return md_title;
}

/******************************************************************************
 * get_link_file -- Extracts the file name from the given markdown link text. *
 *                                                                            *
 * Parameters                                                                 *
 *      link_line -- The line of text containing the markdown link.           *
 *                                                                            *
 * Returns                                                                    *
 *      A string representing the extracted link file name.                   *
 *****************************************************************************/
char* get_link_file(char* link_line) {
    char* md_file = strdup(link_line);

    // Attempt to find the link file
    md_file = strchr(md_file, '(');
    md_file = md_file + 1;
    cut_string(md_file, ')');

    return md_file;
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
    // Check to make sure that the proper characters are present in the link text
    char* temp_char = strstr(line, "[");
    if (temp_char == NULL) {
        return false;
    }
    temp_char = strstr(line, "]");
    if (temp_char == NULL) {
        return false;
    }
    temp_char = strstr(line, "(");
    if (temp_char == NULL) {
        return false;
    }
    temp_char = strstr(line, ")");
    if (temp_char == NULL) {
        return false;
    }

    // Check to see if there is a step link
    char* link_tag = strstr(line, "{step}");
    if (link_tag != NULL) {
        return true;
    }

    return false;
}

/******************************************************************************
 * build_link_line -- Takes the pieces of a line containing a buildup link    *
 *                    tag and puts them together again to form a proper link  *
 *                    line.                                                   *
 *                                                                            *
 * Parameters                                                                 *
 *      new_line -- Pointer to the completed line.                            *
 *      before -- The start of the original line that needs to be preserved.  *
 *      md_title -- The title of the markdown link.                           *
 *      md_file -- The relative file path for the markdown link.              *
 *      after -- The end of the original line that needs to be preserved.     *
 *                                                                            *
 * Returns                                                                    *
 *      A pointer to the newly assembled link line.                           *
 *****************************************************************************/
char* build_link_line(char* new_line, char* before, char* md_title, char* md_file, char* after) {
    // Make sure there is enough space in memory for the newly assembled line
    new_line = realloc(new_line, str_size(before) + str_size(md_title) + str_size(md_file) + str_size(after) + 5);
    new_line[0] = '\0';

    // Assemble what was before the link, the link, and what was after the link
    strcat(new_line, before);
    build_link(new_line, md_title, md_file, false);
    strcat(new_line, after);

    return new_line;
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
    // Save the parts of the line before and after the step link
    char* before = strdup(line);
    cut_string(before, '[');
    char* after = strdup(line);
    after = strrchr(after, '}');
    after++;

    // Make sure that the converted line has a spot in memory
    char* new_line = NULL;

    // Make sure that we really did get a step link
    if (line != NULL) {
        // Pull the title and the file from the given line
        char* md_title = get_link_title(line);
        char* md_file = get_link_file(line);

        // Construct the path to the linked file
        char* path_start = strdup(base_path);
        cut_string_last(path_start, PATH_SEP[0]);
        if (strlen(base_path) < strlen(path_start) + strlen(md_file)) {
            path_start = realloc(path_start, str_size(path_start) + str_size(md_file) + str_size(PATH_SEP) + 1);
        }
        strcat(path_start, PATH_SEP);
        strcat(path_start, md_file);

        // If there is a linked markdown file, pull the title from that
        if (md_title[0] == '.') {
            // Open the documentation file and make sure that the file opened properly
            FILE* doc_file;
            doc_file = fopen(path_start, "r");
            if (doc_file == NULL) {
                printf("Could not open the required file: %s.\n", path_start);
            }
            else {
                // Get a line from the file and see if it is a title
                char line_temp[10000];
                line_temp[0] = '\0';

                // Handle converting the md file extension to html
                if (strcmp(strrchr(md_file, '.'), ".md") == 0)
                    md_file = replace_file_extension(md_file, ".html");

                // Step through the lines of the file until we find a top level title header
                bool title_found = false;
                while (fgets(line_temp, sizeof(line_temp), doc_file) != NULL) {
                    // If we have found a title line, extract the title
                    if (line_temp[0] == '#') {
                        // We want only the text of the title, and not the hash or newline
                        strip_title_text(line_temp);

                        // Build the updated link with the file name
                        new_line = build_link_line(new_line, before, strdup(line_temp), md_file, after);

                        // We do not need to keep looking
                        title_found = true;
                        break;
                    }
                }

                // If no title was found, provide some warning
                if (!title_found)
                    printf("No title found in file: %s", path_start);
            }

            // Make sure that we release the resources associated with the doc file
            fclose(doc_file);
        }
        else {
            // Build the updated link with the file name
            new_line = build_link_line(new_line, before, line, md_file, after);
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
char* preprocess(char* buildup_md, char* base_path) {
    char* line;
    char* old_md = strdup(buildup_md);
    char* new_md = malloc(sizeof(char));
    new_md[0] = '\0';

    // Get the first line
    line = strtok(old_md, NEWLINE);

    // Step through each line of the content
    while (line != NULL) {
        // Save the current size of the processed markdown string
        size_t new_md_size = str_size(new_md);

        // Handle the step link
        if (check_for_step_link(line)) {
            // Get the new step link processed from the line
            char* new_step_link = handle_step_link(line, base_path);

            // Resize the markdown variable appropriately
            new_md = realloc(new_md, new_md_size + str_size(new_step_link) + str_size(NEWLINE) + 1);

            // Add the new link to the text
            strcat(new_md, new_step_link);
            strcat(new_md, NEWLINE);
        }
        else {
            // Resize the new markdown string to hold the new data
            new_md = realloc(new_md, new_md_size + str_size(line) + str_size(NEWLINE) + 1);

            // Add the unchanged line back to the text
            strcat(new_md, line);
            strcat(new_md, NEWLINE);
        }

        // Get the next line
        line = strtok(NULL, NEWLINE);
   }

    return new_md;
}