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

int i;  // Loop counter variable
char file_path[256];  // Holds the selected file/folder path
char buffer[256];  // Holds the BuildUp markdown text entered by the user
char html_preview[256];  // Converted HTML text based on the markdowns
struct directory_contents contents;  // Listed directory contents
int ret;  // The return code for the markdown to HTML conversions
struct nk_rect bounds;  // The bounds of the popup dialog
/* The disadvantage of these two arrays is that it adds state tracking to the
   Nuklear tree, but it meets the UX goals of this UI */
int selected[255];  // The selected states of the project tree items
int prev_selected[255];  // The previously selected states of the tree items

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
 * Buffer struct that the converted HTML text is stored in.
 * Needed by md4c.
 */
struct membuffer {
    char* data;
    size_t asize;
    size_t size;
};

/*
 * Used to initialize the buffer that the converted HTML is stored in.
 * Needed by md4c.
 */
static void membuf_init(struct membuffer* buf, MD_SIZE new_asize)
{
    buf->size = 0;
    buf->asize = new_asize;
    buf->data = malloc(buf->asize);
    if(buf->data == NULL) {
        fprintf(stderr, "membuf_init: malloc() failed.\n");
        exit(1);
    }
}

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

    // Initialize the array that stores the selected states of tree items
    for (int i = 0; i < 255; i++) {
        selected[i] = nk_false;
        prev_selected[i] = nk_false;
    }

    return ctx;
}

/*
 * Responsible for creating the BuildUp Editor UI each frame.
 */
void ui_do(struct nk_context* ctx, int window_width, int window_height, int* running) {
    struct membuffer buf_out = {0};
    membuf_init(&buf_out, (MD_SIZE)(sizeof(buffer) + sizeof(buffer)/8 + 64));

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
                ret = md_html(buffer, (MD_SIZE)strlen(buffer), process_output, (void*) &buf_out, parser_flags, renderer_flags);
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
                    for (i = 0; i < contents.number_directories; i++) {
                        if (nk_tree_element_push(ctx, NK_TREE_NODE, contents.dirs[i]->name, NK_MINIMIZED, &contents.dirs[i]->selected)) {
                            // Add this directory's dir contents
                            for (int j = 0; j < contents.dirs[i]->number_directories; j++) {
                                if (nk_tree_element_push(ctx, NK_TREE_NODE, contents.dirs[i]->dirs[j]->name, NK_MINIMIZED, &contents.dirs[i]->dirs[j]->selected)) {
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
                    for (i = 0; i < contents.number_files; i++) {
                        nk_selectable_label(ctx, contents.files[i].name, NK_TEXT_LEFT, &contents.files[i].selected);
                    }
                }

                // Work out which tree item, if any, has been selected
                /*for (i = 0; i <= contents.listing_length; i++) {
                    // If the item was previously selected, deselect it
                    if (selected[i] == nk_true && prev_selected[i] == nk_false) {
                        printf("%s is selected.\n", contents.dir_contents[i]);

                        // Deselect all other tree items
                        for (int j = 0; j <= contents.listing_length; j++) {
                            // Make sure we are not deseleting the newly selected item
                            if (j == i)
                                continue;

                            // Deselect the current item
                            selected[j] = nk_false;
                        }
                    }

                    // Save the the current state to use it again next frame
                    prev_selected[i] = selected[i];
                }*/

                nk_tree_pop(ctx);
            }

            nk_group_end(ctx);
        }

        // BuildUp markdown editor text field
        nk_layout_row_push(ctx, 0.4f);
        nk_edit_string_zero_terminated(ctx, NK_EDIT_BOX|NK_EDIT_AUTO_SELECT|NK_EDIT_MULTILINE, buffer, sizeof(buffer), nk_filter_ascii);

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
                    // Get the sorted contents at the specified path
                    contents = list_project_dir(file_path);

                    // If the user gave an invalid directory, let them know
                    if (contents.number_directories == -2) {
                        printf("The directory you selected does not exist.\nPlease try to open another directory.\n");
                    }
                    else if (contents.number_directories == -3) {
                        printf("This directory does not appear to be a valid BuildUp directory.\nPlease try to open another directory.\n");
                    }
                    else if (contents.number_directories <= 0) {
                        printf("No project files were found, please try to open another directory.\n");
                    }

                    nk_popup_close(ctx);
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
    nk_end(ctx);
}
