/******************************************************************************
 * buildup-editor -- Provides a GUI for developing BuildUp Markdown and       * 
 *                   exporting it to HTML for use by end users.               *
 *                                                                            *
 * Author: 7B Industries                                                      *
 * License: Apache 2.0                                                        *
 *                                                                            *
 * Purpose: To allow open hardware developers to document their creations.    *
 *                                                                            *
 * Usage:                                                                     *
 *      Run the binary without options to launch a GUI containing controls to *
 *      enter, convert and export your BuildUp documentation.                 *
 * ***************************************************************************/
/*#include <assert.h>*/
#include <stdio.h>
/*#include <stdlib.h>*/
/*#include <stdarg.h>*/
#include <string.h>
/*#include <limits.h>*/
/*#include <math.h>*/
/*#include <unistd.h>*/

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_IMPLEMENTATION
#define NK_XLIB_IMPLEMENTATION
#include "external/nuklear.h"
#include "external/nuklear_xlib.h"
#include "external/md4c.h"
#include "external/md4c-html.h"

#include "lib/bue_util.h"
#include "lib/bue_ui.h"

#define DTIME           20
#define WINDOW_WIDTH    800
#define WINDOW_HEIGHT   600

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

long dt;  /* Difference between the start of a timer and the current time */
long started;  /* Milliseconds when the timer was started */
int running = 1;  /* Setting to zero will shut down the app */
XWindow xw;  /* Struct representing an XLib window */
struct nk_context *ctx;  /* Nuklear UI context struct */

static void die(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputs("\n", stderr);
    exit(EXIT_FAILURE);
}

/*
 * Handles some initialization functions of the Nuklear based UI.
 */
struct nk_context* ui_init(struct XWindow xw) {
    struct nk_context *ctx;

    /* GUI */
    xw.font = nk_xfont_create(xw.dpy, "fixed");
    ctx = nk_xlib_init(xw.font, xw.dpy, xw.screen, xw.win, xw.width, xw.height);

    return ctx;
}

int main(void)
{
    /* X11 */
    memset(&xw, 0, sizeof xw);
    xw.dpy = XOpenDisplay(NULL);
    if (!xw.dpy) die("Could not open a display; perhaps $DISPLAY is not set?");
    xw.root = DefaultRootWindow(xw.dpy);
    xw.screen = XDefaultScreen(xw.dpy);
    xw.vis = XDefaultVisual(xw.dpy, xw.screen);
    xw.cmap = XCreateColormap(xw.dpy,xw.root,xw.vis,AllocNone);

    xw.swa.colormap = xw.cmap;
    xw.swa.event_mask =
        ExposureMask | KeyPressMask | KeyReleaseMask |
        ButtonPress | ButtonReleaseMask| ButtonMotionMask |
        Button1MotionMask | Button3MotionMask | Button4MotionMask | Button5MotionMask|
        PointerMotionMask | KeymapStateMask;
    xw.win = XCreateWindow(xw.dpy, xw.root, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0,
        XDefaultDepth(xw.dpy, xw.screen), InputOutput,
        xw.vis, CWEventMask | CWColormap, &xw.swa);

    XStoreName(xw.dpy, xw.win, "BuildUp Editor");
    XMapWindow(xw.dpy, xw.win);
    xw.wm_delete_window = XInternAtom(xw.dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(xw.dpy, xw.win, &xw.wm_delete_window, 1);
    XGetWindowAttributes(xw.dpy, xw.win, &xw.attr);
    xw.width = (unsigned int)xw.attr.width;
    xw.height = (unsigned int)xw.attr.height;

    /* Initialize the Nuklear GUI */
    ctx = ui_init(xw);

    while (running)
    {
        /* Need the current size of the XWindow to scale the Nuklear window to it */
        XGetWindowAttributes(xw.dpy, xw.win, &xw.attr);
        xw.width = (unsigned int)xw.attr.width;
        xw.height = (unsigned int)xw.attr.height;

        /* Input */
        XEvent evt;
        started = timestamp();
        nk_input_begin(ctx);
        while (XPending(xw.dpy)) {
            XNextEvent(xw.dpy, &evt);
            if (evt.type == ClientMessage) goto cleanup;
            if (XFilterEvent(&evt, xw.win)) continue;
            nk_xlib_handle_event(xw.dpy, xw.screen, xw.win, &evt);
        }
        nk_input_end(ctx);

        /* Render the Nuklear UI */
        ui_do(ctx, xw.attr.width, xw.attr.height, &running);

        /* Draw */
        XClearWindow(xw.dpy, xw.win);
        nk_xlib_render(xw.win, nk_rgb(30,30,30));
        XFlush(xw.dpy);

        /* Timing */
        dt = timestamp() - started;
        if (dt < DTIME)
            sleep_for(DTIME - dt);
    }

cleanup:
    nk_xfont_del(xw.dpy, xw.font);
    nk_xlib_shutdown();
    XUnmapWindow(xw.dpy, xw.win);
    XFreeColormap(xw.dpy, xw.cmap);
    XDestroyWindow(xw.dpy, xw.win);
    XCloseDisplay(xw.dpy);
    return 0;
}
