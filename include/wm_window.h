#ifndef __WM_WINDOW_H
#define __WM_WINDOW_H

#include <wayland-server.h>

struct wm_pointer;

struct wm_window {
  int x;
  int y;

  int width;
  int height;

  int pending_x;
  int pending_y;

  int pending_width;
  int pending_height;

  bool update_x;
  bool update_y;

  struct wm_surface *surface;
  struct wl_list link;
};

void wm_window_focus(struct wm_window* window);

bool wm_window_intersects_point(struct wm_window* window, int x, int y);

void wm_window_commit_pending_movement(struct wm_window* window, int width, int height);

void wm_window_resize(struct wm_window* window, struct wm_pointer* pointer);

void wm_window_move(struct wm_window* window, struct wm_pointer* pointer);

#endif
