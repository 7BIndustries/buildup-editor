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

// #define INCLUDE_STYLE
// #ifdef INCLUDE_STYLE
//set_style(ctx, THEME_WHITE);
// set_style(ctx, THEME_RED);
//set_style(ctx, THEME_BLUE);
//set_style(ctx, THEME_DARK);
// #endif

// Constants that hold mostly array lengths
#define ERROR_MSG_MAX_LENGTH 1000

char file_path[1000];  // Holds the selected file/folder path
char html_preview[1000];  // Converted HTML text based on the markdowns
struct directory_contents contents;  // Listed directory contents
int ret;  // The return code for the markdown to HTML conversions
struct nk_rect bounds;  // The bounds of the popup dialog
bool error_popup_active = false;  // Tracks whether or not the error popup should be displayed
char error_popup_message[ERROR_MSG_MAX_LENGTH];  // The message that will be displayed in the error popup
struct nk_text_edit tedit_state;  // The struct that holds the state of the BuildUp text editor

// md4c flags for converting markdown to HTML
static unsigned parser_flags = 0;
static unsigned renderer_flags = MD_HTML_FLAG_DEBUG;

// Popup dialog control flags
static int show_open_project = nk_false;

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
    if (userdata == NULL) printf("No user data passed for markdown processor.\n");

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
        // If the item was previously selected, deselect it
        if (contents->files[i].selected == nk_true && contents->files[i].prev_selected == nk_false) {
            printf("%s is selected.\n", contents->files[i].path);

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
    // Placeholder for user data that can be passed around
    struct md_userdata userdata = {.name="Temp"};

    if (nk_begin(ctx, "Main Window", nk_rect(0, 0, window_width, window_height),
        NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR))
    {
        // Application menu
        nk_menubar_begin(ctx);
        nk_layout_row_begin(ctx, NK_STATIC, 25, 2);
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
                // Reset the HTML preview text for the new conversion text
                html_preview[0] = '\0';

                // Convert the markdown to HTML
                ret = md_html(tedit_state.string.buffer.memory.ptr, (MD_SIZE)tedit_state.string.len, process_output, (void*) &userdata, parser_flags, renderer_flags);
                if (ret == -1) {
                    printf("The markdown failed to parse.\n");
                }
            }

            // Button to export the project
            if (nk_menu_item_label(ctx, "EXPORT", NK_TEXT_LEFT)) {
                printf("Exporting project...\n");
            }

            // Button to close the app
            if (nk_menu_item_label(ctx, "CLOSE", NK_TEXT_LEFT)) {
                *running = 0;
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
        nk_edit_buffer(ctx, NK_EDIT_FIELD|NK_EDIT_AUTO_SELECT|NK_EDIT_MULTILINE, &tedit_state, nk_filter_default);

        // Output HTML
        nk_layout_row_push(ctx, 0.4f);
        nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD|NK_EDIT_MULTILINE, html_preview, strlen(html_preview) + 1, nk_filter_ascii);

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
                nk_edit_string_zero_terminated(ctx, NK_EDIT_BOX, file_path, sizeof(file_path), nk_filter_ascii);

                // OK button allows user to submit the path information
                nk_layout_row_dynamic(ctx, 25, 3);
                if (nk_button_label(ctx, "OK")) {
                    show_open_project = nk_false;
                    nk_popup_close(ctx);

                    // Get the sorted contents at the specified path
                    contents = list_project_dir(file_path);

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

    // Handle the error popup
    if (error_popup_active) {
        // The position and size of the popup
        struct nk_rect s = {(window_width / 2) - (290 / 2), (window_height / 2) - (150 / 2), 290, 150};

        // Show a popup with the Error title
        if (nk_popup_begin(ctx, NK_POPUP_STATIC, "Error", NK_WINDOW_TITLE, s))
        {
            // Displays the error message
            nk_layout_row_dynamic(ctx, 70, 1);
            nk_edit_string_zero_terminated(ctx, NK_EDIT_READ_ONLY|NK_EDIT_MULTILINE, error_popup_message, sizeof(error_popup_message), nk_filter_ascii);

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

    nk_end(ctx);
}
