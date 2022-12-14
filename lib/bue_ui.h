/******************************************************************************
 * bue_ui-- Sets up the Nuklear GUI for display to the user. Called once      *
 *          per frame. Encapsulates the UI logic so that it is usable         *
 *          across OSes.                                                      *
 *                                                                            *
 * Author: 7B Industries                                                      *
 * License: Apache 2.0                                                        *
 *                                                                            *
 * ***************************************************************************/

#include "bue_io.h"
#include "bue_preprocess.h"

// #define INCLUDE_STYLE
// #ifdef INCLUDE_STYLE
//set_style(ctx, THEME_WHITE);
// set_style(ctx, THEME_RED);
//set_style(ctx, THEME_BLUE);
//set_style(ctx, THEME_DARK);
// #endif

// Constants that hold mostly array lengths
#define ERROR_MSG_MAX_LENGTH 1000
#define FILE_PATH_MAX_LENGTH 1000

typedef struct markdown_state markdown_state;
struct markdown_state {
    int prev_markdown_len;
    bool is_dirty;
    char* dirty_path;
};

char* selected_path = NULL;  // Tracks the currently selected path so see when a change occurs and to know where to save
char file_path[FILE_PATH_MAX_LENGTH];  // Holds the selected file/folder path
char* html_preview = NULL;  // Converted HTML text based on the markdowns
struct directory_contents contents;  // Listed directory contents
int ret;  // The return code for the markdown to HTML conversions
struct nk_rect bounds;  // The bounds of the popup dialog
bool step_link_page_dialog_active = false;  // Tracks whether or not the step link page dialog should be opened
bool image_link_dialog_active = false;  // Tracks whether or not the image dialog should be opened
bool save_confirm_dialog_active = false;  // Tracks whether or not the save confirmation dialog should be opened
bool about_dialog_active = false;  // Tracks whether or not the About dialog should be opened
bool error_popup_active = false;  // Tracks whether or not the error popup should be displayed
char error_popup_message[ERROR_MSG_MAX_LENGTH];  // The message that will be displayed in the error popup
struct nk_text_edit tedit_state;  // The struct that holds the state of the BuildUp text editor
markdown_state bu_state;  // Tracks the state of the BuildUp markdown editor
clipboard_c *cb;  // Used to copy data to/from the clipboard

/* Error messages for insert dialogs*/
char step_link_insert_msg[200] = {'\0'};
char insert_image_msg[200] = {'\0'};

// md4c flags for converting markdown to HTML
static unsigned parser_flags = 0;
static unsigned renderer_flags = MD_HTML_FLAG_DEBUG;

// Popup dialog control flags
static int show_open_project = nk_false;

/*
 * BuildUp tag dialog variables.
 */
char step_link_link_file[1000];  // The page file to link to
char step_link_link_text[1000];  // The link text field
char image_alt_text[1000];  // The image alternate text field
char image_path[1000];  // The path to the image file

// X11 window representation
// Linux only
typedef struct XWindow XWindow;
struct XWindow {
    Display *dpy;
    Window root;
    Visual *vis;
    Colormap cmap;
    XWindowAttributes attr;
    XSetWindowAttributes swa;
    Window win;
    int screen;
    XFont *font;
    unsigned int width;
    unsigned int height;
    Atom wm_delete_window;
};

/*
 * struct of user data that can be passed by the markdown parser.
 * This is mostly just a placeholder right now.
 */
struct md_userdata {
    char* name;
};

/******************************************************************************
 * process_output -- Call back function for the Markdown to HTML processor.   *
 *                                                                            *
 * Parameters                                                                 *
 *      text - The markdown that has been converted to HTML                   *
 *      size - The size of the converted HTML string                          *
 *      userdata - A struct that holds custom data that is passed as-is       *
 *                                                                            *
 * Returns                                                                    *
 *      N/A                                                                   *
 *****************************************************************************/
static void process_output(const MD_CHAR* text, MD_SIZE size, void* userdata)
{
    // To get rid of the unused variable warning for userdata
    if (userdata == NULL)
        printf("No user data passed for markdown processor.\n");

    // Make sure the correct amount of memory is allocated for the HTML preview
    if (html_preview == NULL) {
        html_preview = (char*)realloc(html_preview, size + 1);
        html_preview[0] = '\0';
    }
    else {
        html_preview = (char*)realloc(html_preview, strlen(html_preview) * sizeof(char) + size + 1);
    }

    strncat(html_preview, text, size);
}

/*
 * Handles some initialization functions of the Nuklear based UI.
 */
struct nk_context* ui_init(struct XWindow xw) {
    struct nk_context *ctx;

    // GUI
    xw.font = nk_xfont_create(xw.dpy, "fixed");
    ctx = nk_xlib_init(xw.font, xw.dpy, xw.screen, xw.win, xw.width, xw.height);

    // Initialize the text editor state
    nk_textedit_init_default(&tedit_state);

    // Make sure that the editor string buffer is zero'd out
    memset(tedit_state.string.buffer.memory.ptr, 0, tedit_state.string.buffer.memory.size);

    // Start the markdown editor's state off
    bu_state.is_dirty = false;
    bu_state.prev_markdown_len = 0;
    bu_state.dirty_path = NULL;

    // Initialize all the BuildUp tag dialog variables
    step_link_link_file[0] = '\0';
    strcat(step_link_link_file, "file_to_link_to.md");
    step_link_link_text[0] = '.';
    step_link_link_text[1] = '\0';

    // Set up the clipboard
    cb = clipboard_new(NULL);

    return ctx;
}

/******************************************************************************
 * set_error_popup -- Sets the error popup up for use by setting the message  *
 *                    and then setting the flag to display the popup.         *
 *                                                                            *
 * Parameters                                                                 *
 *      message -- A string holding the message to display to the user.       *
 *                                                                            *
 * Returns                                                                    *
 *      Nothing                                                               *
 *****************************************************************************/
