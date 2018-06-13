#ifndef __WM_WINDOW_H
#define __WM_WINDOW_H

struct wm_window {
  double x;
  double y;

  struct wm_surface *surface;
  struct wl_list link;
};

#endif
