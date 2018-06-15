#ifndef __WM_WINDOW_H
#define __WM_WINDOW_H

#include <wayland-server.h>

struct wm_window {
  double x;
  double y;

  struct wm_surface *surface;
  struct wl_list link;
};

void window_focus(struct wm_window* window);

#endif