void set_error_popup(char* message) {
    // Make sure that the message does not overflow the string that is already defined for it
    if (strlen(message) >= ERROR_MSG_MAX_LENGTH) {
        printf("The error message was too long and will be truncated: %s", message);

        // Truncate the message
        message[ERROR_MSG_MAX_LENGTH - 1] = '\0';
    }

    // Reset the message string
    error_popup_message[0] = '\0';
    strcat(error_popup_message, message);

    // Make sure that the dialog will be displayed
    error_popup_active = true;
}

/******************************************************************************
 * clear_html_preview -- Clears the HTML preview variable, and thus the view. *
 *                                                                            *
 * Parameters                                                                 *
 *      None                                                                  *
 *                                                                            *
 * Returns                                                                    *
 *      Nothing                                                               *
 *****************************************************************************/
void clear_html_preview() {
    // Only free the memory if there is something to free
    if (html_preview != NULL) {
        // Start over again with the html_preview
        free(html_preview);
    }

    // Start over again with the html_preview
    html_preview = NULL;
}

/******************************************************************************
 * update_preview -- Handles the work of updating the HTML preview.           *
 *                                                                            *
 * Parameters                                                                 *
 *      None                                                                  *
 *                                                                            *
 * Returns                                                                    *
 *      Nothing                                                               *
 *****************************************************************************/
void update_html_preview() {
    // Reset the HTML preview text for the new conversion text
    clear_html_preview();

    // Placeholder for user data that can be passed around
    struct md_userdata userdata = {.name="Name"};

    // Preprocess the string to handle all the BuildUp-specific tags
    char* processed_str = preprocess((char*)tedit_state.string.buffer.memory.ptr, selected_path);

    // Convert the markdown to HTML
    ret = md_html(processed_str, (MD_SIZE)str_size(processed_str), process_output, (void*) &userdata, parser_flags, renderer_flags);
    if (ret == -1) {
        set_error_popup("The markdown failed to parse.");
    }
}

/******************************************************************************
 * save_selected_file -- Saves the text in the markdown editor to the open    *
 *                       file.                                                *
 *                                                                            *
 * Parameters                                                                 *
 *      None                                                                  *
 *                                                                            *
 * Returns                                                                    *
 *      Nothing                                                               *
 *****************************************************************************/
void save_selected_file() {
    // If no file is selected, there is no reason to save this file
    if (selected_path == NULL) {
        // Let the user know that no file will be saved
        set_error_popup("There is no selected file to save.");
        return;
    }


    // If there is a selected file path
    if (bu_state.dirty_path != NULL) {
        // Open the dirty file path for writing and write the editor buffer
        FILE* doc_file;
        doc_file = fopen(bu_state.dirty_path, "w");
        if (doc_file == NULL) {
            set_error_popup("There was an error saving the open file.");
        }
        else {
            // Save the editor text to the file
            int res = fputs(tedit_state.string.buffer.memory.ptr, doc_file);
            if (res == EOF) {
                set_error_popup("There was an error saving the file.");
            }
            else {
                bu_state.is_dirty = false;
                bu_state.dirty_path = NULL;
            }
        }
        fclose(doc_file);
    }
}

/******************************************************************************
 * deselect_all_files -- Helper function that deselects all the files in a    *
 *                       given directory.                                     *
 *                                                                            *
 * Parameters                                                                 *
 *      dir -- directory_contents struct holding the files to be deselected.  *
 *                                                                            *
 * Returns                                                                    *
 *      Nothing                                                               *
 *****************************************************************************/
void deselect_all_files(struct directory_contents* dir) {
    for (int i = 0; i < dir->number_files; i++) {
        dir->files[i].selected = nk_false;
        dir->files[i].prev_selected = nk_false;
    }
}

/******************************************************************************
 * deselect_entire_tree -- Toggles the selection flag for all the files in    *
 *                         directory listing to off, so that just the current *
 *                         selection will be highlighted.                     *
 *                                                                            *
 * Parameters                                                                 *
 *      None                                                                  *
 *                                                                            *
 * Returns                                                                    *
 *      Nothing                                                               *
 *****************************************************************************/
void deselect_entire_tree() {
    // Deselect any level 0 files
    deselect_all_files(&contents);

    // Deselect any level 1 files
    for (int i = 0; i < contents.number_directories; i++) {
        deselect_all_files(contents.dirs[i]);

        // Deselect any level 2 files
        for (int j = 0; j < contents.dirs[i]->number_directories; j++) {
            deselect_all_files(contents.dirs[i]->dirs[j]);

            // Deselect any level 3 files
            for (int k = 0; k < contents.dirs[i]->dirs[j]->number_directories; k++) {
                deselect_all_files(contents.dirs[i]->dirs[j]->dirs[k]);

                // Deselect any level 4 files
                for (int l = 0; l < contents.dirs[i]->dirs[j]->dirs[k]->number_directories; l++) {
                    deselect_all_files(contents.dirs[i]->dirs[j]->dirs[k]->dirs[l]);

                    // Deselect any level 5 files
                    for (int m = 0; m < contents.dirs[i]->dirs[j]->dirs[k]->dirs[l]->number_directories; m++) {
                        deselect_all_files(contents.dirs[i]->dirs[j]->dirs[k]->dirs[l]->dirs[m]);
                    }
                }
            }
        }
    }
}

/******************************************************************************
 * clear_editor -- Clears the markdown editor of all existing text.           *
 *                                                                            *
 * Parameters                                                                 *
 *      None                                                                  *
 *                                                                            *
 * Returns                                                                    *
 *      Nothing                                                               *
 *****************************************************************************/
