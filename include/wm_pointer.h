#ifndef __WM_POINTER_H
#define __WM_POINTER_H

#include <wayland-server.h>

#define WM_POINTER_MODE_FREE 0
#define WM_POINTER_MODE_MOVE 1
#define WM_POINTER_MODE_RESIZE 2

struct wm_pointer {
  int mode;

  double delta_x;
  double delta_y;
  double last_x;
  double last_y;

  struct wm_server *server;
  struct wm_seat *seat;

  struct wlr_cursor *cursor;

  struct wl_listener axis;
  struct wl_listener cursor_motion_absolute;
  struct wl_listener cursor_motion;
  struct wl_listener request_set_cursor;
  struct wl_listener button;
};

void wm_pointer_set_mode(struct wm_pointer* pointer, int mode);

void wm_pointer_set_default_cursor(struct wm_pointer* pointer);

#endif
