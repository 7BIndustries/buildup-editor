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

/*# define INCLUDE_STYLE*/
#ifdef INCLUDE_STYLE
/*set_style(ctx, THEME_WHITE);*/
/*set_style(ctx, THEME_RED);*/
/*set_style(ctx, THEME_BLUE);*/
/*set_style(ctx, THEME_DARK);*/
#endif

int i;  /* Loop counter variable */
char file_path[256];  /* Holds the selected file/folder path */
char buffer[256];  /* Holds the BuildUp markdown text entered by the user */
char html_preview[256];  /* Converted HTML text based on the markdown */
struct directory_contents contents;  /* Listed directory contents */

/* md4c flags for converting markdown to HTML */
static unsigned parser_flags = 0;
static unsigned renderer_flags = MD_HTML_FLAG_DEBUG;

/* Popup dialog control flags */
static int show_open_project = nk_false;

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
    strncat(html_preview, text, size);
}


/*
 * Allows a check to make sure that a few needed project files are present
 * in the directory selected by the user.
 */
static bool check_for_buildup_files(struct directory_contents contents) {
    bool result = false;
    bool buildconf_found = false;

    /* Check for a few of the typical files */
    for (i = 0; i <= contents.listing_length; i++) {
        /* Check to see if the current name matches the buildconf.yaml file */
        if (strcmp(contents.dir_contents[i], "buildconf.yaml") == 0) buildconf_found = true;
    }

    /* Make sure everything was found that is required */
    result = buildconf_found;

    return result;
}

int ret;  /* The return code for the markdown to HTML conversion */
struct nk_rect bounds;  /* The bounds of the popup dialog */

/*
 * Responsible for creating the BuildUp Editor UI each frame.
 */
