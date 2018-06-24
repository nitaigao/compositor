#ifndef __WM_SHELL_XDG_H
#define __WM_SHELL_XDG_H

#include <wayland-server.h>

struct wm_shell;
struct wm_server;

struct wm_shell* wm_shell_xdg_create(struct wm_server *server);

#endif
