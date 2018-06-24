#ifndef __WM_SHELL_H
#define __WM_SHELL_H

#include <wayland-server.h>

struct wm_shell {
  void* shell;
  struct wm_server* server;

  struct wl_listener shell_surface;
  struct wl_list link;
};

#endif