void ui_do(struct nk_context* ctx, int window_width, int window_height, int* running) {
    struct membuffer buf_out = {0};
    membuf_init(&buf_out, (MD_SIZE)(sizeof(buffer) + sizeof(buffer)/8 + 64));

    if (nk_begin(ctx, "Main Window", nk_rect(0, 0, window_width, window_height),
        NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR))
    {
        /* Application menu */
        nk_menubar_begin(ctx);
        nk_layout_row_begin(ctx, NK_STATIC, 25, 2);
        nk_layout_row_push(ctx, 45);
        if (nk_menu_begin_label(ctx, "FILE", NK_TEXT_LEFT, nk_vec2(120, 200))) {
            /* Single column layout */
            nk_layout_row_dynamic(ctx, 30, 1);

            /* Button to create a new project */
            if (nk_menu_item_label(ctx, "NEW PROJECT", NK_TEXT_LEFT)) {
                printf("Creating new project...\n");
            }

            /* Button to open an existing project */
            if (nk_menu_item_label(ctx, "OPEN PROJECT", NK_TEXT_LEFT)) {
                printf("Opening existing project...\n");
                show_open_project = nk_true;
            }

            /* Button to save the current file and update the preview */
            if (nk_menu_item_label(ctx, "SAVE", NK_TEXT_LEFT)) {
                /* Reset the HTML preview text for the new conversion text */
                html_preview[0] = '\0';

                /* Convert the markdown to HTML */
                ret = md_html(buffer, (MD_SIZE)strlen(buffer), process_output, (void*) &buf_out, parser_flags, renderer_flags);
                if (ret == -1) {
                    printf("The markdown failed to parse.\n");
                }
            }

            /* Button to export the project */
            if (nk_menu_item_label(ctx, "EXPORT", NK_TEXT_LEFT)) {
                printf("Exporting project...\n");
            }

            /* Button to close the app */
            if (nk_menu_item_label(ctx, "CLOSE", NK_TEXT_LEFT)) {
                *running = 0;
            }
            nk_menu_end(ctx);
        }

        /* Three column layout */
        /*nk_layout_row_dynamic(ctx, window_height, 1);*/
        nk_layout_row_begin(ctx, NK_DYNAMIC, window_height, 3);

        /* Project tree structure */
        nk_layout_row_push(ctx, 0.2f);
        if (nk_group_begin(ctx, "Project", NK_WINDOW_BORDER)) {
            if (nk_tree_push(ctx, NK_TREE_NODE, "Project", NK_MAXIMIZED)) {
                if(nk_tree_push(ctx, NK_TREE_NODE, "images", NK_MINIMIZED)) {
                    nk_label(ctx, " finished.png", NK_TEXT_LEFT);
                    nk_tree_pop(ctx);
                }
                if(nk_tree_push(ctx, NK_TREE_NODE, "manufacture", NK_MINIMIZED)) {
                    nk_label(ctx, " printing.md", NK_TEXT_LEFT);
                    nk_label(ctx, " post_processing.md", NK_TEXT_LEFT);
                    nk_tree_pop(ctx);
                }
                if(nk_tree_push(ctx, NK_TREE_NODE, "assembly", NK_MINIMIZED)) {
                    nk_label(ctx, " subassembly_a.md", NK_TEXT_LEFT);
                    nk_label(ctx, " full_assembly.md", NK_TEXT_LEFT);
                    nk_tree_pop(ctx);
                }
                nk_label(ctx, " index.md", NK_TEXT_LEFT);
                nk_label(ctx, " buildconf.yaml", NK_TEXT_LEFT);
                nk_label(ctx, " parts.yaml", NK_TEXT_LEFT);
                nk_label(ctx, " tools.yaml", NK_TEXT_LEFT);
                nk_tree_pop(ctx);
            }

            nk_group_end(ctx);
        }

        /* BuildUp markdown editor text field*/
        nk_layout_row_push(ctx, 0.4f);
        nk_edit_string_zero_terminated(ctx, NK_EDIT_BOX|NK_EDIT_AUTO_SELECT|NK_EDIT_MULTILINE, buffer, sizeof(buffer), nk_filter_ascii);

        /* Output HTML */
        nk_layout_row_push(ctx, 0.4f);
        nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD|NK_EDIT_MULTILINE, html_preview, strlen(html_preview) + 1, nk_filter_ascii);

        /* Show and handle the Open Project dialog */
        if (show_open_project) {
            /* Compute the position of the popup to put it in the center of the window */
            bounds.x = (window_width / 2) - (300 / 2);
            bounds.y = (window_height / 2) - (190 / 2);
            bounds.w = 300;
            bounds.h = 190;

            /* The open project dialog that allows the user to set the project directory path */
            if (nk_popup_begin(ctx, NK_POPUP_DYNAMIC, "Open Project", NK_WINDOW_CLOSABLE, bounds))
            {
                /* The path label */
                nk_layout_row_dynamic(ctx, 20, 1);
                nk_label(ctx, "Project Directory:", NK_TEXT_LEFT);

                /* The path entry box */
                nk_layout_row_dynamic(ctx, 25, 1);
                nk_edit_string_zero_terminated(ctx, NK_EDIT_BOX, file_path, sizeof(file_path), nk_filter_ascii);

                /* OK button allows user to submit the path information */
                nk_layout_row_dynamic(ctx, 25, 3);
                if (nk_button_label(ctx, "OK")) {
                    show_open_project = nk_false;
                    /* Collect the user's given path contents so that we can work with them */
                    contents = list_dir_contents(file_path);

                    /* If the user gave an invalid directory, let them know */
                    if (contents.listing_length == -2) {
                        printf("The directory you selected does not exist.\nPlease try to open another directory.\n");
                    }
                    else if (contents.listing_length <= 0) {
                        printf("No project files were found, please try to open another directory.\n");
                    }
                    else if (!check_for_buildup_files(contents)) {
                        printf("This directory does not appear to be a valid BuildUp directory.\nPlease try to open another directory.\n");
                    }
                    else {
                        /* List the file directory */
                        for (i = 0; i <= contents.listing_length; i++) {
                            printf("File: %s\n", contents.dir_contents[i]);
                        }
                    }

                    nk_popup_close(ctx);
                }

                /* Cancel button allows the user to skip submitting the path information */
                if (nk_button_label(ctx, "Cancel")) {
                    show_open_project = nk_false;
                    nk_popup_close(ctx);
                }
                nk_popup_end(ctx);
            }
            else {
                show_open_project = nk_false;

                /* Reset the file path string to prevent any old text frorm being reused */
                file_path[0] = '\0';
            }
        }
    }
    nk_end(ctx);
}
