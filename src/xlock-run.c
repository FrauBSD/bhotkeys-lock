/*
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Run xlock under a chosen WM_CLASS instance. Lock Screen
 * (xlock-screen) stays opaque, above a black underlay, so a
 * random saver cannot leak the desktop. Lock Desktop
 * (xlock-desktop) leaves the session visible through the saver.
 */

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static Display *dpy;
static const char *instance_name;
static Atom net_wm_opacity;
static Window backdrop_win;
static int screen_num;
static int want_backdrop;
static int backdrop_stacked;

static int
xlock_xerror(Display *display __unused, XErrorEvent *event)
{
	if (event->error_code == BadWindow || event->error_code == BadDrawable)
		return (0);
	return (1);
}

static void
apply_window(Window win)
{
	XClassHint hint;

	hint.res_name = (char *)instance_name;
	hint.res_class = (char *)"xlock";
	XSetClassHint(dpy, win, &hint);

	if (strcmp(instance_name, "xlock-screen") == 0) {
		unsigned long opacity = 0xffffffffU;

		XChangeProperty(dpy, win, net_wm_opacity, XA_CARDINAL, 32,
		    PropModeReplace, (unsigned char *)&opacity, 1);
	} else
		XDeleteProperty(dpy, win, net_wm_opacity);

	XFlush(dpy);
}

static int
window_valid(Window win)
{
	XWindowAttributes attrs;

	return (win != None && XGetWindowAttributes(dpy, win, &attrs));
}

static int
is_xlock_window(Window win)
{
	XClassHint hint;
	int match = 0;

	if (!window_valid(win))
		return (0);

	if (!XGetClassHint(dpy, win, &hint))
		return (0);

	if (hint.res_class != NULL &&
	    (strcasecmp(hint.res_class, "xlock") == 0 ||
	    strcasecmp(hint.res_class, "XLock") == 0))
		match = 1;
	else if (hint.res_name != NULL &&
	    (strcasecmp(hint.res_name, "xlock") == 0 ||
	    strcasecmp(hint.res_name, instance_name) == 0))
		match = 1;

	if (hint.res_name != NULL)
		XFree(hint.res_name);
	if (hint.res_class != NULL)
		XFree(hint.res_class);

	return (match);
}

static int
tag_xlock_tree(Window win)
{
	Window root, parent, *children;
	unsigned int nchildren;
	int tagged = 0;

	if (!window_valid(win))
		return (0);

	if (is_xlock_window(win)) {
		apply_window(win);
		tagged = 1;
	}

	if (XQueryTree(dpy, win, &root, &parent, &children, &nchildren) == 0)
		return (tagged);

	for (unsigned int i = 0; i < nchildren; i++) {
		if (tag_xlock_tree(children[i]))
			tagged = 1;
	}
	if (children != NULL)
		XFree(children);

	return (tagged);
}

static Window
find_main_xlock(Window win)
{
	Window root, parent, *children;
	unsigned int nchildren;
	Window found = None, candidate = None;

	if (!window_valid(win))
		return (None);

	if (is_xlock_window(win)) {
		XWindowAttributes attrs;

		if (XGetWindowAttributes(dpy, win, &attrs) &&
		    attrs.map_state == IsViewable &&
		    attrs.width > 64 && attrs.height > 64)
			candidate = win;
	}

	if (XQueryTree(dpy, win, &root, &parent, &children, &nchildren) == 0)
		return (candidate);

	for (unsigned int i = 0; i < nchildren; i++) {
		found = find_main_xlock(children[i]);
		if (found != None)
			break;
	}
	if (children != NULL)
		XFree(children);

	return (found != None ? found : candidate);
}

static void
raise_xlock_tree(Window win)
{
	Window root, parent, *children;
	unsigned int nchildren;

	if (!window_valid(win))
		return;

	if (is_xlock_window(win))
		XRaiseWindow(dpy, win);

	if (XQueryTree(dpy, win, &root, &parent, &children, &nchildren) == 0)
		return;

	for (unsigned int i = 0; i < nchildren; i++)
		raise_xlock_tree(children[i]);
	if (children != NULL)
		XFree(children);
}

static void
disable_backdrop_input(void)
{
	Region region;

	region = XCreateRegion();
	XShapeCombineRegion(dpy, backdrop_win, ShapeInput, 0, 0, region,
	    ShapeSet);
	XDestroyRegion(region);
}

static void
purge_stale_backdrops(Window win)
{
	Window root, parent, *children;
	unsigned int nchildren;
	XClassHint hint;

	if (!window_valid(win))
		return;

	if (XGetClassHint(dpy, win, &hint)) {
		if (hint.res_name != NULL &&
		    strcmp(hint.res_name, "xlock-backdrop") == 0) {
			XDestroyWindow(dpy, win);
			if (hint.res_name != NULL)
				XFree(hint.res_name);
			if (hint.res_class != NULL)
				XFree(hint.res_class);
			return;
		}
		if (hint.res_name != NULL)
			XFree(hint.res_name);
		if (hint.res_class != NULL)
			XFree(hint.res_class);
	}

	if (XQueryTree(dpy, win, &root, &parent, &children, &nchildren) == 0)
		return;

	for (unsigned int i = 0; i < nchildren; i++)
		purge_stale_backdrops(children[i]);
	if (children != NULL)
		XFree(children);
}