void clear_editor() {
    // Clear the previous contents of the markdown editor
    nk_textedit_select_all(&tedit_state);
    nk_textedit_delete_selection(&tedit_state);
}

/******************************************************************************
 * cut_copy_to_clipboard -- Allows the cut and copy commands to put text in   *
 *                          the system's clipboard.                           *
 *                                                                            *
 * Parameters                                                                 *
 *      None                                                                  *
 *                                                                            *
 * Returns                                                                    *
 *      Nothing                                                               *
 *****************************************************************************/
void cut_copy_to_clipboard() {
    int glyph_len;
    nk_rune unicode;
    const char *text;
    int start = tedit_state.select_start;
    int ending = tedit_state.select_end;

    // Get the selected text from the markdown editor control
    int begin = NK_MIN(start, ending);
    int end = NK_MAX(start, ending);
    text = nk_str_at_const(&tedit_state.string, begin, &unicode, &glyph_len);
    text = strndup(text, end - begin);

    // Make sure we have data to copy to the clipboard
    if (text) {
        // Copy the markdown editor's selected text to the system clipboard
        clipboard_set_text(cb, text);
    }
}

/******************************************************************************
 * paste_from_clipboard -- Takes the text that is in the system clipboard and *
 *                         puts it at the current cursor position in the      *
 *                         markdown editor.                                   *
 *                                                                            *
 * Parameters                                                                 *
 *      None                                                                  *
 *                                                                            *
 * Returns                                                                    *
 *      Nothing                                                               *
 *****************************************************************************/
void paste_from_clipboard() {
    char* text = clipboard_text(cb);

    // Add the clipboard text into the markdown editor
    nk_textedit_text(&tedit_state, text, strlen(text));
}

/******************************************************************************
 * check_selected_tree_item -- Handles the logic for when a directory tree    *
 *                             item is selected.                              *
 *                                                                            *
 * Parameters                                                                 *
 *      contents -- A pointer to the struct of directory contents to search   *
 *                  for a selected tree item.                                 *
 *                                                                            *
 * Returns                                                                    *
 *      Nothing                                                               *
 *****************************************************************************/
void check_selected_tree_item(struct directory_contents* contents) {
    for (int i = 0; i < contents->number_files; i++) {
        // Protect against switching away from a file that needs to be saved first
        if (bu_state.is_dirty && bu_state.dirty_path != NULL && contents->files[i].selected == nk_true && strcmp(contents->files[i].path, bu_state.dirty_path) != 0) {
            if (!save_confirm_dialog_active) {
                // Deselect this file since it is not the one that needs to be saved
                contents->files[i].selected = nk_false;

                // Open the save confirm dialog that will stop the user before they lose changes accidentally
                printf("You need to save the current file before switching to another.\n");
                save_confirm_dialog_active = true;
            }
            return;
        }

        // Check to see if an asterisk should be added to show that a file is dirty
        if (contents->files[i].selected == nk_true && bu_state.is_dirty && contents->files[i].name[strlen(contents->files[i].name) - 1] != '*') {
            // Append the asterisk to the file name
            append_char_to_string(contents->files[i].name, '*');
            bu_state.dirty_path = selected_path;
        }
        else if (contents->files[i].selected == nk_true && !bu_state.is_dirty && contents->files[i].name[strlen(contents->files[i].name) - 1] == '*') {
            // If the file is not dirty, make sure it does not keep its asterisk
            contents->files[i].name[strlen(contents->files[i].name) - 1] = '\0';
        }

        // If the item was previously selected, deselect it
        if (contents->files[i].selected == nk_true && contents->files[i].prev_selected == nk_false) {
            printf("%s is selected.\n", contents->files[i].path);

            // Clear the HTML preview for the next render
            clear_html_preview();

            // Save this as the currently selected path
            selected_path = contents->files[i].path;

            // Load the contents of the selected file into the markdown editor
            if (string_ends_with(contents->files[i].name, ".md") || string_ends_with(contents->files[i].name, ".yaml")) {
                // Open the documentation file and make sure that the file opened properly
                FILE* doc_file;
                doc_file = fopen(contents->files[i].path, "r");
                if (doc_file == NULL) {
                    set_error_popup("There was an error opening the file\nthat you selected.");
                }
                else {
                    // Clear the previous contents of the markdown editor
                    clear_editor();

                    // Read all of the lines from the file
                    char line_temp[1000] = {'\0'};
                    while(fgets(line_temp, sizeof(line_temp), doc_file) != NULL) {
                        // Add the line read from the file to the markdown editor
                        nk_textedit_text(&tedit_state, line_temp, strlen(line_temp));

                        // Save the current text length as the previous so the file will not be marked as dirty
                        bu_state.prev_markdown_len = tedit_state.string.len;
                    }
                }

                // Make sure to close the file
                fclose(doc_file);

                // If a markdown file was just opened, render the HTML preview
                if (string_ends_with(contents->files[i].name, ".md")) {
                    update_html_preview();
                }
            }

            // Deselect all other tree items
            deselect_entire_tree();

            // Reselect just this one entry
            contents->files[i].selected = nk_true;
        }

        // Save the the current state to use it again next frame
        contents->files[i].prev_selected = contents->files[i].selected;
    }
}

/******************************************************************************
 * ui_do -- Responsible for creating the BuildUp Editor UI each frame.        *
 *                                                                            *
 * Parameters                                                                 *
 *      ctx -- The Nuklear context struct                                     *
 *      window_width -- int representing the current width of the window      *
 *      window_height -- int representing the current height of the window    *
 *      running -- int flag that allows this loop to be exited from outside   *
 *                                                                            *
 * Returns                                                                    *
 *      Nothing                                                               *
 *****************************************************************************/
