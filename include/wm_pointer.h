#ifndef __WM_POINTER_H
#define __WM_POINTER_H

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
  struct wl_listener button;
};

#endif