static void
create_backdrop(void)
{
	XSetWindowAttributes attrs;
	XClassHint hint;
	Window root;
	int w, h;

	if (backdrop_win != None)
		return;

	root = RootWindow(dpy, screen_num);
	w = DisplayWidth(dpy, screen_num);
	h = DisplayHeight(dpy, screen_num);

	attrs.override_redirect = True;
	attrs.background_pixel = BlackPixel(dpy, screen_num);
	attrs.border_pixel = BlackPixel(dpy, screen_num);
	attrs.event_mask = ExposureMask;
	attrs.colormap = DefaultColormap(dpy, screen_num);

	backdrop_win = XCreateWindow(dpy, root, 0, 0, w, h, 0,
	    DefaultDepth(dpy, screen_num), InputOutput,
	    DefaultVisual(dpy, screen_num),
	    CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask |
	    CWColormap, &attrs);
	hint.res_name = (char *)"xlock-backdrop";
	hint.res_class = (char *)"xlock-backdrop";
	XSetClassHint(dpy, backdrop_win, &hint);
	disable_backdrop_input();
	XMapWindow(dpy, backdrop_win);
	XClearWindow(dpy, backdrop_win);
	XFlush(dpy);
}

static void
stack_backdrop_below(Window xlock_win)
{
	XWindowChanges wc;

	if (backdrop_win == None || xlock_win == None)
		return;

	/*
	 * Always re-stack. xlock -mode random can recreate its top-level
	 * window; a one-shot Below sibling then points at a dead window and
	 * the black underlay falls behind the session; Lock Screen looks
	 * "sheer" and leaks the desktop (POLA / security).
	 */
	wc.sibling = xlock_win;
	wc.stack_mode = Below;
	XConfigureWindow(dpy, backdrop_win, CWSibling | CWStackMode, &wc);
	raise_xlock_tree(xlock_win);
	XClearWindow(dpy, backdrop_win);
	XFlush(dpy);
	backdrop_stacked = 1;
}

static void
ensure_backdrop(void)
{
	XWindowAttributes attrs;
	Window root;
	int w, h;

	if (!want_backdrop)
		return;

	root = RootWindow(dpy, screen_num);
	w = DisplayWidth(dpy, screen_num);
	h = DisplayHeight(dpy, screen_num);

	if (backdrop_win != None &&
	    XGetWindowAttributes(dpy, backdrop_win, &attrs)) {
		if (attrs.width != w || attrs.height != h) {
			XMoveResizeWindow(dpy, backdrop_win, 0, 0, w, h);
			XClearWindow(dpy, backdrop_win);
		}
		return;
	}

	backdrop_win = None;
	backdrop_stacked = 0;
	create_backdrop();
}

static void
refresh_tags(void)
{
	Window root, xlock_win;

	root = RootWindow(dpy, screen_num);
	tag_xlock_tree(root);

	if (!want_backdrop)
		return;

	ensure_backdrop();
	xlock_win = find_main_xlock(root);
	if (xlock_win == None)
		return;

	stack_backdrop_below(xlock_win);
}

int
main(int argc, char **argv)
{
	pid_t pid;
	int status;
	char *xlock_path;
	char **xlock_argv;

	if (argc < 3) {
		fprintf(stderr, "usage: %s <class_i> xlock [xlock-args...]\n",
		    argv[0]);
		return (1);
	}

	instance_name = argv[1];
	xlock_path = argv[2];
	xlock_argv = argv + 2;

	dpy = XOpenDisplay(NULL);
	if (dpy == NULL) {
		fprintf(stderr, "%s: cannot open display\n", argv[0]);
		return (1);
	}
	XSetErrorHandler(xlock_xerror);

	screen_num = DefaultScreen(dpy);
	net_wm_opacity = XInternAtom(dpy, "_NET_WM_WINDOW_OPACITY", False);
	backdrop_win = None;
	backdrop_stacked = 0;
	/*
	 * Always underlay Lock Screen with a black override-redirect window.
	 * Do not gate on "is picom up yet"; a late compositor start used to
	 * leave no underlay, and sparse xlock savers then leaked the desktop.
	 */
	want_backdrop = (strcmp(instance_name, "xlock-screen") == 0);
	purge_stale_backdrops(RootWindow(dpy, screen_num));
	if (want_backdrop)
		create_backdrop();

	pid = fork();
	if (pid < 0) {
		perror("fork");
		return (1);
	}
	if (pid == 0) {
		execvp(xlock_path, xlock_argv);
		perror(xlock_path);
		_exit(127);
	}

	for (;;) {
		refresh_tags();
		if (waitpid(pid, &status, WNOHANG) != 0)
			break;
		usleep(50000);
	}

	if (backdrop_win != None) {
		XDestroyWindow(dpy, backdrop_win);
		backdrop_win = None;
	}
	XCloseDisplay(dpy);

	if (WIFEXITED(status))
		return (WEXITSTATUS(status));
	if (WIFSIGNALED(status))
		return (128 + WTERMSIG(status));
	return (1);
}