void ui_do(struct nk_context* ctx, int window_width, int window_height, int* running) {
    if (nk_begin(ctx, "Main Window", nk_rect(0, 0, window_width, window_height),
        NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR))
    {
        // Application menu
        nk_menubar_begin(ctx);
        nk_layout_row_begin(ctx, NK_STATIC, 25, 4);
        nk_layout_row_push(ctx, 45);
        if (nk_menu_begin_label(ctx, "FILE", NK_TEXT_LEFT, nk_vec2(120, 200))) {
            // Single column layout
            nk_layout_row_dynamic(ctx, 30, 1);

            // Button to create a new project
            if (nk_menu_item_label(ctx, "NEW PROJECT", NK_TEXT_LEFT)) {
                printf("Creating new project...\n");
            }

            // Button to open an existing project
            if (nk_menu_item_label(ctx, "OPEN PROJECT", NK_TEXT_LEFT)) {
                // Open the popup later in this frame
                show_open_project = nk_true;
            }

            // Button to save the current file and update the preview
            if (nk_menu_item_label(ctx, "SAVE", NK_TEXT_LEFT)) {
                // Save the markdown file that is open
                save_selected_file();

                // Update the HTML preivew
                update_html_preview();
            }

            // Button to export the project
            if (nk_menu_item_label(ctx, "EXPORT", NK_TEXT_LEFT)) {
                // If there is nothing to export, let the user know
                if (selected_path == NULL) {
                    set_error_popup("You must first open a project to use the\nexport feature.");
                }
                else {
                    // Extract the base path
                    char* base_path = strdup(selected_path);
                    cut_string_last(base_path, PATH_SEP[0]);

                    // Extract the file name without the extension
                    char* export_file_base = strrchr(strdup(selected_path), PATH_SEP[0]);
                    export_file_base++;
                    cut_string_last(export_file_base, '.');

                    // Export happens to the _site directory within the project root directory
                    char* site_dir = "_site";
                    char* extension = ".html";

                    // Saves the parent path to see if we need to create the _site directory
                    char* site_path = malloc(sizeof(base_path) + sizeof(PATH_SEP) + sizeof(site_dir) + 1);
                    site_path[0] = '\0';
                    strcat(site_path, base_path);
                    strcat(site_path, PATH_SEP);
                    strcat(site_path, site_dir);

                    // Try to create the directory and see if it succeeds
                    int res = create_dir(site_path);
                    if (res != 0)
                        set_error_popup("There was an error creating the\n_site directory.");

                    // Create a string to hold all the parts of the path
                    char* export_path = malloc(sizeof(base_path) + sizeof(PATH_SEP) + sizeof(site_dir) + sizeof(PATH_SEP) + sizeof(export_file_base) + sizeof(extension) + 1);
                    export_path[0] = '\0';
                    strcat(export_path, site_path);
                    strcat(export_path, PATH_SEP);
                    strcat(export_path, export_file_base);
                    strcat(export_path, extension);

                    // Write the contents of the HTML preview to the file
                    FILE* html_file;
                    html_file = fopen(export_path, "w");
                    if (html_file == NULL) {
                        strcat(step_link_insert_msg, "Unable to write to an HTML file.");
                    }
                    else {
                        fputs(html_preview, html_file);
                    }
                    fclose(html_file);

                    printf("%s\n", export_path);
                }
            }

            // Button to close the app
            if (nk_menu_item_label(ctx, "CLOSE", NK_TEXT_LEFT)) {
                clipboard_free(cb);
                *running = 0;
            }

            nk_menu_end(ctx);
        }

        // Edit menu
        if (nk_menu_begin_label(ctx, "EDIT", NK_TEXT_LEFT, nk_vec2(120, 200))) {
            // Single column layout
            nk_layout_row_dynamic(ctx, 30, 1);

            // The undo feature for the markdown editor
            if (nk_menu_item_label(ctx, "UNDO", NK_TEXT_LEFT)) {
                printf("Undoing last edit...\n");
            }

            // The redo feature for the markdown editor
            if (nk_menu_item_label(ctx, "REDO", NK_TEXT_LEFT)) {
                printf("Redoing last edit...\n");
            }

            // The copy feature for the markdown editor
            if (nk_menu_item_label(ctx, "CUT", NK_TEXT_LEFT)) {
                cut_copy_to_clipboard();
                nk_textedit_delete_selection(&tedit_state);
            }

            // The copy feature for the markdown editor
            if (nk_menu_item_label(ctx, "COPY", NK_TEXT_LEFT)) {
                cut_copy_to_clipboard();
            }

            // The paste feature for the markdown editor
            if (nk_menu_item_label(ctx, "PASTE", NK_TEXT_LEFT)) {
                paste_from_clipboard();
            }

            // Opens the settings dialog for the user
            if (nk_menu_item_label(ctx, "SETTINGS", NK_TEXT_LEFT)) {
                printf("Opening the settings dialog...\n");
            }

            nk_menu_end(ctx);
        }

        // Insert menu
        if (nk_menu_begin_label(ctx, "INSERT", NK_TEXT_LEFT, nk_vec2(120, 200))) {
            // Single column layout
            nk_layout_row_dynamic(ctx, 30, 1);

            // For inserting the Bill of Materials
            if (nk_menu_item_label(ctx, "BOM", NK_TEXT_LEFT)) {
                printf("Inserting a bill of materials entry...\n");
            }

            // For inserting a step link to another page
            if (nk_menu_item_label(ctx, "STEP LINK", NK_TEXT_LEFT)) {
                step_link_page_dialog_active = true;
            }

            // For inserting an image
            if (nk_menu_item_label(ctx, "IMAGE", NK_TEXT_LEFT)) {
                image_link_dialog_active = true;
            }

            nk_menu_end(ctx);
        }

        // Help menu
        if (nk_menu_begin_label(ctx, "HELP", NK_TEXT_LEFT, nk_vec2(120, 200))) {
            // Single column layout
            nk_layout_row_dynamic(ctx, 30, 1);

            // Opens the About dialog with information about the app and supporters
            if (nk_menu_item_label(ctx, "ABOUT", NK_TEXT_LEFT)) {
                about_dialog_active = true;
            }

            nk_menu_end(ctx);
        }

        // Three column layout
        nk_layout_row_begin(ctx, NK_DYNAMIC, window_height, 3);

        // Project tree structure
        nk_layout_row_push(ctx, 0.2f);
        if (nk_group_begin(ctx, "Project", NK_WINDOW_BORDER)) {
            // Add the directory contents to the project tree
            if (nk_tree_push(ctx, NK_TREE_NODE, "Project", NK_MAXIMIZED)) {
                // See if there are any directories to add to the tree
                if (contents.number_directories > 0) {
                    for (int i = 0; i < contents.number_directories; i++) {
                        if (nk_tree_element_push_id(ctx, NK_TREE_NODE, contents.dirs[i]->name, NK_MINIMIZED, &contents.dirs[i]->selected, i)) {
                            // Add this directory's dir contents
                            for (int j = 0; j < contents.dirs[i]->number_directories; j++) {
                                if (nk_tree_element_push_id(ctx, NK_TREE_NODE, contents.dirs[i]->dirs[j]->name, NK_MINIMIZED, &contents.dirs[i]->dirs[j]->selected, i+j)) {
                                    // Add any subdirectories to the tree
                                    for (int k = 0; k < contents.dirs[i]->dirs[j]->number_directories; k++) {
                                        if (nk_tree_element_push_id(ctx, NK_TREE_NODE, contents.dirs[i]->dirs[j]->dirs[k]->name, NK_MINIMIZED, &contents.dirs[i]->dirs[j]->dirs[k]->selected, i+j+k)) {
                                            // Add this directory's dir contents
                                            for (int l = 0; l < contents.dirs[i]->dirs[j]->dirs[k]->number_directories; l++) {
                                                if (nk_tree_element_push_id(ctx, NK_TREE_NODE, contents.dirs[i]->dirs[j]->dirs[k]->dirs[l]->name, NK_MINIMIZED, &contents.dirs[i]->dirs[j]->dirs[k]->dirs[l]->selected, i+j+k+l)) {
                                                    // Add any subdirectories to the tree
                                                    for (int m = 0; m < contents.dirs[i]->dirs[j]->dirs[k]->dirs[l]->number_directories; m++) {
                                                        if (nk_tree_element_push_id(ctx, NK_TREE_NODE, contents.dirs[i]->dirs[j]->dirs[k]->dirs[l]->dirs[m]->name, NK_MINIMIZED, &contents.dirs[i]->dirs[j]->dirs[k]->dirs[l]->dirs[m]->selected, i+j+k+l+m)) {
                                                            // Add only this bottom subdirectory's files to the tree
                                                            for (int n = 0; n < contents.dirs[i]->dirs[j]->dirs[k]->dirs[l]->dirs[m]->number_files; n++) {
                                                                nk_selectable_label(ctx, contents.dirs[i]->dirs[j]->dirs[k]->dirs[l]->dirs[m]->files[n].name, NK_TEXT_LEFT, &contents.dirs[i]->dirs[j]->dirs[k]->dirs[l]->dirs[m]->files[n].selected);
                                                            }
                                                            nk_tree_element_pop(ctx);
                                                        }
                                                    }

                                                    // Add this directory's files
                                                    for (int m = 0; m < contents.dirs[i]->dirs[j]->dirs[k]->dirs[l]->number_files; m++) {
                                                        nk_selectable_label(ctx, contents.dirs[i]->dirs[j]->dirs[k]->dirs[l]->files[m].name, NK_TEXT_LEFT, &contents.dirs[i]->dirs[j]->dirs[k]->dirs[l]->files[m].selected);
                                                    }
                                                    nk_tree_element_pop(ctx);
                                                }
                                            }

                                            // Add this directory's files
                                            for (int l = 0; l < contents.dirs[i]->dirs[j]->dirs[k]->number_files; l++) {
                                                nk_selectable_label(ctx, contents.dirs[i]->dirs[j]->dirs[k]->files[l].name, NK_TEXT_LEFT, &contents.dirs[i]->dirs[j]->dirs[k]->files[l].selected);
                                            }
                                            nk_tree_element_pop(ctx);
                                        }
                                    }

                                    // Add this subdirectory's file contents
                                    for (int k = 0; k < contents.dirs[i]->dirs[j]->number_files; k++) {
                                        nk_selectable_label(ctx, contents.dirs[i]->dirs[j]->files[k].name, NK_TEXT_LEFT, &contents.dirs[i]->dirs[j]->files[k].selected);
                                    }
                                    nk_tree_element_pop(ctx);
                                }
                            }
                            // Add this directory's file contents
                            for (int j = 0; j < contents.dirs[i]->number_files; j++) {
                                nk_selectable_label(ctx, contents.dirs[i]->files[j].name, NK_TEXT_LEFT, &contents.dirs[i]->files[j].selected);
                            }

                            nk_tree_element_pop(ctx);
                        }
                    }
                }
                // See if there are any files to add to the tree
                if (contents.number_files > 0) {
                    for (int i = 0; i < contents.number_files; i++) {
                        nk_selectable_label(ctx, contents.files[i].name, NK_TEXT_LEFT, &contents.files[i].selected);
                    }
                }

                // Work out which tree item, if any, has been selected
                check_selected_tree_item(&contents);

                // Check any level 1 directories to see if their files are selected
                for (int i = 0; i < contents.number_directories; i++) {
                    check_selected_tree_item(contents.dirs[i]);

                    // Check any level 2 directories to see if their files are selected
                    for (int j = 0; j < contents.dirs[i]->number_directories; j++) {
                        check_selected_tree_item(contents.dirs[i]->dirs[j]);

                        // Check any level 3 directories to see if their files are selected
                        for (int k = 0; k < contents.dirs[i]->dirs[j]->number_directories; k++) {
                            check_selected_tree_item(contents.dirs[i]->dirs[j]->dirs[k]);

                            // Check any level 4 directories to see if their files are selected
                            for (int l = 0; l < contents.dirs[i]->dirs[j]->dirs[k]->number_directories; l++) {
                                check_selected_tree_item(contents.dirs[i]->dirs[j]->dirs[k]->dirs[l]);

                                // Check any level 5 directories to see if their files are selected
                                for (int m = 0; m < contents.dirs[i]->dirs[j]->dirs[k]->dirs[l]->number_directories; m++) {
                                    check_selected_tree_item(contents.dirs[i]->dirs[j]->dirs[k]->dirs[l]->dirs[m]);
                                }
                            }
                        }
                    }
                }

                nk_tree_pop(ctx);
            }

            nk_group_end(ctx);
        }

        // BuildUp markdown editor text field
        nk_layout_row_push(ctx, 0.4f);
        tedit_state.single_line = 0;
        nk_edit_buffer(ctx, NK_EDIT_FIELD|NK_EDIT_MULTILINE|NK_EDIT_CLIPBOARD, &tedit_state, nk_filter_default);

        // Output HTML
        nk_layout_row_push(ctx, 0.4f);
        if (html_preview != NULL)
            nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD|NK_EDIT_MULTILINE|NK_EDIT_CLIPBOARD, html_preview, strlen(html_preview) + 1, nk_filter_default);

        // Check to see if the text has changed
        if (tedit_state.string.len != bu_state.prev_markdown_len) {
            // Save the previous state
            bu_state.is_dirty = true;
            bu_state.prev_markdown_len = tedit_state.string.len;
        }

        // Show and handle the Open Project dialog
        if (show_open_project) {
            // Compute the position of the popup to put it in the center of the window
            bounds.x = (window_width / 2) - (300 / 2);
            bounds.y = (window_height / 2) - (190 / 2);
            bounds.w = 300;
            bounds.h = 190;

            // The open project dialog that allows the user to set the project directory path
            if (nk_popup_begin(ctx, NK_POPUP_DYNAMIC, "Open Project", NK_WINDOW_CLOSABLE, bounds))
            {
                // The path label
                nk_layout_row_dynamic(ctx, 20, 1);
                nk_label(ctx, "Project Directory:", NK_TEXT_LEFT);

                // The path entry box
                nk_layout_row_dynamic(ctx, 25, 1);
                nk_edit_string_zero_terminated(ctx, NK_EDIT_BOX, file_path, sizeof(file_path), nk_filter_default);

                // OK button allows user to submit the path information
                nk_layout_row_dynamic(ctx, 25, 3);
                if (nk_button_label(ctx, "OK")) {
                    show_open_project = nk_false;
                    nk_popup_close(ctx);

                    // Get the sorted contents at the specified path
                    contents = list_project_dir(file_path);

                    // Clear the markdown editor of the previous contents
                    clear_editor();

                    // Reset the HTML preview text for the new conversion text
                    clear_html_preview();

                    // If the user gave an invalid directory, let them know
                    if (contents.error == does_not_exist) {
                        set_error_popup("The directory you selected does not exist.\nPlease try to open another directory.");

                        // printf("The directory you selected does not exist.\nPlease try to open another directory.\n");
                    }
                    else if (contents.error == not_a_buildup_directory) {
                        set_error_popup("This directory either does not exist, or\ndoes not appear to be a valid BuildUp\ndirectory. Please try to open another\ndirectory.");

                        // printf("This directory either does not exist, or does not appear to be a valid BuildUp directory.\nPlease try to open another directory.\n");
                    }
                    else if (contents.error == dir_structure_too_deep) {
                        set_error_popup("The directory structure of the project is\ndeeper than 5 levels, and the listing\nwill be truncated.");

                        // printf("The directory struucture of the project is deeper than 5 levels, and the listing will be truncated.\n");
                    }
                    else if (contents.error == general_error) {
                        set_error_popup("A general error occurred when opening a\nproject directory. Please make sure that you\nhave permissions to read the directory.");

                        // printf("A general error occurred when opening a project directory. Please make sure that you have permissions to read the directory.\n");
                    }
                    else if (contents.error == exceeded_max_dirs) {
                        set_error_popup("There are too many directories on at least\none level of your project documentation\nstructure. Your listing is likely\ntruncated.");
                    }
                    else if (contents.error == exceeded_max_files) {
                        set_error_popup("There are too many files on at least\none level of your project documentation\nstructure. Your listing is likely\ntruncated.");
                    }
                    else if (contents.number_files <= 0) {
                        set_error_popup("No project files were found, please try\nto open another directory.");

                        // printf("No project files were found, please try to open another directory.\n");
                    }
                }

                // Cancel button allows the user to skip submitting the path information
                if (nk_button_label(ctx, "Cancel")) {
                    show_open_project = nk_false;
                    nk_popup_close(ctx);
                }
                nk_popup_end(ctx);
            }
            else {
                show_open_project = nk_false;

                // Reset the file path string to prevent any old text frorm being reused
                file_path[0] = '\0';
            }
        }
    }

    /*
     * Handle the error popup dialog.
     */
    if (error_popup_active) {
        // The position and size of the popup
        struct nk_rect s = {(window_width / 2) - (290 / 2), (window_height / 2) - (150 / 2), 290, 150};

        // Show a popup with the Error title
        if (nk_popup_begin(ctx, NK_POPUP_STATIC, "Error", NK_WINDOW_TITLE, s))
        {
            // Displays the error message
            nk_layout_row_dynamic(ctx, 70, 1);
            nk_edit_string_zero_terminated(ctx, NK_EDIT_READ_ONLY|NK_EDIT_MULTILINE, error_popup_message, sizeof(error_popup_message), nk_filter_default);

            // Displays the OK button to the user
            nk_layout_row_dynamic(ctx, 25, 1);
            if (nk_button_label(ctx, "OK")) {
                error_popup_active = false;
                nk_popup_close(ctx);
            }
            nk_popup_end(ctx);
        }
        else {
            error_popup_active = false;
        }
    }

    /*
     * Handle the save confirmation popup dialog.
     */
    if (save_confirm_dialog_active) {
        // THe position and size of the popup
        struct nk_rect s = {(window_width / 2) - (275 / 2), (window_height / 2) - (150 / 2), 275, 150};

        // Construct the popup
        if (nk_popup_begin(ctx, NK_POPUP_STATIC, "Confirm", NK_WINDOW_TITLE, s)) {
            // Displays the confirmation message
            nk_layout_row_dynamic(ctx, 70, 1);
            const char* message = "Save changes before opening another file?\0";
            nk_label(ctx, message, NK_TEXT_LEFT);

            // The Yes button to save the file before opening another one, and the No button to proceed without saving
            nk_layout_row_dynamic(ctx, 25, 2);
            if (nk_button_label(ctx, "YES")) {
                // Save the markdown file that is open
                save_selected_file();

                // Close the dialog
                save_confirm_dialog_active = false;
                nk_popup_close(ctx);
            }
            if (nk_button_label(ctx, "NO")) {
                // Mark the open file as not being dirty, even if it still is
                bu_state.is_dirty = false;
                bu_state.dirty_path = NULL;

                save_confirm_dialog_active = false;
                nk_popup_close(ctx);
            }
            nk_popup_end(ctx);
        }
        else {
            save_confirm_dialog_active = false;
        }
    }

    /*
     * Handle the About popup dialog.
     */
    if (about_dialog_active) {
        // The position and size of the popup
        struct nk_rect s = {(window_width / 2) - (300 / 2), (window_height / 2) - (200 / 2), 300, 200};

        // Construct the popup
        if (nk_popup_begin(ctx, NK_POPUP_STATIC, "About", NK_WINDOW_TITLE, s)) {
            // Displays the About message
            nk_layout_row_dynamic(ctx, 120, 1);
            char message[1000];
            message[0] = '\0';
            strcat(message, "BuildUp Editor\nA tool for documenting hardware projects\nusing markdown.\nVersion: 0.1-alpha\nSupporters:\nFerdinand, adam-james, Patrick Wagstrom,\nmarked23, Jordan Poles, Anonymous\0");
            nk_edit_string_zero_terminated(ctx, NK_EDIT_READ_ONLY|NK_EDIT_MULTILINE, message, sizeof(message), nk_filter_default);

            // Displays the OK button to the user
            nk_layout_row_dynamic(ctx, 25, 1);
            if (nk_button_label(ctx, "OK")) {
                about_dialog_active = false;
                nk_popup_close(ctx);
            }
            nk_popup_end(ctx);
        }
        else {
            about_dialog_active = false;
        }
    }

    /*
     * Handle the step link page popup dialog.
     */
    if (step_link_page_dialog_active) {
        // The position and size of the popup
        struct nk_rect s = {(window_width / 2) - (300 / 2), (window_height / 2) - (290 / 2), 300, 290};

        // Construct the popup
        if (nk_popup_begin(ctx, NK_POPUP_STATIC, "Insert Step Link", NK_WINDOW_TITLE, s)) {
            // Displayys the description
            nk_layout_row_dynamic(ctx, 60, 1);
            char message[1000];
            message[0] = '\0';
            strcat(message, "Links to another file so that it will be\nincluded as a step. Setting 'Link Text' to '.' will use\nthe title of the page being linked to.");
            nk_edit_string_zero_terminated(ctx, NK_EDIT_READ_ONLY|NK_EDIT_MULTILINE, message, sizeof(message), nk_filter_default);

            // The link text field to be shown in the HTML
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, "Link Text", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_edit_string_zero_terminated(ctx, NK_EDIT_BOX|NK_TEXT_EDIT_SINGLE_LINE, step_link_link_text, sizeof(step_link_link_text), nk_filter_default);

            // The field for the linked-to file
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, "Page To Link To", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_edit_string_zero_terminated(ctx, NK_EDIT_BOX|NK_TEXT_EDIT_SINGLE_LINE, step_link_link_file, sizeof(step_link_link_file), nk_filter_default);

            // Error message field
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, step_link_insert_msg, NK_TEXT_LEFT);

            // Displays the OK button to the user
            nk_layout_row_dynamic(ctx, 25, 2);
            if (nk_button_label(ctx, "Insert")) {
                // Assemble the BuildUp tag
                char step_link_tag[2000];
                step_link_tag[0] = '\0';
                strcat(step_link_tag, "[");
                strcat(step_link_tag, step_link_link_text);
                strcat(step_link_tag, "](");
                strcat(step_link_tag, step_link_link_file);
                strcat(step_link_tag, "){step}");

                // Assemble the full path to the markdown file
                char* full_path = malloc(sizeof(file_path) + sizeof(step_link_link_file) + 2);
                full_path[0] = '\0';
                strcat(full_path, file_path);
                strcat(full_path, PATH_SEP);
                strcat(full_path, step_link_link_file);

                // Make sure that the file exists
                FILE* md_file;
                md_file = fopen(full_path, "r");
                if (md_file == NULL) {
                    strcat(step_link_insert_msg, "The markdown file you entered does not exist.");
                }
                else {
                    // Make sure any previous error messages are cleared
                    step_link_insert_msg[0] = '\0';

                    // Check to make sure there is a main title in the first few lines of the linked-to file
                    if (strlen(step_link_link_text) == 1 && step_link_link_text[0] == '.') {
                        char* line;
                        char line_temp[1000];
                        line = fgets(line_temp, sizeof(line_temp), md_file);

                        // If we got something, check to make sure that the line starts with a single hash
                        bool found_title = false;
                        while (line != NULL) {
                            if (line[0] == '#') {
                                found_title = true;
                                break;
                            }

                            // Check to see if the next line has the hash mark in it
                            line = fgets(line_temp, sizeof(line_temp), md_file);
                        }

                        // If we did not find a title in the file, alert the user
                        if (!found_title) {
                            strcat(step_link_insert_msg, "The linked markdown file has no title (#).");
                        }
                    }

                    // Close the markdown file if it was able to be opened
                    fclose(md_file);

                    // If there is an error message, we do not want to close this dialog
                    if (step_link_insert_msg[0] == '\0') {
                        // Insert the assembled tag at the current cursor location in the editor
                        nk_textedit_text(&tedit_state, step_link_tag, strlen(step_link_tag));

                        // Re-render the HTML preview
                        update_html_preview();

                        // Close the dialog
                        step_link_page_dialog_active = false;
                        nk_popup_close(ctx);
                    }
                }

                // Let the system know that the file is dirty now
                bu_state.dirty_path = selected_path;
                bu_state.is_dirty = true;
            }
            if (nk_button_label(ctx, "Cancel")) {
                // Make sure any previous error messages are cleared
                step_link_insert_msg[0] = '\0';

                // Do the work of closing the dialog
                step_link_page_dialog_active = false;
                nk_popup_close(ctx);
            }

            nk_popup_end(ctx);
        }
        else {
            step_link_page_dialog_active = false;
        }
    }

    // The image insert dialog
    if (image_link_dialog_active) {
        // The position and size of the popup
        struct nk_rect s = {(window_width / 2) - (300 / 2), (window_height / 2) - (310 / 2), 300, 310};

        // Construct the popup
        if (nk_popup_begin(ctx, NK_POPUP_STATIC, "Image Insert", NK_WINDOW_TITLE, s)) {
            // Displayys the description
            nk_layout_row_dynamic(ctx, 70, 1);
            char message[1000];
            message[0] = '\0';
            strcat(message, "Inserts an image into the document. Any\nimages that are in another location on disk\nwill be copied to the local images\ndirectory.");
            nk_edit_string_zero_terminated(ctx, NK_EDIT_READ_ONLY|NK_EDIT_MULTILINE, message, sizeof(message), nk_filter_default);

            // The image alt text field to be shown in the HTML
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, "Alt Text", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_edit_string_zero_terminated(ctx, NK_EDIT_BOX|NK_TEXT_EDIT_SINGLE_LINE, image_alt_text, sizeof(image_alt_text), nk_filter_default);

            // The field for the inserted file name
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, "Image to Insert", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_edit_string_zero_terminated(ctx, NK_EDIT_BOX|NK_TEXT_EDIT_SINGLE_LINE, image_path, sizeof(image_path), nk_filter_default);

            // Error message field
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, insert_image_msg, NK_TEXT_LEFT);

            // Displays the OK/insert button to the user
            nk_layout_row_dynamic(ctx, 25, 2);
            if (nk_button_label(ctx, "Insert")) {
                // Make sure any previous error messages are cleared
                insert_image_msg[0] = '\0';

                // Make sure that the user has opened a project before doing an insert
                if (strlen(file_path) <= 3 || (string_ends_with(selected_path, ".md") && string_ends_with(selected_path, ".yaml"))) {
                    strcat(insert_image_msg, "Open a markdown or yaml file to insert.");
                }

                // Holds the full path to the image
                char* full_path = malloc(sizeof(file_path) + sizeof(image_path) + 2);

                // Check to see if the user has provided a full path to the image
                if (image_path[1] != ':' || image_path[0] != '/') {
                    // Assemble the full path to the markdown file
                    full_path[0] = '\0';
                    strcat(full_path, file_path);
                    strcat(full_path, PATH_SEP);
                    strcat(full_path, image_path);
                }
                else {
                    full_path = image_path;
                }

                // Make sure that the file exists
                FILE* md_file;
                md_file = fopen(full_path, "r");
                if (md_file == NULL) {
                    strcat(insert_image_msg, "The selected image file does not exist.");
                }
                else {
                    // Check to make sure that the user has entered data into the required fields
                    if (image_alt_text[0] == '\0') {
                        strcat(insert_image_msg, "Please enter alternate text.");
                    }
                    else if (image_path[0] == '\0') {
                        strcat(insert_image_msg, "Please enter an image file name.");
                    }

                    // If there is an error message, we do not want to close this dialog
                    if (insert_image_msg[0] == '\0') {
                        // Assemble the BuildUp tag
                        char image_tag[2000];
                        image_tag[0] = '\0';
                        strcat(image_tag, "![");
                        strcat(image_tag, image_alt_text);
                        strcat(image_tag, "](");
                        strcat(image_tag, image_path);
                        strcat(image_tag, ")");

                        // Insert the assembled tag at the current cursor location in the editor
                        nk_textedit_text(&tedit_state, image_tag, strlen(image_tag));

                        // Re-render the HTML preview
                        update_html_preview();

                        // Close the dialog
                        image_link_dialog_active = false;
                        nk_popup_close(ctx);
                    }
                }
            }

            // The Cancel button
            if (nk_button_label(ctx, "Cancel")) {
                // Make sure any previous error messages are cleared
                insert_image_msg[0] = '\0';

                // Do the work of closing the dialog
                nk_popup_close(ctx);
                image_link_dialog_active = false;
            }

            nk_popup_end(ctx);
        }
        else {
            image_link_dialog_active = false;
        }
    }

    nk_end(ctx);
}
