#ifndef __WM_WINDOW_H
#define __WM_WINDOW_H

#include <wayland-server.h>

struct wm_window {
  double x;
  double y;

  double width;
  double height;

  struct wm_surface *surface;
  struct wl_list link;
};

void wm_window_focus(struct wm_window* window);

bool wm_window_intersects_point(struct wm_window* window, int x, int y);

#endif
