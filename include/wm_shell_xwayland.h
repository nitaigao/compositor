#ifndef __WM_SHELL_XWAYLAND_H
#define __WM_SHELL_XWAYLAND_H

#include <wayland-server.h>

struct wm_shell;
struct wm_server;

struct wm_shell* wm_shell_xwayland_create(struct wm_server *server);

#endif
